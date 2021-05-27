#pragma once

#include "flecs.h"
#include "tx_math.h"

typedef struct BoxCollider {
    uint8_t layer; // TEMP
    vec2 size;
} BoxCollider;

typedef struct ColliderQuery {
    const char* expr;
    ecs_query_t* q_targets;
} ColliderQuery;

typedef struct Physics {
    ECS_DECLARE_COMPONENT(BoxCollider);
    ECS_DECLARE_COMPONENT(ColliderQuery);
} Physics;

void PhysicsImport(ecs_world_t* world);

#define PhysicsImportHandles(handles)                                                              \
    ECS_IMPORT_COMPONENT(handles, BoxCollider);                                                    \
    ECS_IMPORT_COMPONENT(handles, ColliderQuery);