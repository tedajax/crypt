#include "debug_gui.h"
#include "game_components.h"
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

ecs_world_stats_t world_stats = {0};

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
        TX_ASSERT(curr_size + size < K_CONTEXT_MEM_MAX_SIZE);
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

typedef struct debug_panel_context {
    ecs_query_t* q_debug_windows;
    ECS_DECLARE_COMPONENT(Position);
} debug_panel_context;

struct plot_marker {
    float value;
    ImU32 color;
};

enum { K_PLOT_DESC_MAX_MARKERS = 16 };

struct gauge_plot_desc {
    const ecs_gauge_t* gauge;
    const char* title;
    ImVec2 size;
    float vmin;
    float vmax;
    struct plot_marker markers[K_PLOT_DESC_MAX_MARKERS];
};

void plot_ecs_guage(int32_t t, const struct gauge_plot_desc* desc)
{
    if (!desc) {
        return;
    }

    igText(desc->title);

    const float spacing = desc->size.x / ECS_STAT_WINDOW;

    igBeginChildEx(desc->title, igGetIDStr(desc->title), desc->size, true, ImGuiWindowFlags_None);
    {
        ImDrawList* draw_list = igGetWindowDrawList();

        ImVec2 pos;
        igGetCursorScreenPos(&pos);

        float top = pos.y;
        float bottom = pos.y + desc->size.y;
        float left = pos.x;
        float right = pos.x + desc->size.x;

        for (int32_t m = 0; m < K_PLOT_DESC_MAX_MARKERS; ++m) {
            ImU32 color = desc->markers[m].color;
            if (color == 0) break;

            float value = desc->markers[m].value;
            float y = bottom - inv_lerp(desc->vmin, desc->vmax, value) * desc->size.y;

            ImDrawList_AddLine(draw_list, (ImVec2){left, y}, (ImVec2){right, y}, color, 1.0f);
        }

        for (int i = 0; i < ECS_STAT_WINDOW - 1; ++i) {
            int i0 = (t + i + 1) % ECS_STAT_WINDOW;
            int i1 = (t + i + 2) % ECS_STAT_WINDOW;

            float v0 = desc->gauge->avg[i0];
            float v1 = desc->gauge->avg[i1];

            float x0 = pos.x + i * spacing;
            float y0 = bottom - inv_lerp(desc->vmin, desc->vmax, v0) * desc->size.y;
            float x1 = pos.x + (i + 1) * spacing;
            float y1 = bottom - inv_lerp(desc->vmin, desc->vmax, v1) * desc->size.y;

            ImDrawList_AddLine(draw_list, (ImVec2){x0, y0}, (ImVec2){x1, y1}, 0xFFFFFFFF, 2.0f);
        }
    }
    igEndChild();
}

void debug_panel_gui(ecs_world_t* world, void* ctx)
{
    debug_panel_context* context = (debug_panel_context*)ctx;

    ecs_get_world_stats(world, &world_stats);
    ecs_world_stats_t* stats = &world_stats;

    DEBUG_PANEL_LOAD_COMPONENT(context, Position);
    igLabelText("Entity Count", "%d", ecs_count(world, Position));
    igLabelText("FPS", "%0.0f", stats->fps.max[stats->t]);

    if (!context->q_debug_windows) {
        return;
    }

    igBeginChildEx("Panels", 42, (ImVec2){-1, 200}, true, ImGuiWindowFlags_None);
    {

        ecs_iter_t it = ecs_query_iter(context->q_debug_windows);
        while (ecs_query_next(&it)) {
            DebugWindow* window = ecs_term(&it, DebugWindow, 1);
            for (int32_t i = 0; i < it.count; ++i) {
                if (strcmp(window[i].name, "DebugPanel") == 0) {
                    continue;
                }

                {
                    char buf[128];
                    snprintf(buf, 128, "%s (%s)", window[i].name, window[i].shortcut_str);
                    if (igCheckbox(buf, &window[i].is_visible)) {
                    }
                }
            }
        }
    }
    igEndChild();

    plot_ecs_guage(
        stats->t,
        &(struct gauge_plot_desc){
            .title = "FPS Plot",
            .gauge = &stats->fps,
            .size = (ImVec2){360.f, 100.f},
            .vmin = 0,
            .vmax = 160,
            .markers = {
                [0] = {.value = 144.0f, .color = 0xFFFFFF00},
                [1] = {.value = 120.0f, .color = 0xFF00FF00},
                [2] = {.value = 60.0f, .color = 0xFF00FFFF},
                [3] = {.value = 30.0f, .color = 0xFF0000FF},
            }});

    plot_ecs_guage(
        stats->t,
        &(struct gauge_plot_desc){
            .title = "Frame time Plot",
            .gauge = &stats->frame_time_total.rate,
            .size = (ImVec2){360.f, 100.f},
            .vmin = 0,
            .vmax = 1.f / 15,
            .markers = {
                [0] = {.value = 1.0f / 30, .color = 0xFF0000FF},
                [1] = {.value = 1.0f / 60, .color = 0xFF00FFFF},
                [2] = {.value = 1.0f / 120, .color = 0xFF00FF00},
                [3] = {.value = 1.0f / 144, .color = 0xFFFFFF00},
            }});

    const int bytes_per_block = 64;
    const int total_blocks = K_CONTEXT_MEM_MAX_SIZE / bytes_per_block;
    const float block_size = 16.0f;
    const float separation = 2.0f;
    igBeginChildEx(
        "Debug Gui Contexts",
        43,
        (ImVec2){total_blocks * block_size + (block_size + separation) / 2.0f, block_size * 2.0f},
        true,
        ImGuiWindowFlags_None);
    {
        ImDrawList* draw_list = igGetWindowDrawList();

        ImVec2 pos;
        igGetCursorScreenPos(&pos);

        const ptrdiff_t mem_size = context_mem_head - context_memory;

        pos.x += (block_size + separation) / 2.0f;

        for (int i = 0; i < total_blocks; ++i) {
            float x = i * (block_size + separation) + pos.x;
            float y = pos.y + (block_size + separation) / 2;
            float perc = 0.0f;

            ptrdiff_t curr_block = i * bytes_per_block;
            ptrdiff_t next_block = (i + 1) * bytes_per_block;

            if (curr_block < mem_size && next_block > mem_size) {
                ptrdiff_t in_block = mem_size - curr_block;
                perc = (float)in_block / bytes_per_block;
            } else if (curr_block < mem_size) {
                perc = 1.0f;
            }

            float block_hsize = block_size * 0.5f;
            ImVec2 center = (ImVec2){x, y};
            ImVec2 extents = (ImVec2){block_hsize, block_hsize};
            ImVec2 full_p0 = (ImVec2){center.x - extents.x, center.y - extents.y};
            ImVec2 full_p1 = (ImVec2){center.x + extents.x, center.y + extents.y};

            ImVec2 sub_extents = (ImVec2){extents.x * perc, extents.y * perc};
            ImVec2 sub_p0 = (ImVec2){center.x - sub_extents.x, center.y - sub_extents.y};
            ImVec2 sub_p1 = (ImVec2){center.x + sub_extents.x, center.y + sub_extents.y};

            ImDrawList_AddRectFilled(
                draw_list, full_p0, full_p1, 0xFFFFFF00, 0.0f, ImDrawCornerFlags_None);
            ImDrawList_AddRectFilled(
                draw_list, sub_p0, sub_p1, 0xFFFF00FF, 0.0f, ImDrawCornerFlags_None);
        }
    }
    igEndChild();
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

static void StoreDebugWindowContext(ecs_iter_t* it)
{
    DebugWindow* window = ecs_term(it, DebugWindow, 1);

    for (int32_t i = 0; i < it->count; ++i) {
        window[i].ctx = store_context(window[i].ctx, window[i].ctx_size);

        txinp_parse_shortcut_str(
            window[i].shortcut_str, &window[i].shortcut.mod, &window[i].shortcut.key);
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

    ECS_IMPORT(world, GameComp);

    ecs_query_t* query = ecs_query_new(world, "debug.gui.Window");
    DEBUG_PANEL(
        world,
        DebugPanel,
        "`",
        debug_panel_gui,
        debug_panel_context,
        {
            .q_debug_windows = query,
            DEBUG_PANEL_STORE_COMPONENT(world, Position, "game.comp.Position"),
        });

    ECS_EXPORT_COMPONENT(DebugWindow);
}