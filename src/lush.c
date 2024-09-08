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
#include "help.h"
#include "history.h"
#include "lauxlib.h"
#include "lua.h"
#include "lua_api.h"
#include "lualib.h"
#include <asm-generic/ioctls.h>
#include <bits/time.h>
#include <linux/limits.h>
#include <pwd.h>
#include <signal.h>
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

#define BUFFER_SIZE 1024

// -- builtin functions --
char *builtin_strs[] = {"cd", "help", "exit", "time"};

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
		if (tilda) {
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
		if (chdir(exp_path) != 0) {
			perror("lush: cd");
		}
	}

	return 1;
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
		printf("- %s\n", builtin_strs[i]);
	}
	return 1;
}

int lush_exit(lua_State *L, char ***args) { return 0; }

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

	lua_load_script(L, script, args[0]);

	// return pointer back to lua file
	args[0]--;
	return 1;
}

// -- shell utility --

// -- static helpers for input --

static void set_raw_mode(struct termios *orig_termios) {
	struct termios raw;
	tcgetattr(STDIN_FILENO, orig_termios);
	raw = *orig_termios;
	raw.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

static void reset_terminal_mode(struct termios *orig_termios) {
	tcsetattr(STDIN_FILENO, TCSANOW, orig_termios);
}

static int get_terminal_width() {
	struct winsize w;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
		perror("ioctl");
		return -1;
	}
	return w.ws_col;
}

static char *get_prompt() {
	char *username = getenv("USER");
	char device_name[256];
	gethostname(device_name, sizeof(device_name));
	char *cwd = getcwd(NULL, 0);

	// Replace /home/<user> with ~
	char *home_prefix = "/home/";
	size_t home_len = strlen(home_prefix) + strlen(username);
	char *prompt_cwd;
	if (strncmp(cwd, home_prefix, strlen(home_prefix)) == 0 &&
		strncmp(cwd + strlen(home_prefix), username, strlen(username)) == 0) {
		prompt_cwd = malloc(strlen(cwd) - home_len +
							2); // 1 for ~ and 1 for null terminator
		snprintf(prompt_cwd, strlen(cwd) - home_len + 2, "~%s", cwd + home_len);
	} else {
		prompt_cwd = strdup(cwd);
	}

	// Print the prompt
	size_t prompt_len =
		strlen(prompt_cwd) + strlen(username) + strlen(device_name) + 6;
	char *prompt = (char *)malloc(prompt_len);
	snprintf(prompt, prompt_len, "[%s@%s:%s]", username, device_name,
			 prompt_cwd);

	free(cwd);
	return prompt;
}

static void reprint_buffer(char *buffer, int *last_lines, int *pos,
						   int history_pos) {
	char *prompt = get_prompt();
	int width = get_terminal_width();

	// handle history before doing calculations
	if (history_pos >= 0) {
		char *history_line = lush_get_past_command(history_pos);
		if (history_line != NULL) {
			strncpy(buffer, history_line, BUFFER_SIZE);
			free(history_line);
			// remove newline from buffer
			buffer[strlen(buffer) - 1] = '\0';
			*pos = strlen(buffer);
		}
	}

	int num_lines = ((strlen(buffer) + strlen(prompt) + 1) / width) + 1;
	int cursor_pos = (strlen(prompt) + *pos + 1) % width;
	int cursor_line = (strlen(prompt) + *pos + 1) / width + 1;
	// move cursor down if it is up a number of lines first
	if (num_lines - cursor_line > 0) {
		printf("\033[%dB", num_lines - cursor_line);
		// compensate for if the we have just filled a line
		if ((strlen(buffer) + strlen(prompt) + 1) % width == 0) {
			printf("\033[A");
		}
	}
	for (int i = 0; i < *last_lines; i++) {
		printf("\r\033[K");
		if (i > 0)
			printf("\033[A");
	}
	*last_lines = num_lines;

	// ensure line is cleared before printing
	printf("\r\033[K");
	printf("%s ", prompt);
	printf("%s ", buffer);

	// move cursor up and to the right to be in correct position
	if (cursor_pos > 0)
		printf("\r\033[%dC", cursor_pos);
	else
		printf("\r");
	if (num_lines > 1 && (num_lines - cursor_line) > 0)
		printf("\033[%dA", num_lines - cursor_line);

	free(prompt);
}

char *lush_read_line() {
	struct termios orig_termios;
	char *buffer = (char *)calloc(BUFFER_SIZE, sizeof(char));
	int pos = 0;
	int history_pos = -1;
	int last_lines = 1;
	char last_command;
	int c;

	// init buffer and make raw mode
	set_raw_mode(&orig_termios);

	while (true) {
		c = getchar();

		if (c == '\033') { // escape sequence
			getchar();	   // skip [
			switch (getchar()) {
			case 'A': // up arrow
				reprint_buffer(buffer, &last_lines, &pos, ++history_pos);
				break;
			case 'B': // down arrow
				reprint_buffer(buffer, &last_lines, &pos, --history_pos);
				if (history_pos < 0)
					history_pos = 0;
				break;
			case 'C': // right arrow
			{
				int width = get_terminal_width();
				char *prompt = get_prompt();
				if ((strlen(prompt) + pos) % width == width - 2) {
					printf("\033[B");
				}
				if (pos < strlen(buffer)) {
					pos++;
					// if modifying text reset history
					history_pos = -1;
					reprint_buffer(buffer, &last_lines, &pos, history_pos);
				}
				free(prompt);
			} break;
			case 'D': // left arrow
			{
				int width = get_terminal_width();
				char *prompt = get_prompt();
				if ((strlen(prompt) + pos) % width == width - 1) {
					printf("\033[A");
				}

				if (pos > 0) {
					pos--;
					// if modifying text reset history
					history_pos = -1;
					reprint_buffer(buffer, &last_lines, &pos, history_pos);
				}
				free(prompt);
			} break;
			case '3': // delete
				if (getchar() == '~') {
					if (pos < strlen(buffer)) {
						memmove(&buffer[pos], &buffer[pos + 1],
								strlen(&buffer[pos + 1]) + 1);
						// if modifying text reset history
						history_pos = -1;
						reprint_buffer(buffer, &last_lines, &pos, history_pos);
					}
				}
				break;
			default:
				break;
			}
		} else if (c == '\177') { // backspace
			if (pos > 0) {
				memmove(&buffer[pos - 1], &buffer[pos],
						strlen(&buffer[pos]) + 1);
				pos--;
				// if modifying text reset history
				history_pos = -1;
				reprint_buffer(buffer, &last_lines, &pos, history_pos);
			}
		} else if (c == '\n') {
			// if modifying text reset history
			history_pos = -1;
			pos = strlen(buffer);
			reprint_buffer(buffer, &last_lines, &pos, history_pos);
			break; // submit the command
		} else {
			if (pos < BUFFER_SIZE - 1) {
				// insert text into buffer
				memmove(&buffer[pos + 1], &buffer[pos],
						strlen(&buffer[pos]) + 1);
				buffer[pos] = c;
				pos++;
				// handle edge case where cursor should be moved down
				int width = get_terminal_width();
				char *prompt = get_prompt();
				if ((strlen(prompt) + pos) % width == width - 1 &&
					pos < strlen(buffer)) {
					printf("\033[B");
				}
				// if modifying text reset history
				history_pos = -1;
				reprint_buffer(buffer, &last_lines, &pos, history_pos);
			}
		}
	}

	reset_terminal_mode(&orig_termios);
	return buffer;
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
	int status;

	struct sigaction sa;

	if ((pid = fork()) == 0) {
		// child process content

		// restore default sigint for child
		sa.sa_handler = SIG_DFL;
		sigaction(SIGINT, &sa, NULL);

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
			waitpid(pid, &status, WUNTRACED);
		} while (!WIFEXITED(status) && !WIFSIGNALED(status));
	}
}

int lush_run(lua_State *L, char ***commands, int num_commands) {
	if (commands[0][0] == NULL) {
		// no command given
		return 1;
	}

	// check if the command is a lua script
	char *ext = strchr(commands[0][0], '.');
	if (ext) {
		ext++;
		if (strcmp(ext, "lua") == 0) {
			return ((*builtin_func[4])(L, commands));
		}
	}

	// check shell builtins
	for (int i = 0; i < lush_num_builtins(); i++) {
		if (strcmp(commands[0][0], builtin_strs[i]) == 0) {
			return ((*builtin_func[i])(L, commands));
		}
	}

	return lush_execute_pipeline(commands, num_commands);
}

int main(int argc, char *argv[]) {
	// check if the --version arg was passed
	if (argc > 1 && strcmp(argv[1], "--version") == 0) {
#ifdef LUSH_VERSION
		printf("Lunar Shell, version %s\n", LUSH_VERSION);
#endif
		return 0;
	}
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);
	lua_register_api(L);
	// eat ^C in main
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);

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
		printf("[%s@%s:%s] ", username, device_name, prompt_cwd);
		char *line = lush_read_line();
		lush_push_history(line);
		printf("\n");
		if (line == NULL || strlen(line) == 0) {
			free(line);
			continue;
		}
		char **commands = lush_split_pipes(line);
		char ***args = lush_split_args(commands, &status);
		if (status == -1) {
			fprintf(stderr, "lush: Expected end of quoted string\n");
		} else if (lush_run(L, args, status) == 0) {
			exit(1);
		}

		for (int i = 0; args[i]; i++) {
			free(args[i]);
		}
		// add last line to history
		free(cwd);
		free(args);
		free(commands);
		free(line);
	}
	lua_close(L);
	return 0;
}
