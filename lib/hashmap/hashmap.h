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

#ifndef HASHMAP_H
#define HASHMAP_H

typedef struct pair {
    char *key;
    char *val;
    struct pair *next;
} map_pair_t;

typedef struct {
    map_pair_t **list;
    unsigned int cap;
    unsigned int len;
} hashmap_t;

hashmap_t *hm_new_hashmap();
unsigned int hm_hashcode(hashmap_t *this, char *key);
char *hm_get(hashmap_t *this, char *key);
void hm_set(hashmap_t *this, char *key, char *val);

#endif // HASHMAP_H
