#include "tx_system.h"
#include "flecs.h"
#include <stdio.h>

void _tx_internal_print_assert(const char* filename, int line, const char* expression)
{
    printf("[ASSERTION FAILED] (%s) : %s#%d\n", expression, filename, line);
}

void ecs_logf(ecs_world_t* world, const char* fmt, ...)
{
    char buffer[512];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, 512, fmt, args);
    va_end(args);

    const ecs_world_info_t* info = ecs_get_world_info(world);
    printf("%04.2fs | %s\n", info->world_time_total, buffer);
}