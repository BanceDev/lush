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

#define MAX_SIGNALS 64

lua_State *trap_L;

char *traps[MAX_SIGNALS];

const char *sig_strs[] = {
	"SIGHUP",	   "SIGINT",	  "SIGQUIT",	 "SIGILL",		"SIGTRAP",
	"SIGABRT",	   "SIGBUS",	  "SIGFPE",		 "SIGKILL",		"SIGUSR1",
	"SIGSEGV",	   "SIGUSR2",	  "SIGPIPE",	 "SIGALRM",		"SIGTERM",
	"SIGSTKFLT",   "SIGCHLD",	  "SIGCONT",	 "SIGSTOP",		"SIGTSTP",
	"SIGTTIN",	   "SIGTTOU",	  "SIGURG",		 "SIGXCPU",		"SIGXFSZ",
	"SIGVTALRM",   "SIGPROF",	  "SIGWINCH",	 "SIGIO",		"SIGPWR",
	"SIGSYS",	   NULL,		  NULL,			 "SIGRTMIN",	"SIGRTMIN+1",
	"SIGRTMIN+2",  "SIGRTMIN+3",  "SIGRTMIN+4",	 "SIGRTMIN+5",	"SIGRTMIN+6",
	"SIGRTMIN+7",  "SIGRTMIN+8",  "SIGRTMIN+9",	 "SIGRTMIN+10", "SIGRTMIN+11",
	"SIGRTMIN+12", "SIGRTMIN+13", "SIGRTMIN+14", "SIGRTMIN+15", "SIGRTMAX-14",
	"SIGRTMAX-13", "SIGRTMAX-12", "SIGRTMAX-11", "SIGRTMAX-10", "SIGRTMAX-9",
	"SIGRTMAX-8",  "SIGRTMAX-7",  "SIGRTMAX-6",	 "SIGRTMAX-5",	"SIGRTMAX-4",
	"SIGRTMAX-3",  "SIGRTMAX-2",  "SIGRTMAX-1",	 "SIGRTMAX"};

static int trap_exec(const char *line) {
	int status = 0;
	lush_push_history(line);
	char *expanded_line = lush_resolve_aliases((char *)line);
	char **commands = lush_split_commands(expanded_line);
	char ***args = lush_split_args(commands, &status);

	if (status == -1) {
		fprintf(stderr, "lush: Expected end of quoted string\n");
	} else if (lush_run(trap_L, args, status) != 0) {
		return -1;
	}

	for (int i = 0; args[i]; i++) {
		free(args[i]);
	}
	free(args);
	free(commands);
	return 0;
}

void trap_handler(int signum) {
	if (traps[signum - 1]) {
		trap_exec(traps[signum - 1]);
	}
}

char *builtin_strs[] = {"cd", "help", "exit", "time", "trap"};
char *builtin_usage[] = {"[dirname]", "", "", "[pipeline]",
						 "[-lp] [[command] signal]"};

int (*builtin_func[])(lua_State *, char ***) = {
	&lush_cd, &lush_help, &lush_exit, &lush_time, &lush_trap, &lush_lua};

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
		   "time to reference this list.\n\n");
	printf("Available commands:\n");
	for (int i = 0; i < lush_num_builtins(); i++) {
		printf("- %s %s\n", builtin_strs[i], builtin_usage[i]);
	}

	char *api_strs[] = {"exec(string command)",
						"getcwd()",
						"debug(boolean isOn)",
						"cd(string path)",
						"exists(string path)",
						"isFile(string path)",
						"isDir(string path)",
						"isReadable(string path)",
						"isWriteable(string path)",
						"lastHistory()",
						"getHistory(int index)",
						"getenv(string envar)",
						"setenv(string envar, string val)",
						"unsetenv(string envar)",
						"setPrompt(string prompt)",
						"alias(string alias, string command)",
						"termCols()",
						"termRows()",
						"glob(string extension)",
						"exit()"};
	char *api_usage[] = {
		"executes the command line chain given",
		"gets current working directory",
		"sets debug mode",
		"changed current working directory to path given",
		"checks if a file/directory exists at path",
		"checks if given path is a file",
		"checks if given path is a directory",
		"checks if given path is readable",
		"checks if given path is writeable",
		"returns last history element",
		"returns history at an index, 1 is most recent",
		"returns value of an environment variable",
		"sets the value of an environment variable",
		"unsets the value of an environment variable",
		"sets the prompt for the shell",
		"sets an alias for a command",
		"returns present number of columns in terminal",
		"returns present number of rows in terminal",
		"returns an array of filenames that have a given extension",
		"ends the current process erroneously"};
	printf("\nLunar Shell Lua API:\n\n");
	for (int i = 0; i < sizeof(api_strs) / sizeof(char *); i++) {
		printf("\033[1;32mlush.%s: \033[0m%s\n", api_strs[i], api_usage[i]);
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

int lush_trap(lua_State *L, char ***args) {
	args[0]++;
	// check for -lp
	if (args[0][0][0] == '-') {
		if (strchr(args[0][0], 'l')) {
			// list all the signals
			printf("1) SIGHUP	 2) SIGINT	 3) "
				   "SIGQUIT	 4) SIGILL	 5) SIGTRAP\n"
				   "6) SIGABRT	 7) SIGBUS	 8) "
				   "SIGFPE	 9) SIGKILL	10) SIGUSR1\n"
				   "11) SIGSEGV	12) SIGUSR2	13) "
				   "SIGPIPE	14) SIGALRM	15) "
				   "SIGTERM\n"
				   "16) SIGSTKFLT	17) SIGCHLD	18) "
				   "SIGCONT	19) SIGSTOP	20) "
				   "SIGTSTP\n"
				   "21) SIGTTIN	22) SIGTTOU	23) "
				   "SIGURG	24) SIGXCPU	25) "
				   "SIGXFSZ\n"
				   "26) SIGVTALRM	27) SIGPROF	28) "
				   "SIGWINCH	29) SIGIO	30) "
				   "SIGPWR\n"
				   "31) SIGSYS	34) SIGRTMIN	35) "
				   "SIGRTMIN+1	36) SIGRTMIN+2	"
				   "37) SIGRTMIN+3\n"
				   "38) SIGRTMIN+4	39) SIGRTMIN+5	40) "
				   "SIGRTMIN+6	41) "
				   "SIGRTMIN+7	42) SIGRTMIN+8\n"
				   "43) SIGRTMIN+9	44) SIGRTMIN+10	45) "
				   "SIGRTMIN+11	46) "
				   "SIGRTMIN+12	47) SIGRTMIN+13\n"
				   "48) SIGRTMIN+14	49) SIGRTMIN+15	50) "
				   "SIGRTMAX-14	51) "
				   "SIGRTMAX-13	52) SIGRTMAX-12\n"
				   "53) SIGRTMAX-11	54) SIGRTMAX-10	55) "
				   "SIGRTMAX-9	56) "
				   "SIGRTMAX-8	57) SIGRTMAX-7\n"
				   "58) SIGRTMAX-6	59) SIGRTMAX-5	60) "
				   "SIGRTMAX-4	61) "
				   "SIGRTMAX-3	62) SIGRTMAX-2\n"
				   "63) SIGRTMAX-1	64) SIGRTMAX\n");
			args[0]--;
			return 0;
		}
		if (strchr(args[0][0], 'p')) {
			for (int i = 0; i < MAX_SIGNALS; i++) {
				if (traps[i] != NULL) {
					printf("trap -- '%s' %s\n", traps[i], sig_strs[i]);
				}
			}
			args[0]--;
			return 0;
		}
		if (strlen(args[0][0]) == 1) {
			// check for unbinding
			for (int i = 0; i < MAX_SIGNALS; i++) {
				if (sig_strs[i] && strcmp(args[0][1], sig_strs[i]) == 0) {
					if (i == 1) {
						// SIGINT is a special case for parent process
						signal(i + 1, SIG_IGN);
					} else {
						signal(i + 1, SIG_DFL);
					}
					break;
				}
			}
			args[0]--;
			return 0;
		}
	}

	if (args[0][0] && args[0][1]) {
		for (int i = 0; i < MAX_SIGNALS; i++) {
			if (sig_strs[i] && strcmp(args[0][1], sig_strs[i]) == 0) {
				traps[i] = strdup(args[0][0]);
				trap_L = L; // idk if I like this but doing it for now
				signal(i + 1, trap_handler);
				break;
			}
		}
	}

	args[0]--;
	return 0;
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
