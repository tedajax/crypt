#pragma once

#include <stdint.h>

typedef struct str_id {
    uint64_t value;
} str_id;

extern str_id str_id_empty;
extern str_id str_id_invalid;

void str_id_init();
void str_id_term();
str_id str_id_store(const char* str);
str_id str_id_store_w_len(const char* str, int len);
void str_id_release(str_id id);
const char* str_id_cstr(str_id id);