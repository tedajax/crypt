#include "thread_api_stdc11.h"
#include "flecs.h"
#include "stb_ds.h"
#include "tinycthread.h"

#define INVALID_HANDLE -1
#define handle_is_valid(handle) (handle != INVALID_HANDLE)

mtx_t* mutex_storage = NULL;
ecs_os_mutex_t* mutex_free_stack = NULL;
cnd_t* cond_storage = NULL;
ecs_os_cond_t* cond_free_stack = NULL;

ecs_os_mutex_t stdc11_mutex_new(void)
{
    ecs_os_mutex_t mutex = INVALID_HANDLE;
    if (arrlen(mutex_free_stack) > 0) {
        mutex = arrpop(mutex_free_stack);
    } else {
        arrput(mutex_storage, ((mtx_t){0}));
        mutex = arrlen(mutex_storage) - 1;
    }

    mtx_t* mtx = &mutex_storage[mutex];
    if (mtx_init(mtx, mtx_plain) == thrd_success) {
        return mutex;
    } else {
        return INVALID_HANDLE;
    }
}

void stdc11_mutex_free(ecs_os_mutex_t mutex)
{
    ecs_assert(handle_is_valid(mutex), ECS_INVALID_PARAMETER, "mutex");

    arrput(mutex_free_stack, mutex);

    mtx_t* mtx = &mutex_storage[mutex];
    mtx_destroy(mtx);
}

void stdc11_mutex_lock(ecs_os_mutex_t mutex)
{
    ecs_assert(handle_is_valid(mutex), ECS_INVALID_PARAMETER, "mutex");

    mtx_t* mtx = &mutex_storage[mutex];
    mtx_lock(mtx);
}

void stdc11_mutex_unlock(ecs_os_mutex_t mutex)
{
    ecs_assert(handle_is_valid(mutex), ECS_INVALID_PARAMETER, "mutex");

    mtx_t* mtx = &mutex_storage[mutex];
    mtx_unlock(mtx);
}

ecs_os_cond_t stdc11_cond_new(void)
{
    ecs_os_cond_t cond = INVALID_HANDLE;
    if (arrlen(cond_free_stack) > 0) {
        cond = arrpop(cond_free_stack);
    } else {
        arrput(cond_storage, ((cnd_t){0}));
        cond = arrlen(cond_storage) - 1;
    }

    cnd_t* cnd = &cond_storage[cond];
    if (cnd_init(cnd) == thrd_success) {
        return cond;
    } else {
        return INVALID_HANDLE;
    }
}

void stdc11_cond_free(ecs_os_cond_t cond)
{
    ecs_assert(handle_is_valid(cond), ECS_INVALID_PARAMETER, "cond");

    arrput(cond_free_stack, cond);

    cnd_t* cnd = &cond_storage[cond];
    cnd_destroy(cnd);
}

void stdc11_cond_wait(ecs_os_cond_t cond, ecs_os_mutex_t mutex)
{
    ecs_assert(handle_is_valid(cond), ECS_INVALID_PARAMETER, "cond");
    ecs_assert(handle_is_valid(mutex), ECS_INVALID_PARAMETER, "mutex");

    cnd_t* cnd = &cond_storage[cond];
    mtx_t* mtx = &mutex_storage[mutex];

    cnd_wait(cnd, mtx);
}

void stdc11_cond_signal(ecs_os_cond_t cond)
{
    ecs_assert(handle_is_valid(cond), ECS_INVALID_PARAMETER, "cond");
    cnd_t* cnd = &cond_storage[cond];
    cnd_signal(cnd);
}

void stdc11_cond_broadcast(ecs_os_cond_t cond)
{
    ecs_assert(handle_is_valid(cond), ECS_INVALID_PARAMETER, "cond");
    cnd_t* cnd = &cond_storage[cond];
    cnd_broadcast(cnd);
}

ecs_os_thread_t stdc11_thread_new(ecs_os_thread_callback_t callback, void* param)
{
    thrd_t thrd;
    thrd_create(&thrd, (thrd_start_t)callback, param);
    return (ecs_os_thread_t)thrd;
}

void* stdc11_thread_join(ecs_os_thread_t thread)
{
    int result = 0;
    thrd_join((thrd_t)thread, &result);
    return (void*)(ptrdiff_t)result;
}

void set_ecs_os_thread_api_stdc11(void)
{
    arrsetcap(mutex_storage, 256);
    arrsetcap(mutex_free_stack, 256);
    arrsetcap(cond_storage, 256);
    arrsetcap(cond_free_stack, 256);

    ecs_os_api.mutex_new_ = stdc11_mutex_new;
    ecs_os_api.mutex_free_ = stdc11_mutex_free;
    ecs_os_api.mutex_lock_ = stdc11_mutex_lock;
    ecs_os_api.mutex_unlock_ = stdc11_mutex_unlock;
    ecs_os_api.cond_new_ = stdc11_cond_new;
    ecs_os_api.cond_free_ = stdc11_cond_free;
    ecs_os_api.cond_wait_ = stdc11_cond_wait;
    ecs_os_api.cond_signal_ = stdc11_cond_signal;
    ecs_os_api.cond_broadcast_ = stdc11_cond_broadcast;
    ecs_os_api.thread_new_ = stdc11_thread_new;
    ecs_os_api.thread_join_ = stdc11_thread_join;
}