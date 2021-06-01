#pragma once

#ifdef _MSC_VER
#define DEBUG_BREAK __debugbreak();
#else
#include <signal.h>
#define DEBUG_BREAK raise(SIGTRAP);
#endif

typedef struct ecs_world_t ecs_world_t;

void _tx_internal_print_assert(const char* file, int line, const char* expression);
void ecs_logf(ecs_world_t* world, const char* fmt, ...);