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
#include "hashmap.h"
#include "lauxlib.h"
#include "lua.h"
#include "lua_api.h"
#include "lualib.h"
#include "compat-5.3.h"
#include <asm-generic/ioctls.h>
#include <bits/time.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <glob.h>
#include <linux/limits.h>
#include <locale.h>
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

// Forward declare the open functions for the compat C modules we need to preload
int luaopen_bit32 (lua_State *L);
int luaopen_utf8 (lua_State *L);

#define BUFFER_SIZE 1024
#define MAX_GLOB 512

typedef enum {
	OP_PIPE = 1,		// |
	OP_AND,				// &&
	OP_OR,				// ||
	OP_SEMICOLON,		// ;
	OP_BACKGROUND,		// &
	OP_REDIRECT_STDOUT, // 1> or >
	OP_REDIRECT_STDERR, // 2>
	OP_REDIRECT_BOTH,	// &>
	OP_APPEND_STDOUT,	// 1>> or >>
	OP_APPEND_STDERR,	// 2>>
	OP_APPEND_BOTH,		// &>>
	OP_OTHER			// All other operators like parentheses, braces, etc.
} OperatorType;

// initialize prompt format
char *prompt_format = NULL;

// -- aliasing --
hashmap_t *aliases = NULL;

void lush_add_alias(const char *alias, const char *command) {
	// make a new map if one doesnt exist
	if (aliases == NULL) {
		aliases = hm_new_hashmap();
	}

	hm_set(aliases, (char *)alias, (char *)command);
}

char *lush_get_alias(char *alias) { return hm_get(aliases, alias); }

// -- shell utility --

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

// -- prompt helper functions --

static size_t get_prompt_size(const char *format, const char *username,
							  const char *hostname, const char *cwd) {
	size_t prompt_len = 0;

	while (*format) {
		if (strncmp(format, "%u", 2) == 0) {
			prompt_len += strlen(username);
			format += 2;
		} else if (strncmp(format, "%h", 2) == 0) {
			prompt_len += strlen(hostname);
			format += 2;
		} else if (strncmp(format, "%w", 2) == 0) {
			prompt_len += strlen(cwd);
			format += 2;
		} else if (strncmp(format, "%t", 2) == 0) {
			prompt_len += 8; // size of time format
			format += 2;
		} else if (strncmp(format, "%d", 2) == 0) {
			prompt_len += 10;
			format += 2;
		} else {
			prompt_len++;
			format++;
		}
	}
	return prompt_len;
}

static char *format_prompt_string(const char *input, const char *username,
								  const char *hostname, const char *cwd) {
	// Calculate the size of the new string
	size_t new_size = get_prompt_size(input, username, hostname, cwd) + 1;

	// Allocate memory for the new string
	char *result = (char *)malloc(new_size);
	if (!result) {
		perror("malloc failed");
		return NULL;
	}

	// Get the current time
	time_t current_time;
	time(&current_time);
	struct tm *local_time = localtime(&current_time);

	// Replace placeholders in the input string and build the result string
	char *dest = result;
	while (*input) {
		if (strncmp(input, "%u", 2) == 0) {
			strcpy(dest, username);
			dest += strlen(username);
			input += 2;
		} else if (strncmp(input, "%h", 2) == 0) {
			strcpy(dest, hostname);
			dest += strlen(hostname);
			input += 2;
		} else if (strncmp(input, "%w", 2) == 0) {
			strcpy(dest, cwd);
			dest += strlen(cwd);
			input += 2;
		} else if (strncmp(input, "%t", 2) == 0) {
			// Format the time as HH:MM:SS
			char time_string[9]; // HH:MM:SS is 8 characters + null terminator
			strftime(time_string, sizeof(time_string), "%H:%M:%S", local_time);
			strcpy(dest, time_string);
			dest += strlen(time_string);
			input += 2;
		} else if (strncmp(input, "%d", 2) == 0) {
			// Format the date as mm/dd/yyyy
			char date_string[36];
			snprintf(date_string, 36, "%02d/%02d/%04d", local_time->tm_mon + 1,
					 local_time->tm_mday, local_time->tm_year + 1900);
			strcpy(dest, date_string);
			dest += strlen(date_string);
			input += 2;
		} else {
			*dest++ = *input++;
		}
	}

	*dest = '\0'; // Null-terminate the result string
	return result;
}

static char *get_prompt() {
	char *username = getenv("USER");
	char hostname[256];
	gethostname(hostname, sizeof(hostname));
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

	// get the prompt if no format in init.lua
	if (prompt_format == NULL) {
		size_t prompt_len =
			strlen(prompt_cwd) + strlen(username) + strlen(hostname) + 6;
		char *prompt = (char *)malloc(prompt_len);
		snprintf(prompt, prompt_len, "[%s@%s:%s]", username, hostname,
				 prompt_cwd);
		free(cwd);
		return prompt;
	}

	// get formatted prompt
	char *prompt =
		format_prompt_string(prompt_format, username, hostname, prompt_cwd);

	free(cwd);
	return prompt;
}

static size_t get_stripped_length(const char *str) {
	size_t len = 0;
	size_t i = 0;

	// Set the locale to properly handle UTF-8 multi-byte characters
	setlocale(LC_CTYPE, "");

	while (str[i] != '\0') {
		if (str[i] == '\033' && str[i + 1] == '[') {
			// Skip over the escape sequence
			while (str[i] != 'm' && str[i] != '\0') {
				i++;
			}
			// Include 'm' character to end the escape sequence
			if (str[i] == 'm') {
				i++;
			}
		} else if (str[i] == '\n') {
			len = 0;
			i++;
		} else {
			// Calculate the length of the current multi-byte character
			int char_len = mblen(&str[i], MB_CUR_MAX);
			if (char_len < 1) {
				char_len = 1; // Fallback in case of errors
			}
			len++;
			i += char_len;
		}
	}

	return len;
}

// physical refers to the number of character spaces it takes up
static size_t get_physical_length(const char *str) {
	size_t len = 0;
	size_t i = 0;

	// Set the locale to properly handle UTF-8 multi-byte characters
	setlocale(LC_CTYPE, "");

	while (str[i] != '\0') {
		if (str[i] == '\033' && str[i + 1] == '[') {
			// Skip over the escape sequence
			while (str[i] != 'm' && str[i] != '\0') {
				i++;
			}
			// Include 'm' character to end the escape sequence
			if (str[i] == 'm') {
				i++;
			}
		} else {
			// Calculate the length of the current multi-byte character
			int char_len = mblen(&str[i], MB_CUR_MAX);
			if (char_len < 1) {
				char_len = 1; // Fallback in case of errors
			}
			len++;
			i += char_len;
		}
	}

	return len;
}

static char *get_stripped_prompt(const char *str) {
	size_t i = 0, j = 0;
	char *clean_str = (char *)malloc(strlen(str) + 1);

	if (clean_str == NULL) {
		perror("malloc failed");
		exit(EXIT_FAILURE);
	}

	// Set the locale to properly handle UTF-8 multi-byte characters
	setlocale(LC_CTYPE, "");

	while (str[i] != '\0') {
		if (str[i] == '\033' && str[i + 1] == '[') {
			// Skip over the escape sequence
			while (str[i] != 'm' && str[i] != '\0') {
				i++;
			}
			// Skip the 'm' character to end the escape sequence
			if (str[i] == 'm') {
				i++;
			}
		} else {
			// Calculate the length of the current multi-byte character
			int char_len = mblen(&str[i], MB_CUR_MAX);
			if (char_len < 1) {
				char_len = 1; // Fallback in case of errors
			}

			// Copy the non-escape sequence character(s) to the clean string
			for (int k = 0; k < char_len; k++) {
				clean_str[j++] = str[i++];
			}
		}
	}

	// Null-terminate the clean string
	clean_str[j] = '\0';

	return clean_str;
}

static int get_prompt_newlines(const char *prompt) {
	int newlines = 0;
	int width = get_terminal_width();
	size_t lines = 0;
	size_t capacity = 2;
	char *stripped_prompt = get_stripped_prompt(prompt);
	int *line_widths = (int *)malloc(capacity * sizeof(int));

	if (line_widths == NULL) {
		perror("malloc failed");
		exit(EXIT_FAILURE);
	}

	setlocale(LC_CTYPE, "");

	size_t i = 0, j = 0;
	while (i < strlen(stripped_prompt)) {
		int char_len = mblen(&stripped_prompt[i], MB_CUR_MAX);
		if (char_len < 1)
			char_len = 1;

		if (stripped_prompt[i] == '\n') {
			if (i % width != width - 1)
				newlines++;

			if (lines >= capacity) {
				capacity *= 2;
				line_widths = realloc(line_widths, capacity * sizeof(int));
				if (line_widths == NULL) {
					perror("realloc failed");
					exit(EXIT_FAILURE);
				}
			}
			line_widths[lines++] = j;
			j = 0;
		}
		j++;
		i += char_len;
	}

	// also account for if the terminal width causes wrapping
	for (size_t k = 0; k < lines; k++) {
		newlines += (line_widths[k]) / width;
		if ((line_widths[k] % width) == 0)
			newlines--;
	}

	// Total number of lines is newlines plus the number of wrapped lines
	free(line_widths);
	free(stripped_prompt);
	return newlines;
}

// -- autocomplete --

static int alphebetize_strings(const void *a, const void *b) {
	return strcmp(*(const char **)a, *(const char **)b);
}

static char **get_suggestions(size_t *count, const char *path) {
	DIR *dir;
	struct dirent *entry;
	size_t capacity = 10;
	size_t size = 0;

	// Allocate initial space for the array of strings
	char **items = malloc(capacity * sizeof(char *));
	if (items == NULL) {
		perror("Unable to allocate memory");
		exit(EXIT_FAILURE);
	}

	// Open the current directory
	dir = opendir(path);
	if (dir == NULL) {
		dir = opendir(".");
		if (dir == NULL) {
			perror("Unable to open current directory");
			free(items);
			exit(EXIT_FAILURE);
		}
	}

	// Read each directory entry
	while ((entry = readdir(dir)) != NULL) {
		// Allocate space for each filename
		if (size >= capacity) {
			capacity *= 2;
			items = realloc(items, capacity * sizeof(char *));
			if (items == NULL) {
				perror("Unable to reallocate memory");
				closedir(dir);
				exit(EXIT_FAILURE);
			}
		}

		// Copy the entry name to the array
		items[size] = strdup(entry->d_name);
		if (items[size] == NULL) {
			perror("Unable to allocate memory for entry");
			closedir(dir);
			exit(EXIT_FAILURE);
		}
		size++;
	}

	// Close the directory
	closedir(dir);

	// Set the count to the number of items found
	*count = size;

	// Reallocate the array to the exact size needed
	items = realloc(items, size * sizeof(char *));

	qsort(items, size, sizeof(char *), alphebetize_strings);

	return items;
}

static const char *suggestion_difference(const char *input,
										 const char *suggestion) {
	size_t input_len = strlen(input);
	size_t suggestion_len = strlen(suggestion);

	if (input_len <= suggestion_len &&
		strncmp(input, suggestion, input_len) == 0) {
		return &suggestion[input_len];
	}

	return NULL;
}

static void free_suggestions(char **suggestions, int count) {
	for (size_t i = 0; i < count; i++) {
		free(suggestions[i]);
	}

	free(suggestions);
}

static const char *find_suggestion(const char *input, char **suggestions,
								   size_t count) {
	if (strlen(input) == 0)
		return NULL;

	for (size_t i = 0; i < count; i++) {
		if (strncmp(input, suggestions[i], strlen(input)) == 0) {
			const char *suggestion =
				suggestion_difference(input, suggestions[i]);
			return suggestion;
		}
	}

	return NULL;
}

static const char *get_current_token(const char *input) {
	const char *last_space = strrchr(input, ' ');

	if (last_space == NULL)
		return input;

	// return the substring after the space
	return last_space + 1;
}

static const char *get_current_word(const char *input) {
	const char *current_token = get_current_token(input);
	const char *last_slash = strrchr(current_token, '/');

	if (last_slash == NULL)
		return current_token;

	return last_slash + 1;
}

char *get_suggestions_path(const char *str) {
	if (str == NULL) {
		return strdup("./");
	}

	const char *last_slash = strrchr(str, '/');

	if (last_slash == NULL) {
		return strdup("./");
	}

	size_t length = last_slash - str;

	// Allocate memory for the substring and copy it
	char *result = (char *)malloc(length + 3);
	if (result == NULL) {
		perror("malloc failed");
		exit(EXIT_FAILURE);
	}

	strncpy(result, "./", 2);
	result += 2;
	strncpy(result, str, length);
	result -= 2;
	result[length + 2] = '\0';

	return result;
}

// -- shell buffer handling --

static void reprint_buffer(char *buffer, int *last_lines, int *pos,
						   int history_pos) {
	static size_t old_buffer_len = 0;
	char suggestion[PATH_MAX] = {0};
	char *prompt = get_prompt();
	int width = get_terminal_width();
	int prompt_newlines = get_prompt_newlines(prompt);
	size_t prompt_length = get_stripped_length(prompt);
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

	// handle autocomplete before doing calculations as well
	size_t suggestions_count = 0;
	const char *current_token = get_current_token(buffer);
	char *suggestions_path = get_suggestions_path(current_token);
	const char *current_word = get_current_word(buffer);
	char **suggestions = get_suggestions(&suggestions_count, suggestions_path);
	const char *autocomplete_suggestion =
		find_suggestion(current_word, suggestions, suggestions_count);

	if (autocomplete_suggestion != NULL) {
		strncpy(suggestion, autocomplete_suggestion, PATH_MAX);
	}

	// free(suggestions_path);

	int num_lines = ((strlen(buffer) + prompt_length + 1) / width) + 1;
	int cursor_pos = (prompt_length + *pos + 1) % width;
	int cursor_line = (prompt_length + *pos + 1) / width + 1;
	// move cursor down if it is up a number of lines first
	if (num_lines - cursor_line > 0) {
		printf("\033[%dB", num_lines - cursor_line);
		// compensate for if we have just filled a line
		if ((strlen(buffer) + prompt_length + 1) % width == 0 &&
			old_buffer_len < strlen(buffer)) {
			printf("\033[A");
		}
		// compense for if we have just emptied a line
		if ((strlen(buffer) + prompt_length + 1) % width == width - 1 &&
			old_buffer_len > strlen(buffer)) {
			printf("\033[B");
		}
	}

	if (old_buffer_len < strlen(buffer) || history_pos >= 0) {
		for (int i = 0; i < *last_lines; i++) {
			printf("\r\033[K");
			if (i > 0)
				printf("\033[A");
		}
	} else {
		for (int i = 0; i < num_lines; i++) {
			printf("\r\033[K");
			if (i > 0)
				printf("\033[A");
		}
	}
	*last_lines = num_lines;

	// if the prompt has new lines move up
	if (prompt_newlines > 0) {
		printf("\033[%dA", prompt_newlines);
	}

	// ensure line is cleared before printing
	printf("\r\033[K");
	printf("%s ", prompt);
	printf("%s", buffer);
	if (suggestion_enable)
		printf("\033[0;33m%s\033[0m ", suggestion);

	// move cursor up and to the right to be in correct position
	if (cursor_pos > 0)
		printf("\r\033[%dC", cursor_pos);
	else
		printf("\r");

	if (num_lines > 1 && (num_lines - cursor_line) > 0)
		printf("\033[%dA", num_lines - cursor_line);

	// if autocomplete goes to new line move up
	int suggested_lines =
		((strlen(buffer) + strlen(suggestion) + prompt_length + 1) / width) + 1;
	if (suggested_lines > num_lines)
		printf("\033[A");

	// cleanup
	free(prompt);
	old_buffer_len = strlen(buffer);
	suggestion[0] = '\0';
	free_suggestions(suggestions, suggestions_count);
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
				size_t prompt_length = get_stripped_length(prompt);
				if ((prompt_length + pos) % width == width - 2) {
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
				size_t prompt_length = get_stripped_length(prompt);
				if ((prompt_length + pos) % width == width - 1) {
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
				// handle edge case where cursor should be moved up
				int width = get_terminal_width();
				char *prompt = get_prompt();
				size_t prompt_length = get_stripped_length(prompt);
				if ((prompt_length + pos + 1) % width == width - 1 &&
					pos <= strlen(buffer)) {
					printf("\033[A");
				}
				// if modifying text reset history
				history_pos = -1;
				reprint_buffer(buffer, &last_lines, &pos, history_pos);
			}
		} else if (c == '\t') {
			char suggestion[PATH_MAX];
			size_t suggestions_count = 0;
			const char *current_token = get_current_token(buffer);
			char *suggestions_path = get_suggestions_path(current_token);
			const char *current_word = get_current_word(buffer);
			char **suggestions =
				get_suggestions(&suggestions_count, suggestions_path);
			const char *autocomplete_suggestion =
				find_suggestion(current_word, suggestions, suggestions_count);

			if (autocomplete_suggestion != NULL) {
				size_t suggestion_len = strlen(autocomplete_suggestion);
				size_t current_word_len = strlen(current_word);

				// Insert the suggestion in place of the current word
				memmove(&buffer[pos + suggestion_len], &buffer[pos],
						strlen(&buffer[pos]) + 1);
				strncpy(&buffer[pos], autocomplete_suggestion, suggestion_len);

				// Move the cursor forward
				pos += suggestion_len;
			}

			reprint_buffer(buffer, &last_lines, &pos, history_pos);
			free(suggestions_path);
			free_suggestions(suggestions, suggestions_count);

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
				size_t prompt_length = get_stripped_length(prompt);
				if ((prompt_length + pos) % width == width - 1 &&
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

char *lush_resolve_aliases(char *line) {
	// Allocate memory for the new string
	char *result = (char *)malloc(BUFFER_SIZE);
	if (!result) {
		perror("malloc failed");
		return NULL;
	}

	// Create a copy of the input line for tokenization
	char *line_copy = strdup(line);
	if (!line_copy) {
		perror("strdup failed");
		free(result);
		return NULL;
	}

	// Start building the result string
	char *dest = result;
	char *arg = strtok(line_copy, " ");
	while (arg != NULL) {
		// Check shell aliases
		if (aliases != NULL) {
			char *alias = hm_get(aliases, arg);
			if (alias != NULL) {
				// Replace alias
				strcpy(dest, alias);
				dest += strlen(alias);
			} else {
				// No alias found, use the original token
				strcpy(dest, arg);
				dest += strlen(arg);
			}
		} else {
			// No aliases set, just use the original token
			strcpy(dest, arg);
			dest += strlen(arg);
		}

		// Add a space after each token (if it's not the last one)
		arg = strtok(NULL, " ");
		if (arg != NULL) {
			*dest++ = ' ';
		}
	}

	*dest = '\0'; // Null-terminate the result string

	// Clean up
	free(line_copy);

	return result;
}

static void lush_expand_globs(char ***args) {
	if (args == NULL || *args == NULL) {
		return;
	}

	// iterate over each arg
	for (int i = 0; args[i]; i++) {
		char **arg_list = args[i];
		for (int j = 0; arg_list[j]; j++) {
			char *arg = arg_list[j];

			// check if the arg string has a glob pattern
			if (strpbrk(arg, "*?[") != NULL) {
				glob_t glob_result;
				memset(&glob_result, 0, sizeof(glob_result));

				// use glob to expand the arg
				int ret =
					glob(arg, GLOB_TILDE | GLOB_BRACE, NULL, &glob_result);
				if (ret == 0) {
					// if there is a nonzero number of matches malloc new arg
					// memory
					if (glob_result.gl_pathc > 0) {
						char **new_args = (char **)malloc(
							(glob_result.gl_pathc + 1) * sizeof(char *));
						if (new_args == NULL) {
							perror("malloc failed");
							globfree(&glob_result);
							return;
						}

						for (size_t k = 0; k < glob_result.gl_pathc; k++) {
							new_args[k] = strdup(glob_result.gl_pathv[k]);
							if (new_args[k] == NULL) {
								perror("strdup failed");
								for (size_t l = 0; l < k; l++) {
									free(new_args[l]);
								}
								free(new_args);
								globfree(&glob_result);
								return;
							}
						}
						new_args[glob_result.gl_pathc] = NULL;

						// replace the original arg with new list
						arg_list[j] = new_args[0];

						for (size_t k = 1; k < glob_result.gl_pathc; k++) {
							// insert the new argument
							arg_list[j + k] = new_args[k];
						}

						free(new_args);
					}
				} else if (ret == GLOB_NOMATCH) {

				} else {
					fprintf(stderr, "glob failed with error code %d\n", ret);
				}

				globfree(&glob_result);
			}
		}
	}
}

static int is_operator(const char *str) {
	const char *operators[] = {"1>>", "2>>", "&>>", ">>", "1>", "2>", "&>",
							   "||",  "&&",	 ">",	"&",  ";",	"|"};
	int num_operators = sizeof(operators) / sizeof(operators[0]);
	for (int i = 0; i < num_operators; i++) {
		if (strncmp(str, operators[i], strlen(operators[i])) == 0) {
			switch (i) {
			case 0:
				return OP_APPEND_STDOUT;
			case 1:
				return OP_APPEND_STDERR;
			case 2:
				return OP_APPEND_BOTH;
			case 3:
				return OP_APPEND_STDOUT;
			case 4:
				return OP_REDIRECT_STDOUT;
			case 5:
				return OP_REDIRECT_STDERR;
			case 6:
				return OP_REDIRECT_BOTH;
			case 7:
				return OP_OR;
			case 8:
				return OP_AND;
			case 9:
				return OP_REDIRECT_STDOUT;
			case 10:
				return OP_BACKGROUND;
			case 11:
				return OP_SEMICOLON;
			case 12:
				return OP_PIPE;
			default:
				return 0;
			}
		}
	}
	return 0; // Not an operator
}

static int operator_length(const char *str) {
	const char *operators[] = {"1>>", "2>>", "&>>", ">>", "1>", "2>", "&>",
							   "||",  "&&",	 ">",	"&",  ";",	"|"};
	int num_operators = sizeof(operators) / sizeof(operators[0]);
	for (int i = 0; i < num_operators; i++) {
		if (strncmp(str, operators[i], strlen(operators[i])) == 0) {
			switch (i) {
			case 0:
			case 1:
			case 2:
				return 3;
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
				return 2;
			case 9:
			case 10:
			case 11:
			case 12:
				return 1;
			default:
				return 0;
			}
		}
	}
	return 0;
}

static char *trim_whitespace(char *str) {
	char *end;

	// Trim leading space
	while (isspace((unsigned char)*str))
		str++;

	if (*str == 0)
		return str; // If all spaces, return empty string

	// Trim trailing space
	end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char)*end))
		end--;

	*(end + 1) = '\0';

	return str;
}

// Split the command based on various chaining operations
char **lush_split_commands(char *line) {
	char **commands = calloc(16, sizeof(char *));
	if (!commands) {
		perror("calloc failed");
		exit(1);
	}

	int pos = 0;
	char *start = line;
	bool in_quote = false;
	while (*start) {
		// Skip leading spaces
		while (isspace((unsigned char)*start))
			start++;

		// Check for operators
		int op_len = operator_length(start);
		if (op_len > 0) {
			// Allocate memory for operator command
			char *operator_cmd = calloc(op_len + 1, sizeof(char));
			strncpy(operator_cmd, start, op_len);
			commands[pos++] = operator_cmd;
			start += op_len;
		} else {
			// Collect regular commands until the next operator or end of string
			char *next = start;
			while (*next) {
				if (*next == '"') {
					in_quote = !in_quote;
				}
				next++;

				if (!in_quote && is_operator(next))
					break;
			}

			// Copy the command between start and next
			char *command = strndup(start, next - start);
			commands[pos++] = trim_whitespace(command);
			start = next;
		}

		if (pos >= 16) {
			// Resize if necessary
			commands = realloc(commands, (pos + 16) * sizeof(char *));
			if (!commands) {
				perror("realloc failed");
				exit(1);
			}
		}
	}

	commands[pos] = NULL;
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

static int run_command(lua_State *L, char ***commands) {
	// check if the command is a lua script
	char *ext = strrchr(commands[0][0], '.');
	if (ext) {
		ext++;
		if (strcmp(ext, "lua") == 0) {
			return ((*builtin_func[LUSH_LUA])(L, commands));
		}
	}

	// check shell builtins
	for (int j = 0; j < lush_num_builtins(); j++) {
		if (strcmp(commands[0][0], builtin_strs[j]) == 0) {
			return ((*builtin_func[j])(L, commands));
		}
	}

	return lush_execute_command(commands[0], STDIN_FILENO, STDOUT_FILENO);
}

static int run_command_redirect(lua_State *L, char ***commands, int operator) {
	if (commands[2] == NULL)
		return -1;

	int mode = (operator>= OP_REDIRECT_STDOUT && operator<= OP_REDIRECT_BOTH)
				   ? O_TRUNC
				   : O_APPEND;

	int redirect_flags = 0;
	if (operator== OP_REDIRECT_STDOUT || operator== OP_APPEND_STDOUT) {
		redirect_flags = 1;
	} else if (operator== OP_REDIRECT_STDERR || operator== OP_APPEND_STDERR) {
		redirect_flags = 2;
	} else {
		redirect_flags = 3;
	}

	int saved_stdout = -1, saved_stderr = -1;
	int fd = open(commands[2][0], O_WRONLY | O_CREAT | mode, 0644);
	if (fd == -1) {
		perror("invalid fd");
		return -1;
	}

	// Redirect stdout
	if (redirect_flags & 1) {
		saved_stdout = dup(STDOUT_FILENO);
		if (saved_stdout == -1) {
			perror("dup stdout");
			close(fd);
			return -1;
		}

		if (dup2(fd, STDOUT_FILENO) == -1) {
			perror("dup2 stdout");
			close(fd);
			close(saved_stdout);
			return -1;
		}
	}

	// Redirect stderr
	if (redirect_flags & 2) {
		saved_stderr = dup(STDERR_FILENO);
		if (saved_stderr == -1) {
			perror("dup stderr");
			if (saved_stdout != -1) {
				dup2(saved_stdout, STDOUT_FILENO);
				close(saved_stdout);
			}
			close(fd);
			return -1;
		}

		if (dup2(fd, STDERR_FILENO) == -1) {
			perror("dup2 stderr");
			if (saved_stdout != -1) {
				dup2(saved_stdout, STDOUT_FILENO);
				close(saved_stdout);
			}
			close(saved_stderr);
			close(fd);
			return -1;
		}
	}

	close(fd);

	// Run the command
	run_command(L, commands);

	// Restore stdout
	if (saved_stdout != -1) {
		if (dup2(saved_stdout, STDOUT_FILENO) == -1) {
			perror("dup2 restore stdout");
			close(saved_stdout);
			return -1;
		}
		close(saved_stdout);
	}

	// Restore stderr
	if (saved_stderr != -1) {
		if (dup2(saved_stderr, STDERR_FILENO) == -1) {
			perror("dup2 restore stderr");
			close(saved_stderr);
			return -1;
		}
		close(saved_stderr);
	}

	return 0;
}

// TODO: Allow background process to run lua script
static int run_command_background(lua_State *L, char ***commands) {
	pid_t pid = fork();

	if (pid < 0) {
		perror("fork");
		return -1;
	} else if (pid == 0) {
		execvp(commands[0][0], commands[0]);
		perror("execvp background");
		exit(EXIT_FAILURE);
	}

	printf("Process %d running in background\n", pid);
	return 0;
}

int lush_execute_chain(lua_State *L, char ***commands, int num_commands) {
	if (commands[0][0][0] == '\0') {
		return 0;
	}

	int num_actions = (num_commands + 1) / 2;
	int last_result = 0;

	for (int i = 0; i < num_actions; i++) {
		// Handle &&, ||, and ; operators
		if (i > 0 && commands[0] != NULL) {
			commands--;
			if (last_result != 0) {
				if (is_operator(commands[0][0]) == OP_AND) {
					commands += 3;
					continue;
				}
			} else {
				if (is_operator(commands[0][0]) == OP_OR) {
					commands += 3;
					continue;
				}
			}
			commands++;

			if (is_operator(commands[0][0]) == OP_SEMICOLON) {
				commands++;
			}
		}

		// Handle other operations
		if (commands[1] != NULL) {
			int op_type = is_operator(commands[1][0]);
			if (op_type == OP_PIPE) {
				char ***pipe_commands =
					malloc(sizeof(char **) * (num_actions - i));
				int pipe_count = 0;

				while (i < num_actions - 1 && op_type == OP_PIPE) {
					pipe_commands[pipe_count++] = commands[0];
					commands += 2;
					i++;
					if (i < num_actions - 1) {
						op_type = is_operator(commands[1][0]);
					} else {
						break;
					}
				}

				pipe_commands[pipe_count++] = commands[0];
				last_result = lush_execute_pipeline(pipe_commands, pipe_count);

				free(pipe_commands);
				commands += 2;
				continue;
			} else if (op_type == OP_BACKGROUND) {
				last_result = run_command_background(L, commands);
				commands += 2;
				continue;
			} else if (op_type >= OP_REDIRECT_STDOUT &&
					   op_type <= OP_APPEND_BOTH) {
				last_result = run_command_redirect(L, commands, op_type);
				commands += 3;
				continue;
			}
		}
		// Run the command or move past the operator
		if (commands[0] != NULL && !is_operator(commands[0][0])) {
			last_result = run_command(L, commands);
			commands += 2;
		} else {
			commands++;
		}
	}

	return 0;
}

int lush_execute_pipeline(char ***commands, int num_commands) {
	// no command given
	if (commands[0][0][0] == '\0') {
		return 0;
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
	return 0;
}

static void build_alt_command(char *buffer, char **args) {
	size_t offset = 0;

	for (int i = 0; args[i]; i++) {
		if (offset + strlen(args[i]) + 1 < BUFFER_SIZE) {
			strcat(buffer, args[i]);
			strcat(buffer, " ");
			offset = strlen(buffer);
		} else {
			fprintf(stderr, "command too long\n");
			exit(EXIT_FAILURE);
		}
	}
}

int lush_execute_command(char **args, int input_fd, int output_fd) {
	// create child
	pid_t pid;
	int status;

	struct sigaction sa;

	if ((pid = fork()) == 0) {
		// child process content

		// restore default sigint for child
		sa.sa_handler = SIG_DFL;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
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
			if (alt_shell) {
				char command[BUFFER_SIZE] = {0};
				build_alt_command(command, args);
				execlp(alt_shell, alt_shell, "-c", command, (char *)NULL);
				perror("alt shell");
			} else {
				perror("lush");
				exit(EXIT_FAILURE);
			}
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

	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	}
	return -1;
}

int lush_run(lua_State *L, char ***commands, int num_commands) {
	if (commands[0][0] == NULL) {
		// no command given
		return 0;
	}

	return lush_execute_chain(L, commands, num_commands);
}

static void background_handler(int sig) {
	// Reap all child processes
	while (waitpid(-1, NULL, WNOHANG) > 0)
		;
}

int main(int argc, char *argv[]) {
	// check if the --version arg was passed
	if (argc > 1 && strcmp(argv[1], "--version") == 0) {
#ifdef LUSH_VERSION
		printf("Lunar Shell, version %s\n", LUSH_VERSION);
#endif
		return 0;
	}

		// init lua state
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);

    // --- Pre-load compat modules ---
    // This is to make C modules available to Lua
    luaL_getglobal(L, "package");
    luaL_getfield(L, -1, "preload");

    // Preload bit32 for Lua 5.1 compatibility
    lua_pushcfunction(L, luaopen_bit32);
    lua_setfield(L, -2, "bit32");

    // Preload utf8 for Lua 5.1/5.2 compatibility
    lua_pushcfunction(L, luaopen_utf8);
    lua_setfield(L, -2, "utf8");

    // Pop package and preload tables
    lua_pop(L, 2);
    // --- End pre-loading ---
	lua_register_api(L);
	lua_run_init(L);

	// check if being run in command string mode
	if (argc > 2 && strcmp(argv[1], "-c") == 0) {

		// execute the command provided
		char *command = argv[2];
		char *expanded_command = lush_resolve_aliases(command);
		char **commands = lush_split_commands(expanded_command);
		int status = 0;
		char ***args = lush_split_args(commands, &status);

		if (status == -1) {
			fprintf(stderr, "lush: Expected end of quoted string\n");
		} else {
			lush_expand_globs(args);
		}

		if (lush_run(L, args, status) != 0) {
			exit(1);
		}

		// clean up
		for (int i = 0; args[i]; i++) {
			free(args[i]);
		}
		free(args);
		free(commands);
		free(expanded_command);
		lua_close(L);
		return 0;
	}

    // This is the corrected logic for running a script file non-interactively.
    if (argc > 1) {
        char *ext = strrchr(argv[1], '.');
        if (ext && strcmp(ext, ".lua") == 0) {
            const char *script_name = argv[1];
            // The arguments for the script start at argv[2].
            // We create a pointer to that part of the array.
            char **script_args = (argc > 2) ? &argv[2] : NULL;

            // Call the script loader directly with the script and its arguments.
            if (lua_load_script(L, script_name, script_args) != 0) {
                exit(1); // Exit if the script had an error
            }

            lua_close(L); // Clean up and exit
            return 0;
        }
    }

    // --- Interactive Shell Mode ---

	// eat ^C in main
	struct sigaction sa_int;
	sa_int.sa_handler = SIG_IGN;
	sa_int.sa_flags = 0;
	sigemptyset(&sa_int.sa_mask);
	sigaction(SIGINT, &sa_int, NULL);

	struct sigaction sa_bk;
	sa_bk.sa_handler = &background_handler;
	sigemptyset(&sa_bk.sa_mask);
	sa_bk.sa_flags = SA_RESTART;
	sigaction(SIGCHLD, &sa_bk, NULL);

	// set custom envars
	char hostname[256];
	gethostname(hostname, sizeof(hostname));
	setenv("HOSTNAME", hostname, 1);
	char *cwd = getcwd(NULL, 0);
	setenv("OLDPWD", cwd, 1);
	free(cwd);

	int status = 0;
	while (true) {
		// Prompt
		char *prompt = get_prompt();

		printf("%s ", prompt);
		char *line = lush_read_line();
		lush_push_history(line);
		printf("\n");
		if (line == NULL || strlen(line) == 0) {
			free(line);
			continue;
		}
		char *expanded_line = lush_resolve_aliases(line);
		char **commands = lush_split_commands(expanded_line);
		char ***args = lush_split_args(commands, &status);
		if (status == -1) {
			fprintf(stderr, "lush: Expected end of quoted string\n");
		} else {
			lush_expand_globs(args);
		}

		if (lush_run(L, args, status) != 0) {
			exit(1);
		}

		for (int i = 0; args[i]; i++) {
			free(args[i]);
		}
		// add last line to history
		free(prompt);
		free(args);
		free(commands);
		free(expanded_line);
		free(line);
	}
	lua_close(L);
	if (prompt_format != NULL)
		free(prompt_format);
	if (aliases != NULL)
		free(aliases);
	if (alt_shell != NULL)
		free(alt_shell);
	return 0;
}