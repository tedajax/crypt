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

typedef struct Sprite {
    uint16_t sprite_id;
} Sprite;

typedef struct SpriteFlags {
    uint16_t flags;
} SpriteFlags;

typedef struct SpriteSize {
    uint8_t width;
    uint8_t height;
} SpriteSize;

typedef struct SpriteRenderConfig {
    float pixels_per_meter;
    uint32_t canvas_width;
    uint32_t canvas_height;
    ecs_entity_t e_window;
} SpriteRenderConfig;

typedef struct SpriteRenderer {
    ECS_DECLARE_COMPONENT(Sprite);
    ECS_DECLARE_COMPONENT(SpriteFlags);
    ECS_DECLARE_COMPONENT(SpriteSize);
    ECS_DECLARE_COMPONENT(SpriteRenderConfig);
} SpriteRenderer;

void SpriteRendererImport(ecs_world_t* world);

#define SpriteRendererImportHandles(handles)                                                       \
    ECS_IMPORT_COMPONENT(handles, Sprite);                                                         \
    ECS_IMPORT_COMPONENT(handles, SpriteFlags);                                                    \
    ECS_IMPORT_COMPONENT(handles, SpriteSize);                                                     \
    ECS_IMPORT_COMPONENT(handles, SpriteRenderConfig);
