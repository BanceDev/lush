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

#include "history.h"
#include <linux/limits.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_LINES 512

static char *get_history_path() {
	uid_t uid = getuid();
	struct passwd *pw = getpwuid(uid);
	if (!pw) {
		perror("retrieve home dir");
		return NULL;
	}

	size_t path_length = strlen(pw->pw_dir) + strlen("/.lush/.history") + 1;
	char *path = malloc(path_length);
	if (!path) {
		perror("malloc");
		return NULL;
	}

	strcpy(path, pw->pw_dir);
	strcat(path, "/.lush/.history");

	return path;
}

char *lush_get_past_command(int pos) {
	char *path = get_history_path();
	if (access(path, F_OK) != 0) {
		// file does not exist
		perror("history file check");
		free(path);
		return NULL;
	}

	// read file from the bottom
	FILE *fp = fopen(path, "r");
	if (!fp) {
		perror("opening file");
		free(path);
		return NULL;
	}
	char *line = (char *)malloc(1024 * sizeof(char));
	if (line == NULL) {
		perror("malloc");
		free(path);
		return NULL;
	}
	int curr_line = 0;
	while (fgets(line, 1024, fp)) {
		if (curr_line == pos) {
			fclose(fp);
			return line;
		}
		curr_line++;
	}

	fclose(fp);
	free(path);
	return line;
}

void lush_push_history(const char *line) {
	char *path = get_history_path();
	// check if the file exists and create if not
	if (access(path, F_OK) != 0) {
		FILE *fp = fopen(path, "w");
		if (fp == NULL) {
			perror("creating file");
		} else {
			fclose(fp);
		}
	}

	FILE *fp = fopen(path, "r");
	if (fp == NULL) {
		perror("opening file");
		free(path);
		return;
	}

	// Count lines
	int line_count = 0;
	char buffer[1024]; // Adjust buffer size as needed
	while (fgets(buffer, sizeof(buffer), fp)) {
		line_count++;
	}

	// Allocate memory for file content
	fseek(fp, 0, SEEK_SET);
	char **lines = (char **)malloc(line_count * sizeof(char *));
	if (lines == NULL) {
		perror("malloc");
		fclose(fp);
		free(path);
		return;
	}

	// Read lines into memory
	int index = 0;
	while (fgets(buffer, sizeof(buffer), fp)) {
		lines[index] = strdup(buffer); // Allocate and copy each line
		if (lines[index] == NULL) {
			perror("strdup");
			while (index > 0)
				free(lines[--index]);
			free(lines);
			free(path);
			fclose(fp);
			return;
		}
		index++;
	}
	fclose(fp);

	// Open file for writing
	fp = fopen(path, "w");
	if (fp == NULL) {
		perror("opening file");
		for (int i = 0; i < index; i++)
			free(lines[i]);
		free(lines);
		free(path);
		return;
	}

	// Write the new line
	fprintf(fp, "%s\n", line);

	// Write the last MAX_LINES lines
	int total_lines = line_count < MAX_LINES ? line_count : MAX_LINES;
	for (int i = 0; i < total_lines; i++) {
		fprintf(fp, "%s", lines[i]);
		free(lines[i]); // Free each line after writing
	}

	// Clean up
	free(lines);
	free(path);
	fclose(fp);
}
