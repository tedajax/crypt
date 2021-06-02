#pragma once

#include "flecs.h"
#include "tx_math.h"

typedef void (*entity_contact_action_t)(ecs_world_t* world, ecs_entity_t self, ecs_entity_t other);

typedef struct PhysReceiver {
    entity_contact_action_t on_contact_start;
    entity_contact_action_t on_contact_continue;
    entity_contact_action_t on_contact_stop;
    ecs_filter_t filter;
} PhysReceiver;

typedef struct PhysCollider {
    uint8_t layer;
} PhysCollider;

typedef struct PhysBox {
    vec2 size;
} PhysBox;

typedef struct PhysWorldBounds {
    float left, right;
    float top, bottom;
} PhysWorldBounds;

typedef struct Physics {
    ECS_DECLARE_COMPONENT(PhysReceiver);
    ECS_DECLARE_COMPONENT(PhysCollider);
    ECS_DECLARE_COMPONENT(PhysBox);
    ECS_DECLARE_COMPONENT(PhysWorldBounds);
} Physics;

void PhysicsImport(ecs_world_t* world);

#define PhysicsImportHandles(handles)                                                              \
    ECS_IMPORT_COMPONENT(handles, PhysReceiver);                                                   \
    ECS_IMPORT_COMPONENT(handles, PhysCollider);                                                   \
    ECS_IMPORT_COMPONENT(handles, PhysBox);                                                        \
    ECS_IMPORT_COMPONENT(handles, PhysWorldBounds);
