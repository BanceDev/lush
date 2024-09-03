/*
Copyright (c) 2024, Lance Borden
All rights reserved.

This software is licensed under the BSD 3-Clause License.
You may obtain a copy of the license at:
https://opensource.org/licenses/BSD-3-Clause

Redistribution and use in source and binary forms, with or without
modification, are permitted under the conditions stated in the BSD 3-Clause
License.

THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTIES,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
*/

#include "lua_api.h"
#include "lush.h"
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdio.h>
#include <stdlib.h>

// -- script execution --
void lua_load_script(lua_State *L, const char *script) {
	if (luaL_dofile(L, script) != LUA_OK) {
		printf("[C] Error reading script\n");
		luaL_error(L, "Error: %s\n", lua_tostring(L, -1));
	}
}

// -- C funtions --
static void execute_command(lua_State *L, const char *line) {
	int status = 0;
	char **commands = lush_split_pipes((char *)line);
	char ***args = lush_split_args(commands, &status);
	if (status == -1) {
		fprintf(stderr, "lush: Expected end of quoted string\n");
	} else if (lush_run(L, args, status) == 0) {
		exit(1);
	}

	for (int i = 0; args[i]; i++) {
		free(args[i]);
	}
	free(args);
	free(commands);
}

// -- Lua wrappers --
static int l_execute_command(lua_State *L) {
	const char *command = luaL_checkstring(L, 1);
	execute_command(L, command);
	return 0;
}

// -- register Lua functions --

void lua_register_api(lua_State *L) {
	lua_register(L, "exec", l_execute_command);
}
