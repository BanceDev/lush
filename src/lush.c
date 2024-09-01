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

#include "lush.h"
#include <bits/time.h>
#include <linux/limits.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

// -- builtin functions --
char *builtin_strs[] = {"cd", "help", "exit", "time"};

int (*builtin_func[])(char ***) = {&lush_cd, &lush_help, &lush_exit,
								   &lush_time};

int lush_num_builtins() { return sizeof(builtin_strs) / sizeof(char *); }

int lush_cd(char ***args) {
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
		if (tilda) {
			strcpy(path, pw->pw_dir);
			strcat(path, tilda + 1);
		} else {
			strcpy(path, args[0][1]);
		}
		char *exp_path = realpath(path, extended_path);
		if (!exp_path) {
			perror("realpath");
			return 0;
		}
		if (chdir(exp_path) != 0) {
			perror("lush: cd");
		}
	}

	return 1;
}

int lush_help(char ***args) {
	// TODO: make this more fun
	printf("Lunar Shell Help Page\n\n");
	printf("Available commands: \n");
	for (int i = 0; i < lush_num_builtins(); i++) {
		printf("- %s\n", builtin_strs[i]);
	}
	return 1;
}

int lush_exit(char ***args) { return 0; }

int lush_time(char ***args) {
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
	int rc = lush_run(args, i);
	clock_gettime(CLOCK_MONOTONIC, &end);

	elapsed_time = (end.tv_sec - start.tv_sec) * 1000.0 +
				   (end.tv_nsec - start.tv_nsec) / 1e6;
	printf("Time: %.3f milliseconds", elapsed_time);

	// return pointer back to "time" for free()
	args[0]--;
	return rc;
}

// -- shell utility --
char *lush_read_line() {
	char *line = NULL;
	size_t bufsize = 0;
	if (getline(&line, &bufsize, stdin) == -1) {
		if (feof(stdin)) {
			exit(EXIT_SUCCESS);
		} else {
			perror("readline");
			exit(EXIT_FAILURE);
		}
	}
	return line;
}

char **lush_split_pipes(char *line) {
	char **commands = calloc(16, sizeof(char *));
	if (!commands) {
		perror("calloc failed");
		exit(1);
	}

	char *command;
	int pos = 0;

	command = strtok(line, "|");
	while (command) {
		commands[pos++] = command;
		command = strtok(NULL, "|");
	}

	// trim off whitespace
	for (int i = 0; i < pos; i++) {
		while (*commands[i] == ' ' || *commands[i] == '\n') {
			commands[i]++;
		}
		char *end_of_str = strrchr(commands[i], '\0');
		--end_of_str;
		while (*end_of_str == ' ' || *end_of_str == '\n') {
			*end_of_str = '\0';
			--end_of_str;
		}
	}
	return commands;
}

char ***lush_split_args(char **commands, int *status) {
	int outer_pos = 0;
	char ***command_args = calloc(128, sizeof(char **));
	if (!command_args) {
		perror("calloc failed");
		exit(1);
	}

	for (int i = 0; commands[i]; i++) {
		int pos = 0;
		char **args = calloc(128, sizeof(char *));
		if (!args) {
			perror("calloc failed");
			exit(1);
		}

		bool inside_string = false;
		char *current_token = &commands[i][0];
		for (int j = 0; commands[i][j]; j++) {
			if (commands[i][j] == '"' && !inside_string) {
				// beginning of a string
				commands[i][j++] = '\0';
				if (commands[i][j] != '"') {
					inside_string = true;
					current_token = &commands[i][j];
				} else {
					commands[i][j] = '\0';
					current_token = &commands[i][++j];
				}
			} else if (inside_string) {
				if (commands[i][j] == '"') {
					// ending of a string
					inside_string = false;
					commands[i][j] = '\0';
					args[pos++] = current_token;
					current_token = NULL;
				} else {
					// character in string
					continue;
				}
			} else if (commands[i][j] == ' ') {
				// space delimeter
				if (current_token && *current_token != ' ') {
					args[pos++] = current_token;
				}
				current_token = &commands[i][j + 1]; // go past the space
				commands[i][j] = '\0';				 // null the space
			} else if (commands[i][j] == '$' && commands[i][j + 1] &&
					   commands[i][j + 1] != ' ') {
				// environment variable
				args[pos++] = getenv(&commands[i][++j]);
				while (commands[i][j]) {
					++j;
				}
				current_token = &commands[i][j + 1];
			} else {
				// regular character
				continue;
			}
		}

		// verify that string literals are finished
		if (inside_string) {
			*status = -1;
			return command_args;
		} else if (current_token && *current_token != ' ') {
			// tack on last arg
			args[pos++] = current_token;
		}

		// add this commands args array to the outer array
		command_args[outer_pos++] = args;
	}

	*status = outer_pos;
	return command_args;
}

int lush_execute_pipeline(char ***commands, int num_commands) {
	// no command given
	if (commands[0][0][0] == '\0') {
		return 1;
	}

	// create pipes for each command
	int **pipes = malloc((num_commands - 1) * sizeof(int *));
	for (int i = 0; i < num_commands - 1; i++) {
		pipes[i] =
			malloc(2 * sizeof(int)); // pipes[i][0] = in, pipes[i][1] = out
		if (pipe(pipes[i]) == -1) {
			perror("pipe");
			return 0;
		}
	}

	// execute commands in the pipeline
	for (int i = 0; i < num_commands - 1; i++) {
		int input_fd = (i == 0) ? STDIN_FILENO : pipes[i - 1][0];
		int output_fd = pipes[i][1];

		lush_execute_command(commands[i], input_fd, output_fd);
		close(output_fd); // no longer need to write to this pipe
	}

	// execute last or only command
	int input_fd =
		(num_commands > 1) ? pipes[num_commands - 2][0] : STDIN_FILENO;
	int output_fd = STDOUT_FILENO;
	lush_execute_command(commands[num_commands - 1], input_fd, output_fd);

	// close pipes
	for (int i = 0; i < num_commands - 1; i++) {
		close(pipes[i][0]);
		close(pipes[i][1]);
		free(pipes[i]);
	}
	free(pipes);
	return 1;
}

void lush_execute_command(char **args, int input_fd, int output_fd) {
	// create child
	pid_t pid;
	pid_t wpid;
	int status;

	if ((pid = fork()) == 0) {
		// child process content

		// redirect in and out fd's if needed
		if (input_fd != STDIN_FILENO) {
			dup2(input_fd, STDIN_FILENO);
			close(input_fd);
		}

		if (output_fd != STDOUT_FILENO) {
			dup2(output_fd, STDOUT_FILENO);
			close(output_fd);
		}

		// execute the command
		if (execvp(args[0], args) == -1) {
			perror("execvp");
			exit(EXIT_FAILURE);
		}
	} else if (pid < 0) {
		// forking failed
		perror("fork");
		exit(EXIT_FAILURE);
	} else {
		// parent process
		do {
			wpid = waitpid(pid, &status, WUNTRACED);
		} while (!WIFEXITED(status) && !WIFSIGNALED(status));
	}
}

int lush_run(char ***commands, int num_commands) {
	if (commands[0][0] == NULL) {
		// no command given
		return 1;
	}

	// check shell builtins
	for (int i = 0; i < lush_num_builtins(); i++) {
		if (strcmp(commands[0][0], builtin_strs[i]) == 0) {
			return ((*builtin_func[i])(commands));
		}
	}

	return lush_execute_pipeline(commands, num_commands);
}

int main() {
	int status = 0;
	while (true) {
		// Prompt
		char *username = getenv("USER");
		char device_name[256];
		gethostname(device_name, sizeof(device_name));
		char *cwd = getcwd(NULL, 0);

		// Replace /home/<user> with ~
		char *home_prefix = "/home/";
		size_t home_len = strlen(home_prefix) + strlen(username);
		char *prompt_cwd;
		if (strncmp(cwd, home_prefix, strlen(home_prefix)) == 0 &&
			strncmp(cwd + strlen(home_prefix), username, strlen(username)) ==
				0) {
			prompt_cwd = malloc(strlen(cwd) - home_len +
								2); // 1 for ~ and 1 for null terminator
			snprintf(prompt_cwd, strlen(cwd) - home_len + 2, "~%s",
					 cwd + home_len);
		} else {
			prompt_cwd = strdup(cwd);
		}

		// Print the prompt
		printf("%s@%s:%s$ ", username, device_name, prompt_cwd);

		char *line = lush_read_line();
		char **commands = lush_split_pipes(line);
		char ***args = lush_split_args(commands, &status);
		if (status == -1) {
			fprintf(stderr, "lush: Expected end of quoted string\n");
		} else if (lush_run(args, status) == 0) {
			exit(1);
		}

		for (int i = 0; args[i]; i++) {
			free(args[i]);
		}
		free(args);
		free(commands);
		free(line);
		free(cwd);
	}
}
