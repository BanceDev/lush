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

#include "hashmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

hashmap_t *hm_new_hashmap() {
	hashmap_t *this = malloc(sizeof(hashmap_t));
	this->cap = 8;
	this->len = 0;
	// null all pointers in list
	this->list = calloc((this->cap), sizeof(map_pair_t *));
	return this;
}

unsigned int hm_hashcode(hashmap_t *this, char *key) {
	unsigned int code;
	for (code = 0; *key != '\0'; key++) {
		code = *key + 31 * code;
	}

	return code % (this->cap);
}

char *hm_get(hashmap_t *this, char *key) {
	map_pair_t *current;
	for (current = this->list[hm_hashcode(this, key)]; current;
		 current = current->next) {
		if (strcmp(current->key, key) == 0) {
			return current->val;
		}
	}
	// the key is not found
	return NULL;
}

void hm_set(hashmap_t *this, char *key, char *val) {
	unsigned int idx = hm_hashcode(this, key);
	map_pair_t *current;
	for (current = this->list[idx]; current; current = current->next) {
		if (strcmp(current->key, key) == 0) {
			current->val = val;
			return;
		}
	}

	map_pair_t *p = malloc(sizeof(map_pair_t));
	p->key = key;
	p->val = val;
	p->next = this->list[idx];
	this->list[idx] = p;
	this->len++;
}
