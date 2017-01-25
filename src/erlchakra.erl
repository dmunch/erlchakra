%
%   Licensed under the Apache License, Version 2.0 (the "License");
%   you may not use this file except in compliance with the License.
%   You may obtain a copy of the License at
%
%       http://www.apache.org/licenses/LICENSE-2.0
%
%   Unless required by applicable law or agreed to in writing, software
%   distributed under the License is distributed on an "AS IS" BASIS,
%   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
%   See the License for the specific language governing permissions and
%   limitations under the License.
%

 -module(erlchakra).

-export([
    create_runtime/0,
    create_context/1,
    run/3
    ]).

-on_load(init/0).
-include_lib("eunit/include/eunit.hrl").

init() ->
    SoName = case code:priv_dir(?MODULE) of
    {error, bad_name} ->
        case filelib:is_dir(filename:join(["..", "priv"])) of
        true ->
            filename:join(["..", "priv", "erlchakra"]);
        false ->
            filename:join(["priv", "erlchakra"])
        end;
    Dir ->
        case os:getenv("ESCRIPT") of
        "1" ->
            filename:join([filename:dirname(escript:script_name()),
                "..", "lib", "erlchakra"]);
        _ ->
            filename:join(Dir, "erlchakra")
        end
    end,
    (catch erlang:load_nif(SoName, 0)).

create_runtime() ->
    erlang:nif_error(nif_not_loaded).

create_context(Runtime) ->
    erlang:nif_error(nif_not_loaded).

run(Runtime, Context, Script) ->
    erlang:nif_error(nif_not_loaded).

-ifdef(TEST).

create_runtime_test_() -> 
	Runtime = create_runtime(),
	[{"Can create runtime", ?_assert(true)}].

create_run_test2_() -> 
	Runtime = create_runtime(),
	Context = create_context(Runtime),
	Text = run(Runtime, Context, <<"(() => { return 'Hello world!'; })();">>),
	%Text = run(Runtime, <<"'Hello world!'">>),
	[{"Can create runtime", ?_assertEqual(<<"Hello world!">>, Text)}].

js_array_to_eterm_test_() -> 
	Runtime = create_runtime(),
	Context = create_context(Runtime),
	Obj = run(Runtime, Context, <<"(() => {return [1, 2, 3];})()">>),
	[{"Can convert objects", ?_assertEqual([1.0, 2.0, 3.0], Obj)}].

js_object_to_eterm_test_() -> 
	Runtime = create_runtime(),
	Context = create_context(Runtime),
	Obj = run(Runtime, Context, <<"(() => {return {'a': 1, 'b': 2 };})()">>),
	[{"Can convert objects", ?_assertEqual([{<<"a">>, 1.0}, {<<"b">>, 2.0}], Obj)}].
-endif.
