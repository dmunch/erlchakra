/*
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */
#include <stdio.h>
#include <stdarg.h>

#include <string.h>

#include "erl_nif.h"
#include <ChakraCore.h>


/* From comp.lang.c FAQ Question 17.3 */
#define Streq(s1, s2) (strcmp((s1), (s2)) == 0)

static int load(ErlNifEnv *env, void **priv, ERL_NIF_TERM load_info);
void unload(ErlNifEnv* env, void* priv_data);

static void chakra_runtime_destroy(ErlNifEnv *env, void *obj);
static void chakra_context_destroy(ErlNifEnv *env, void *obj);

static ERL_NIF_TERM create_runtime(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]);
static char get_runtime(ErlNifEnv* env, ERL_NIF_TERM argv, JsRuntimeHandle *runtime);

static ERL_NIF_TERM create_context(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]);
static char get_context(ErlNifEnv* env, ERL_NIF_TERM argv, JsContextRef *runtime);

static ERL_NIF_TERM run(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]);

static ErlNifBinary string_ref_to_binary(JsValueRef value); 
static ERL_NIF_TERM value_ref_to_eterm(ErlNifEnv* env, JsValueRef value);

  
static ErlNifResourceType* CHAKRA_RUNTIME_RESOURCE;
static ErlNifResourceType* CHAKRA_CONTEXT_RESOURCE;

static int
load(ErlNifEnv *env, void **priv, ERL_NIF_TERM load_info)
{
    ErlNifResourceFlags flags = (ErlNifResourceFlags)(ERL_NIF_RT_CREATE | ERL_NIF_RT_TAKEOVER);

    CHAKRA_RUNTIME_RESOURCE = enif_open_resource_type(
        env, NULL, "chakra_runtime_resource", &chakra_runtime_destroy,
        flags, NULL);
    
    CHAKRA_CONTEXT_RESOURCE = enif_open_resource_type(
        env, NULL, "chakra_context_resource", &chakra_context_destroy,
        flags, NULL);
    
    return 0;
}

void
unload(ErlNifEnv* env, void* priv_data)
{
}

static void
chakra_runtime_destroy(ErlNifEnv *env, void *obj)
{
    JsSetCurrentContext(JS_INVALID_REFERENCE);
    JsRuntimeHandle runtime = *((JsRuntimeHandle*)obj);
    JsDisposeRuntime(runtime);
}

static void
chakra_context_destroy(ErlNifEnv *env, void *obj)
{
    JsContextRef context= *((JsContextRef*)obj);
    JsRelease(context, NULL);
}

static ErlNifFunc nif_funcs[] =
{
    {"create_runtime", 0, create_runtime},
    {"create_context", 1, create_context},
    {"run", 3, run}
};

ERL_NIF_INIT(erlchakra, nif_funcs, &load, NULL, NULL, unload);

static ERL_NIF_TERM
create_runtime(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    JsRuntimeHandle runtime;
    JsCreateRuntime(JsRuntimeAttributeNone, NULL, &runtime);
      
    JsRuntimeHandle *handle = (JsRuntimeHandle*) \
        enif_alloc_resource(CHAKRA_RUNTIME_RESOURCE, sizeof(JsRuntimeHandle*));

    *handle = runtime;
    
    return enif_make_resource(env, handle);
}

static char get_runtime(ErlNifEnv* env, ERL_NIF_TERM argv, JsRuntimeHandle *runtime)
{
    JsRuntimeHandle *handle;

    if(!enif_get_resource(env, argv, CHAKRA_RUNTIME_RESOURCE, (void**)&handle)) {
        return 0; 
    }
    *runtime = *handle;
    return 1;
}

static ERL_NIF_TERM
create_context(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    JsRuntimeHandle runtime;

    if (argc != 1) {
        return enif_make_badarg(env);
    }

    if(!get_runtime(env, argv[0], &runtime)) {
        return enif_make_badarg(env);
    }
    
    JsContextRef context;
    JsCreateContext(runtime, &context);
    
    //need to increment the ref count so it doesn't get garbage collected
    //it's released in chakra_context_destroy 
    JsAddRef(context, NULL);    
    JsContextRef *handle = (JsContextRef*) \
        enif_alloc_resource(CHAKRA_CONTEXT_RESOURCE, sizeof(JsContextRef*));

    *handle = context;
 
    return enif_make_resource(env, handle);
}

static char get_context(ErlNifEnv* env, ERL_NIF_TERM argv, JsContextRef *context)
{
    JsContextRef *handle;

    if(!enif_get_resource(env, argv, CHAKRA_CONTEXT_RESOURCE, (void**)&handle)) {
        return 0; 
    }
    *context= *handle;
    return 1;
}

static ERL_NIF_TERM
run(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
fprintf(stderr, "run");
    JsRuntimeHandle runtime;
    JsContextRef context;
    ErlNifBinary script;

    if (argc != 3) {
fprintf(stderr, "not enogh args");
        return enif_make_badarg(env);
    }

    if(!get_runtime(env, argv[0], &runtime)) {
fprintf(stderr, "erorr runtime");
        return enif_make_badarg(env);
    }
    
    if(!get_context(env, argv[1], &context)) {
fprintf(stderr, "erorr context");
        return enif_make_badarg(env);
    }

    if(!enif_inspect_iolist_as_binary(env, argv[2], &script)) {
fprintf(stderr, "erorr script");
        return enif_make_badarg(env);
    }

    JsSetCurrentContext(context);

    JsValueRef scriptValueRef;
    JsValueRef sourceValueRef;
    JsValueRef result;
    
    JsCreateString(script.data, script.size, &scriptValueRef);
    JsCreateString("", 0, &sourceValueRef);
    JsErrorCode error = JsRun(scriptValueRef, JS_SOURCE_CONTEXT_NONE, sourceValueRef, (JsParseScriptAttributes)0x0, &result);
    fprintf(stderr, "\n\nretcode: 0x%X\n\n", error); 

    return value_ref_to_eterm(env, result);
}

static ErlNifBinary string_ref_to_binary(JsValueRef value) 
{
  ErlNifBinary binary;
  size_t written;
  int stringLength;

  JsGetStringLength(value, &stringLength);
  enif_alloc_binary(stringLength, &binary);
  JsCopyString(value, (char*) binary.data, stringLength, &written);

  return binary;
}

static ERL_NIF_TERM
value_ref_to_eterm(ErlNifEnv* env, JsValueRef value)
{
  JsValueType type;
  JsGetValueType(value, &type);

  switch(type) {
    case JsString: {
                     ErlNifBinary binary = string_ref_to_binary(value);
                     return enif_make_binary(env, &binary);
                   }
    case JsNumber: {
                     double doubleValue;
                     JsNumberToDouble(value, &doubleValue);
                     return enif_make_double(env, doubleValue);
                   }
    case JsArray: {
                     unsigned int i = 0;
                     bool hasValue;
                     JsErrorCode error;
                     JsValueRef index;

                     ERL_NIF_TERM list = enif_make_list(env, 0);
                     do 
                     {
                       JsIntToNumber(i++, &index);

                       error = JsHasIndexedProperty(value, index, &hasValue);
                       if(!hasValue)
                       {
                         continue;
                       }

                       JsValueRef indexedValue;
                       JsGetIndexedProperty(value, index, &indexedValue);

                       list = enif_make_list_cell(env, value_ref_to_eterm(env, indexedValue) , list);
                     } while(error == JsNoError && hasValue);

                     ERL_NIF_TERM reversedList;
                     //this is stupid
                     enif_make_reverse_list(env, list, &reversedList);
                     return reversedList;

                  } 
    case JsObject: {
                     JsValueRef propertyNames = JS_INVALID_REFERENCE;
                     JsGetOwnPropertyNames(value, &propertyNames);

                     unsigned int i = 0;
                     bool hasValue;
                     JsErrorCode error;
                     JsValueRef index;

                     ERL_NIF_TERM list = enif_make_list(env, 0);
                     do 
                     {
                       JsIntToNumber(i++, &index);

                       error = JsHasIndexedProperty(propertyNames, index, &hasValue);
                       if(!hasValue)
                       {
                         continue;
                       }

                       JsValueRef propName;
                       JsGetIndexedProperty(propertyNames, index, &propName);

                       JsPropertyIdRef propId;
                       JsValueRef propValue;
                       
                       ErlNifBinary binary = string_ref_to_binary(propName);
                       JsCreatePropertyId((char*) binary.data, binary.size, &propId);
                       JsGetProperty(value, propId, &propValue);
                       ERL_NIF_TERM propList = enif_make_tuple2(env,
                          enif_make_binary(env, &binary),
                          value_ref_to_eterm(env, propValue));
                     
                       list = enif_make_list_cell(env, propList, list);
                     } while(error == JsNoError && hasValue);

                     ERL_NIF_TERM reversedList;
                     //this is stupid
                     enif_make_reverse_list(env, list, &reversedList);
                     return reversedList;
                   } 
  }

  return NULL;
}


