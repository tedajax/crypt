#include "str_id.h"
#include "string.h"
#include "strpool.h"

static strpool_t pool;

str_id str_id_empty;
str_id str_id_invalid = {-1};

void str_id_init()
{
    strpool_init(&pool, NULL);
    str_id_empty = str_id_store("");
}

void str_id_term()
{
    strpool_term(&pool);
}

str_id str_id_store(const char* str)
{
    return str_id_store_w_len(str, (int)strlen(str));
}

str_id str_id_store_w_len(const char* str, int len)
{
    return (str_id){
        .value = strpool_inject(&pool, str, len),
    };
}

void str_id_release(str_id id)
{
    strpool_discard(&pool, (uint64_t)id.value);
}

const char* str_id_cstr(str_id id)
{
    return strpool_cstr(&pool, (uint64_t)id.value);
}
