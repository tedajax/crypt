#pragma once

#include "flecs.h"
#include "tx_math.h"

typedef vec2 Position;
typedef vec2 Velocity;

typedef struct GameComp {
    ECS_DECLARE_COMPONENT(Position);
    ECS_DECLARE_COMPONENT(Velocity);
} GameComp;

void GameCompImport(ecs_world_t* world);

#define GameCompImportHandles(handles)                                                             \
    ECS_IMPORT_COMPONENT(handles, Position);                                                       \
    ECS_IMPORT_COMPONENT(handles, Velocity);