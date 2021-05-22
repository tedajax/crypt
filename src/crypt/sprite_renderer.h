#pragma once

#include "flecs.h"
#include "sokol_gfx.h"
#include "tx_math.h"
#include "tx_types.h"

void draw_line(vec2 from, vec2 to);
void draw_line_col(vec2 from, vec2 to, vec4 col);
void draw_line_col2(vec2 from, vec2 to, vec4 col0, vec4 col1);

void draw_rect_col4(vec2 p0, vec2 p1, vec4 cols[4]);
void draw_rect_col(vec2 p0, vec2 p1, vec4 col);
void draw_vgrad(vec2 p0, vec2 p1, vec4 top, vec4 bot);
void draw_hgrad(vec2 p0, vec2 p1, vec4 left, vec4 right);

void draw_line_rect_col4(vec2 p0, vec2 p1, vec4 cols[4]);
void draw_line_rect_col(vec2 p0, vec2 p1, vec4 col);

void draw_set_prim_layer(float layer);
void draw_reset_prim_layer();

typedef enum sprite_flags {
    SpriteFlags_None = 0,
    SpriteFlags_FlipX = 1,
    SpriteFlags_FlipY = 2,
} sprite_flags;

typedef struct Sprite {
    vec2 origin;
    float layer;
    uint16_t sprite_id;
    uint16_t flags;
    uint8_t width;
    uint8_t height;
} Sprite;

typedef struct SpriteRenderConfig {
    float pixels_per_meter;
    uint32_t canvas_width;
    uint32_t canvas_height;
    ecs_entity_t e_window;
} SpriteRenderConfig;

typedef struct SpriteRenderer {
    ECS_DECLARE_COMPONENT(Sprite);
    ECS_DECLARE_COMPONENT(SpriteRenderConfig);
} SpriteRenderer;

void SpriteRendererImport(ecs_world_t* world);

#define SpriteRendererImportHandles(handles)                                                       \
    ECS_IMPORT_COMPONENT(handles, Sprite);                                                         \
    ECS_IMPORT_COMPONENT(handles, SpriteRenderConfig);
