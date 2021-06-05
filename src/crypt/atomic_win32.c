#include "flecs.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

int win32_atomic_inc(int32_t* value)
{
    return (int)InterlockedIncrement((LONG*)value);
}

int win32_atomic_dec(int32_t* value)
{
    return (int)InterlockedDecrement((LONG*)value);
}

void set_ecs_os_atomic_api_win32(void)
{
    ecs_os_api.ainc_ = win32_atomic_inc;
    ecs_os_api.adec_ = win32_atomic_dec;
}