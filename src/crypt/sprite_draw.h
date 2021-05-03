#pragma once

#include "flecs.h"
#include "sokol_gfx.h"
#include "tx_math.h"
#include "tx_types.h"

typedef enum sprite_flags {
    SpriteFlags_None = 0,
    SpriteFlags_FlipX = 1,
    SpriteFlags_FlipY = 2,
} sprite_flags;

typedef struct TdjxSprite {
    uint16_t sprite_id;
} TdjxSprite;

typedef struct TdjxSpriteFlags {
    uint16_t flags;
} TdjxSpriteFlags;

typedef struct TdjxSpriteSize {
    uint8_t width;
    uint8_t height;
} TdjxSpriteSize;

typedef struct SpriteRenderConfig {
    float pixels_per_meter;
    uint32_t canvas_width;
    uint32_t canvas_height;
    ecs_entity_t e_window;
} SpriteRenderConfig;

typedef struct TdjxSpriteRenderer {
    ECS_DECLARE_COMPONENT(TdjxSprite);
    ECS_DECLARE_COMPONENT(TdjxSpriteFlags);
    ECS_DECLARE_COMPONENT(TdjxSpriteSize);
    ECS_DECLARE_COMPONENT(SpriteRenderConfig);
} TdjxSpriteRenderer;

vec4 spr_calc_rect(uint32_t sprite_id, sprite_flags flip, uint16_t sw, uint16_t sh);

void TdjxSpriteRendererImport(ecs_world_t* world);

#define TdjxSpriteRendererImportHandles(handles)                                                   \
    ECS_IMPORT_COMPONENT(handles, TdjxSprite);                                                     \
    ECS_IMPORT_COMPONENT(handles, TdjxSpriteFlags);                                                \
    ECS_IMPORT_COMPONENT(handles, TdjxSpriteSize);                                                 \
    ECS_IMPORT_COMPONENT(handles, SpriteRenderConfig);
