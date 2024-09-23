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

#ifndef LUSH_H
#define LUSH_H

#include <lua.h>

// alias
void lush_add_alias(const char *alias, const char *command);
char *lush_get_alias(char *alias);
char *lush_resolve_aliases(char *line);

// builtins
extern char *builtin_strs[];
extern char *builtin_usage[];

extern int (*builtin_func[])(lua_State *, char ***);

int lush_cd(lua_State *L, char ***args);
int lush_help(lua_State *L, char ***args);
int lush_exit(lua_State *L, char ***args);
int lush_time(lua_State *L, char ***args);
int lush_lua(lua_State *L, char ***args);

int lush_num_builtins();

int lush_run(lua_State *L, char ***commands, int num_commands);

char *lush_read_line();
char **lush_split_commands(char *line);
char ***lush_split_args(char **commands, int *status);

int lush_execute_command(char **args, int input_fd, int output_fd);
int lush_execute_pipeline(char ***commands, int num_commands);
int lush_execute_chain(lua_State *L, char ***commands, int num_commands);

void lush_format_prompt(const char *prompt_format);

// format spec for the prompt
extern char *prompt_format;

// history
char *lush_get_past_command(int pos);
void lush_push_history(const char* line);

#endif // LUSH_H
