#pragma once

#include "flecs.h"
#include "tx_math.h"

typedef struct TdjxSprite {
    vec3 pos;
    vec4 rect;
    vec2 origin;
    vec2 scale;
} TdjxSprite;

typedef struct TdjxSystemsSprites {
    int dummy;
} TdjxSystemsSprites;

void TdjxSystemsSpritesImport(ecs_world_t* world);

#define TdjxSystemsSpritesImportHandles(handles)
