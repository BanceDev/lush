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
#include "history.h"
#include "lush.h"
#include <dirent.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

// globals
static bool debug_mode = false;

// -- script execution --
void lua_load_script(lua_State *L, const char *script, char **args) {
	char script_path[512];
	// check if script is in the current directory
	if (access(script, F_OK) == 0) {
		snprintf(script_path, sizeof(script_path), "%s", script);
	} else {
		const char *home_dir = getenv("HOME");
		if (home_dir != NULL) {
			snprintf(script_path, sizeof(script_path), "%s/.lush/scripts/%s",
					 home_dir, script);

			if (access(script_path, F_OK) != 0) {
				// script not in either location
				fprintf(stderr, "[C] Script not found: %s\n", script);
				return;
			}
		} else {
			// HOME not set
			fprintf(stderr, "[C] HOME directory is not set.\n");
			return;
		}
	}
	// add args global if args were passed
	if (args != NULL && args[0] != NULL) {
		lua_newtable(L);
		for (int i = 0; args[i]; i++) {
			lua_pushstring(L, args[i]);
			// i + 1 since Lua is 1 based indexed
			lua_rawseti(L, -2, i + 1);
		}
		lua_setglobal(L, "args");
	}
	// if we got here the file exists
	if (luaL_loadfile(L, script_path) == LUA_OK) {
		if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK) {
			const char *error_msg = lua_tostring(L, -1);
			fprintf(stderr, "[C] Error executing script: %s\n", error_msg);
			lua_pop(L, 1); // remove error from stack
		}
	} else {
		const char *error_msg = lua_tostring(L, -1);
		fprintf(stderr, "[C] Error loading script: %s\n", error_msg);
		lua_pop(L, 1); // remove error from stack
	}

	// reset args after running or just keep it nil
	lua_pushnil(L);
	lua_setglobal(L, "args");
}

void lua_run_init(lua_State *L) {
	char script_path[64];
	snprintf(script_path, sizeof(script_path), ".lush/%s", "init.lua");
	lua_load_script(L, script_path, NULL);
}

// -- C funtions --
static int execute_command(lua_State *L, const char *line) {
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
	lush_push_history(line);
	free(args);
	free(commands);
	return status;
}

static char *get_expanded_path(const char *check_item) {
	uid_t uid = getuid();
	struct passwd *pw = getpwuid(uid);
	if (!pw) {
		perror("retrieve home dir");
		return NULL;
	}

	if (check_item == NULL) {
		// passed nothing
		return NULL;
	}

	char path[PATH_MAX];
	char *tilda = strchr(check_item, '~');
	if (tilda) {
		strcpy(path, pw->pw_dir);
		strcat(path, tilda + 1);
	} else {
		strcpy(path, check_item);
	}
	char *exp_path = realpath(path, NULL);
	// if the path doesnt exist
	if (!exp_path) {
		return NULL;
	}

	return exp_path;
}

// -- Lua wrappers --
static int l_execute_command(lua_State *L) {
	const char *command = luaL_checkstring(L, 1);
	int status = execute_command(L, command);
	bool rc = status != -1 ? true : false;

	if (debug_mode) {
		if (rc)
			printf("Executed: %s, success\n", command);
		else
			printf("Executed: %s, failed\n", command);
	}

	lua_pushboolean(L, rc);
	return 1;
}

static int l_get_cwd(lua_State *L) {
	char *cwd = getcwd(NULL, 0);
	lua_pushstring(L, cwd);
	free(cwd);
	return 1;
}

static int l_debug(lua_State *L) {
	if (lua_isboolean(L, 1)) {
		debug_mode = lua_toboolean(L, 1);
	}
	return 0;
}

static int l_cd(lua_State *L) {
	const char *newdir = luaL_checkstring(L, 1);
	char *exp_path = get_expanded_path(newdir);
	// if the path doesnt exist
	if (exp_path == NULL) {
		lua_pushboolean(L, false);
		return 1;
	}

	if (chdir(exp_path) != 0) {
		perror("lush: cd");
		free(exp_path);
		lua_pushboolean(L, false);
	}

	lua_pushboolean(L, true);
	free(exp_path);
	return 1;
}

static int l_exists(lua_State *L) {
	const char *check_item = luaL_checkstring(L, 1);
	char *exp_path = get_expanded_path(check_item);
	// if the path doesnt exist
	if (exp_path == NULL) {
		lua_pushboolean(L, false);
		return 1;
	}
	lua_pushboolean(L, true);
	free(exp_path);
	return 1;
}

static int l_is_file(lua_State *L) {
	const char *check_item = luaL_checkstring(L, 1);
	char *exp_path = get_expanded_path(check_item);
	// if the path doesnt exist
	if (exp_path == NULL) {
		lua_pushboolean(L, false);
		return 1;
	}
	struct stat path_stat;
	stat(exp_path, &path_stat);
	bool rc = S_ISREG(path_stat.st_mode);
	lua_pushboolean(L, rc);
	free(exp_path);
	return 1;
}

static int l_is_dir(lua_State *L) {
	const char *check_item = luaL_checkstring(L, 1);
	char *exp_path = get_expanded_path(check_item);
	// if the path doesnt exist
	if (exp_path == NULL) {
		lua_pushboolean(L, false);
		return 1;
	}
	struct stat path_stat;
	stat(exp_path, &path_stat);
	bool rc = S_ISDIR(path_stat.st_mode);
	lua_pushboolean(L, rc);
	free(exp_path);
	return 1;
}

static int l_is_readable(lua_State *L) {
	const char *check_item = luaL_checkstring(L, 1);
	char *exp_path = get_expanded_path(check_item);
	// if the path doesnt exist
	if (exp_path == NULL) {
		lua_pushboolean(L, false);
		return 1;
	}
	bool rc = access(exp_path, R_OK) == 0;
	lua_pushboolean(L, rc);
	free(exp_path);
	return 1;
}

static int l_is_writeable(lua_State *L) {
	const char *check_item = luaL_checkstring(L, 1);
	char *exp_path = get_expanded_path(check_item);
	// if the path doesnt exist
	if (exp_path == NULL) {
		lua_pushboolean(L, false);
		return 1;
	}
	bool rc = access(exp_path, W_OK) == 0;
	lua_pushboolean(L, rc);
	free(exp_path);
	return 1;
}

static int l_last_history(lua_State *L) {
	char *history_element = lush_get_past_command(0);
	// remove the newline character
	history_element[strlen(history_element) - 1] = '\0';
	lua_pushstring(L, history_element);
	free(history_element);
	return 1;
}

static int l_get_history(lua_State *L) {
	int i = luaL_checkinteger(L, 1);

	// using 1 based indexing to be aligned with Lua
	if (i < 1)
		return 0;

	char *history_element = lush_get_past_command(i - 1);
	// remove the newline character
	history_element[strlen(history_element) - 1] = '\0';
	lua_pushstring(L, history_element);
	free(history_element);
	return 1;
}

static int l_get_env(lua_State *L) {
	const char *env = luaL_checkstring(L, 1);
	char *env_val = getenv(env);
	lua_pushstring(L, env_val);
	return 1;
}

static int l_set_env(lua_State *L) {
	const char *env_name = luaL_checkstring(L, 1);
	const char *env_value = luaL_checkstring(L, 2);
	setenv(env_name, env_value, 1);
	return 0;
}

static int l_unset_env(lua_State *L) {
	const char *env = luaL_checkstring(L, 1);
	unsetenv(env);
	return 0;
}

static int l_set_prompt(lua_State *L) {
	const char *format = luaL_checkstring(L, 1);
	if (format == NULL) {
		perror("string format not passed");
		return 0;
	}

	// free old prompt format if necessary
	if (prompt_format) {
		free(prompt_format);
	}

	prompt_format = malloc(strlen(format) + 1);

	if (prompt_format) {
		strcpy(prompt_format, format);
	} else {
		perror("malloc failed");
	}
	return 0;
}

static int l_alias(lua_State *L) {
	const char *alias = luaL_checkstring(L, 1);
	const char *command = luaL_checkstring(L, 2);
	lush_add_alias(alias, command);
	return 0;
}

static int l_terminal_cols(lua_State *L) {
	struct winsize w;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
		perror("ioctl");
		return 0;
	}
	lua_pushnumber(L, w.ws_col);
	return 1;
}

static int l_terminal_rows(lua_State *L) {
	struct winsize w;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
		perror("ioctl");
		return 0;
	}
	lua_pushnumber(L, w.ws_row);
	return 1;
}

static int l_glob(lua_State *L) {
	const char *ext = luaL_checkstring(L, 1);
	DIR *dir;
	struct dirent *entry;
	size_t size = 0;

	// Open the current directory
	dir = opendir(".");
	if (dir == NULL) {
		perror("Unable to open current directory");
		return 0;
	}

	// Create a Lua table for the results
	lua_newtable(L);

	// Read each directory entry
	while ((entry = readdir(dir)) != NULL) {
		const char *last_dot = strrchr(entry->d_name, '.');

		// Only process files with extensions
		if (last_dot != NULL) {
			last_dot++; // Move past dot to the extension

			// Check if the extension matches the input argument
			if (strcmp(last_dot, ext) == 0) {
				lua_pushstring(L, entry->d_name);
				lua_rawseti(L, -2, size + 1);
				size++;
			}
		}
	}

	// Close the directory
	closedir(dir);

	// Return the table with matching filenames
	return 1;
}

// -- register Lua functions --

void lua_register_api(lua_State *L) {
	// global table for api functions
	lua_newtable(L);

	lua_pushcfunction(L, l_execute_command);
	lua_setfield(L, -2, "exec");
	lua_pushcfunction(L, l_get_cwd);
	lua_setfield(L, -2, "getcwd");
	lua_pushcfunction(L, l_debug);
	lua_setfield(L, -2, "debug");
	lua_pushcfunction(L, l_cd);
	lua_setfield(L, -2, "cd");
	lua_pushcfunction(L, l_exists);
	lua_setfield(L, -2, "exists");
	lua_pushcfunction(L, l_is_file);
	lua_setfield(L, -2, "isFile");
	lua_pushcfunction(L, l_is_dir);
	lua_setfield(L, -2, "isDirectory");
	lua_pushcfunction(L, l_is_readable);
	lua_setfield(L, -2, "isReadable");
	lua_pushcfunction(L, l_is_writeable);
	lua_setfield(L, -2, "isWriteable");
	lua_pushcfunction(L, l_last_history);
	lua_setfield(L, -2, "lastHistory");
	lua_pushcfunction(L, l_get_history);
	lua_setfield(L, -2, "getHistory");
	lua_pushcfunction(L, l_get_env);
	lua_setfield(L, -2, "getenv");
	lua_pushcfunction(L, l_set_env);
	lua_setfield(L, -2, "setenv");
	lua_pushcfunction(L, l_unset_env);
	lua_setfield(L, -2, "unsetenv");
	lua_pushcfunction(L, l_set_prompt);
	lua_setfield(L, -2, "setPrompt");
	lua_pushcfunction(L, l_alias);
	lua_setfield(L, -2, "alias");
	lua_pushcfunction(L, l_terminal_cols);
	lua_setfield(L, -2, "termCols");
	lua_pushcfunction(L, l_terminal_rows);
	lua_setfield(L, -2, "termRows");
	lua_pushcfunction(L, l_glob);
	lua_setfield(L, -2, "glob");
	// set the table as global
	lua_setglobal(L, "lush");
}
