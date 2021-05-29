#pragma once

#include "flecs.h"
#include "tx_input.h"

typedef void (*debug_window_fn_t)(ecs_world_t* world, void* ctx);

typedef struct DebugWindow {
    const char* name;
    const char* shortcut_str;
    struct {
        txinp_key key;
        txinp_mod mod;
    } shortcut;
    bool is_visible;
    bool is_open;
    debug_window_fn_t window_fn;
    void* ctx;
    size_t ctx_size;
} DebugWindow;

typedef struct DebugGui {
    ECS_DECLARE_COMPONENT(DebugWindow);
} DebugGui;

void DebugGuiImport(ecs_world_t* world);

#define DebugGuiImportHandles(handles) ECS_IMPORT_COMPONENT(handles, DebugWindow);

#define DEBUG_PANEL(world, entity, shortcut, func, ctx_type, ...)                                  \
    ECS_ENTITY(world, entity##DebugGui, debug.gui.Window);                                         \
    ecs_set(                                                                                       \
        world,                                                                                     \
        entity##DebugGui,                                                                          \
        DebugWindow,                                                                               \
        {                                                                                          \
            .name = #entity,                                                                       \
            .window_fn = func,                                                                     \
            .shortcut_str = shortcut,                                                              \
            .ctx = &(ctx_type)__VA_ARGS__,                                                         \
            .ctx_size = sizeof(ctx_type),                                                          \
        });
