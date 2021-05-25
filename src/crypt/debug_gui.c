#include "debug_gui.h"
#include "stb_ds.h"
#include "strhash.h"
#include "system_imgui.h"
#include <ccimgui.h>

struct mem_block {
    void* mem;
    size_t size;
    struct mem_block* next;
};

// this free list is absolutely unnecessary for the usage xpected here but I can't help myself.
struct mem_block* free_list = NULL;

enum { K_CONTEXT_MEM_MAX_SIZE = 1024 };
uint8_t context_memory[K_CONTEXT_MEM_MAX_SIZE];
uint8_t* context_mem_head = context_memory;

void* store_context(void* ctx, size_t size)
{
    if (!ctx || !size) {
        return NULL;
    }

    struct mem_block* find = NULL;
    struct mem_block* block = free_list;
    struct mem_block* prev = NULL;
    while (block) {
        if (block->size >= size) {
            find = block;
            break;
        } else {
            prev = block;
            block = block->next;
        }
    }

    uint8_t* storage = NULL;
    if (!find) {
        ptrdiff_t curr_size = context_mem_head - context_memory;
        if (curr_size + size >= K_CONTEXT_MEM_MAX_SIZE) {
            return NULL;
        }
        storage = context_mem_head;
    } else {
        storage = find->mem;

        if (prev) {
            prev->next = find->next;
        } else {
            free_list = find->next;
        }

        free(find);
    }

    memcpy(storage, ctx, size);
    context_mem_head += size;

    return storage;
}

void unload_context(void* ctx, size_t size)
{
    if (!ctx || !size) {
        return;
    }

    struct mem_block* block = (struct mem_block*)calloc(1, sizeof(struct mem_block));
    if (!free_list) {
        free_list = block;
    } else {
        struct mem_block* slider = free_list;
        while (slider->next) {
            slider = slider->next;
        }
        slider->next = block;
    }
}

static void UpdateDebugWindows(ecs_iter_t* it)
{
    DebugWindow* window = ecs_term(it, DebugWindow, 1);

    for (int32_t i = 0; i < it->count; ++i) {
        if (window[i].shortcut.key && txinp_get_key_down(window[i].shortcut.key)
            && txinp_mods_down(window[i].shortcut.mod))
        {
            window[i].is_visible = !window[i].is_visible;
            if (window[i].is_visible) window[i].is_open = true;
        }

        if (window[i].is_visible) {
            if (igBegin(window[i].name, &window[i].is_open, ImGuiWindowFlags_None)) {
                if (window[i].window_fn) {
                    window[i].window_fn(it->world, window[i].ctx);
                }
            }
            igEnd();
        }
    }
}

typedef struct debug_panel_context {
    ecs_query_t* q_debug_windows;
} debug_panel_context;

void debug_panel_gui(ecs_world_t* world, void* ctx)
{
    debug_panel_context* context = (debug_panel_context*)ctx;

    if (!context->q_debug_windows) {
        return;
    }

    ecs_iter_t it = ecs_query_iter(context->q_debug_windows);

    while (ecs_query_next(&it)) {
        DebugWindow* window = ecs_term(&it, DebugWindow, 1);
        for (int32_t i = 0; i < it.count; ++i) {
            if (strcmp(window[i].name, "Debug Panel") == 0) {
                continue;
            }
            if (igButton(window[i].name, (ImVec2){120, 20})) {
                window[i].is_visible = !window[i].is_visible;
            }
        }
    }
}

static void StoreDebugWindowContext(ecs_iter_t* it)
{
    DebugWindow* window = ecs_term(it, DebugWindow, 1);

    for (int32_t i = 0; i < it->count; ++i) {
        window[i].ctx = store_context(window[i].ctx, window[i].ctx_size);
    }
}

static void UnloadDebugWindowContext(ecs_iter_t* it)
{
    DebugWindow* window = ecs_term(it, DebugWindow, 1);

    for (int32_t i = 0; i < it->count; ++i) {
        unload_context(window[i].ctx, window[i].ctx_size);
    }
}

void DebugGuiImport(ecs_world_t* world)
{
    ECS_MODULE(world, DebugGui);

    ecs_set_name_prefix(world, "Debug");

    ECS_COMPONENT(world, DebugWindow);

    ECS_SYSTEM(world, StoreDebugWindowContext, EcsOnSet, DebugWindow);
    ECS_SYSTEM(world, UnloadDebugWindowContext, EcsUnSet, DebugWindow);
    ECS_SYSTEM(world, UpdateDebugWindows, EcsPreStore, DebugWindow);

    ECS_ENTITY(world, DebugPanel, DebugWindow);

    ecs_query_t* query = ecs_query_new(world, "debug.gui.Window");
    ecs_set(
        world,
        DebugPanel,
        DebugWindow,
        {
            .name = "Debug Panel",
            .shortcut =
                {
                    .key = TXINP_KEY_GRAVE,
                },
            .window_fn = debug_panel_gui,
            .ctx =
                &(debug_panel_context){
                    .q_debug_windows = query,
                },
            .ctx_size = sizeof(debug_panel_context),
        });

    ECS_EXPORT_COMPONENT(DebugWindow);
}