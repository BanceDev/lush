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

#include "help.h"
#include "lua.h"
#include "lua_api.h"
#include "lush.h"
#include <asm-generic/ioctls.h>
#include <bits/time.h>
#include <dirent.h>
#include <linux/limits.h>
#include <locale.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

char *builtin_strs[] = {"cd", "help", "exit", "time"};
char *builtin_usage[] = {"[dirname]", "", "", "[pipeline]"};

int (*builtin_func[])(lua_State *, char ***) = {
	&lush_cd, &lush_help, &lush_exit, &lush_time, &lush_lua};

int lush_num_builtins() { return sizeof(builtin_strs) / sizeof(char *); }

int lush_cd(lua_State *L, char ***args) {
	uid_t uid = getuid();
	struct passwd *pw = getpwuid(uid);
	if (!pw) {
		perror("retrieve home dir");
		return 0;
	}
	if (args[0][1] == NULL) {
		if (chdir(pw->pw_dir) != 0) {
			perror("lush: cd");
		}
	} else {
		char path[PATH_MAX];
		char extended_path[PATH_MAX];
		char *tilda = strchr(args[0][1], '~');
		// first check if they want to go to old dir
		if (strcmp("-", args[0][1]) == 0) {
			strcpy(path, getenv("OLDPWD"));
		} else if (tilda) {
			strcpy(path, pw->pw_dir);
			strcat(path, tilda + 1);
		} else {
			strcpy(path, args[0][1]);
		}

		char *exp_path = realpath(path, extended_path);
		if (!exp_path) {
			perror("realpath");
			return 1;
		}

		char *cwd = getcwd(NULL, 0);
		setenv("OLDPWD", cwd, 1);
		free(cwd);

		if (chdir(exp_path) != 0) {
			perror("lush: cd");
		}
	}

	return 0;
}

int lush_help(lua_State *L, char ***args) {
	printf("%s\n", lush_get_help_text());
#ifdef LUSH_VERSION
	printf("Lunar Shell, version %s\n", LUSH_VERSION);
#endif
	printf("These shell commands are defined internally. Type 'help' at any "
		   "time to reference this list.\n");
	printf("Available commands: \n");
	for (int i = 0; i < lush_num_builtins(); i++) {
		printf("- %s %s\n", builtin_strs[i], builtin_usage[i]);
	}
	return 0;
}

int lush_exit(lua_State *L, char ***args) { exit(0); }

int lush_time(lua_State *L, char ***args) {
	// advance past time command
	args[0]++;

	// count commands
	int i = 0;
	while (args[i++]) {
		;
	}

	// get time
	struct timespec start, end;
	double elapsed_time;

	clock_gettime(CLOCK_MONOTONIC, &start);
	int rc = lush_run(L, args, i);
	clock_gettime(CLOCK_MONOTONIC, &end);

	elapsed_time = (end.tv_sec - start.tv_sec) * 1000.0 +
				   (end.tv_nsec - start.tv_nsec) / 1e6;
	printf("Time: %.3f milliseconds\n", elapsed_time);

	// return pointer back to "time" for free()
	args[0]--;
	return rc;
}

int lush_lua(lua_State *L, char ***args) {
	// run the lua file given
	const char *script = args[0][0];
	// move args forward to any command line args
	args[0]++;

	int rc = lua_load_script(L, script, args[0]);

	// return pointer back to lua file
	args[0]--;
	return rc;
}
