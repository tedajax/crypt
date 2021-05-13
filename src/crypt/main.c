#include "game_components.h"
#include "sprite_renderer.h"
#include "strhash.h"
#include "system_sdl2.h"
#include "tx_input.h"
#include "tx_math.h"
#include "tx_rand.h"
#include <time.h>

//// COMPONENTS
typedef vec2 Target;

typedef struct TankInput {
    float move;
    bool fire;
} TankInput;

typedef struct Collider {
    vec2 offset;
    vec2 half_extents;
    uint8_t layer; // TEMP
} Collider;

typedef struct ExpireAfter {
    float seconds;
} ExpireAfter;
//// TAGS

//// SYSTEMS
void InvaderMovement(ecs_iter_t* it);
void TankGatherInput(ecs_iter_t* it);
void TankMovement(ecs_iter_t* it);
void ProjectileMovement(ecs_iter_t* it);
void ProjectileView(ecs_iter_t* it);
void ExpireAfterUpdate(ecs_iter_t* it);
void ColliderView(ecs_iter_t* it);

ECS_DECLARE_ENTITY(TankProjectilePrefab);

ECS_COMPONENT_DECLARE(Collider);
ECS_COMPONENT_DECLARE(ExpireAfter);

ECS_TAG_DECLARE(Projectile);

int main(int argc, char* argv[])
{
    txrng_seed((uint32_t)time(NULL));
    strhash_init();

    ecs_world_t* world = ecs_init_w_args(argc, argv);
    ecs_set_target_fps(world, 144.0f);

    ECS_IMPORT(world, GameComp);
    ECS_IMPORT(world, SystemSdl2);
    ECS_IMPORT(world, SpriteRenderer);

    ecs_entity_t window =
        ecs_set(world, 0, WindowConfig, {.title = "crypt", .width = 1920, .height = 1080});

    ecs_entity_t render_config = ecs_set(
        world,
        0,
        SpriteRenderConfig,
        {
            .e_window = window,
            .pixels_per_meter = 8.0f,
            .canvas_width = 256,
            .canvas_height = 144,
        });

    ECS_COMPONENT(world, Target);
    ECS_COMPONENT(world, TankInput);
    ECS_COMPONENT_DEFINE(world, Collider);
    ECS_COMPONENT_DEFINE(world, ExpireAfter);

    ECS_TAG_DEFINE(world, Projectile);

    // clang-format off
    ECS_SYSTEM(world, TankGatherInput, EcsPostLoad, TankInput);
    ECS_SYSTEM(world, TankMovement, EcsOnUpdate, game.comp.Position, game.comp.Velocity, TankInput);
    ECS_SYSTEM(world, InvaderMovement, EcsOnUpdate, game.comp.Position, game.comp.Velocity, Target);
    ECS_SYSTEM(
        world,
        ProjectileMovement,
        EcsOnUpdate,
        game.comp.Position,
        game.comp.Velocity,
        Collider,
        Projectile);
    ECS_SYSTEM(world, ColliderView, EcsPostUpdate, game.comp.Position, ANY:Collider);
    ECS_SYSTEM(world, ProjectileView, EcsOnUpdate, Projectile);
    ECS_SYSTEM(world, ExpireAfterUpdate, EcsOnUpdate, ExpireAfter);
    // clang-format on

    ECS_ENTITY(
        world, Tank, game.comp.Position, game.comp.Velocity, TankInput, sprite.renderer.Sprite);
    ecs_set(world, Tank, Position, {.x = 0.0f, .y = 9.0f});
    ecs_set(world, Tank, Velocity, {.x = 0.0f, .y = 0.0f});
    ecs_set(world, Tank, TankInput, {0});
    ecs_set(world, Tank, Sprite, {.sprite_id = 0, .layer = 1.0f, .origin = (vec2){0.5f, 1.0f}});
    ecs_set(world, Tank, SpriteSize, {.width = 2, .height = 1});

    ECS_PREFAB(world, InvaderPrefab, sprite.renderer.Sprite, Collider);
    ecs_set(
        world,
        InvaderPrefab,
        Sprite,
        {.sprite_id = 2, .layer = 2.0f, .origin = (vec2){0.5f, 0.5f}});
    ecs_set(world, InvaderPrefab, Collider, {.half_extents = {.x = 0.5f, .y = 0.5f}, .layer = 1});

    for (int i = 0; i < 25; ++i) {
        ecs_entity_t invader = ecs_new_w_pair(world, EcsIsA, InvaderPrefab);
        ecs_set(world, invader, Position, {.x = txrng_rangef(-16, 16), .y = txrng_rangef(-9, 6)});
        ecs_set(world, invader, Velocity, {.x = 0.0f, .y = 0.0f});
        ecs_set(world, invader, Target, {.x = txrng_rangef(-16, 16), .y = txrng_rangef(-9, 6)});
    }

    ECS_PREFAB(world, TankProjectilePrefab, Projectile);

    while (ecs_progress(world, 0.0f)) {
    }

    return ecs_fini(world);
}

void InvaderMovement(ecs_iter_t* it)
{
    Position* position = ecs_column(it, Position, 1);
    Velocity* velocity = ecs_column(it, Velocity, 2);
    Target* target = ecs_column(it, Target, 3);

    draw_set_prim_layer(10.0f);
    draw_line_col((vec2){-15, -8}, (vec2){15, 8}, (vec4){1, 0, 0, 1});
    draw_set_prim_layer(5.0f);
    draw_rect_col((vec2){-5, -5}, (vec2){5, 5}, (vec4){1, 1, 0, 1});
    draw_set_prim_layer(0.0f);
    draw_rect_col((vec2){-2, -3}, (vec2){3, 2}, (vec4){1, 0, 1, 1});

    for (int i = 0; i < it->count; ++i) {
        vec2 delta = vec2_sub(target[i], position[i]);
        vec2 normDir = vec2_norm(delta);
        float dist = vec2_len(delta);
        float accel = 10.0f;

        if (vec2_len(velocity[i]) > 0.0f) {
            vec2 normVel = vec2_norm(velocity[i]);
            if (vec2_dot(normDir, normVel) < 0.0f && dist > 1.0f) {
                accel *= 2.0f;
            }
        }

        vec2 vel = vec2_add(velocity[i], vec2_scale(normDir, it->delta_time * accel));
        if (vec2_len(vel) > 25.0f) {
            vel = vec2_scale(vec2_norm(vel), 25.0f);
        }

        velocity[i] = vel;
        position[i] = vec2_add(position[i], vec2_scale(velocity[i], it->delta_time));
    }
}

void TankGatherInput(ecs_iter_t* it)
{
    TankInput* input = ecs_column(it, TankInput, 1);

    for (int32_t i = 0; i < it->count; ++i) {
        float move = 0.0f;

        if (txinp_get_key(TXINP_KEY_LEFT)) move -= 1.0f;
        if (txinp_get_key(TXINP_KEY_RIGHT)) move += 1.0f;

        input[i].move = move;
        input[i].fire = txinp_get_key_down(TXINP_KEY_Z);
    }
}

void TankMovement(ecs_iter_t* it)
{
    ECS_COLUMN_COMPONENT(it, Position, 1);
    ECS_COLUMN_COMPONENT(it, Velocity, 2);

    Position* position = ecs_column(it, Position, 1);
    Velocity* velocity = ecs_column(it, Velocity, 2);
    TankInput* input = ecs_column(it, TankInput, 3);

    for (int32_t i = 0; i < it->count; ++i) {
        position[i].x += input[i].move * 32.0f * it->delta_time;

        if (input[i].fire) {
            ecs_entity_t projectile = ecs_new(it->world, Projectile);
            ecs_set(
                it->world, projectile, Position, {.x = position[i].x, .y = position[i].y - 1.0f});
            ecs_set(it->world, projectile, Velocity, {.x = 0.0f, .y = -32.0f});
            ecs_set(it->world, projectile, Collider, {.half_extents = {.x = 0.125f, .y = 0.25f}});
            ecs_set(it->world, projectile, ExpireAfter, {.seconds = 1.0f});
        }
    }
}

void ProjectileMovement(ecs_iter_t* it)
{
    Position* position = ecs_column(it, Position, 1);
    Velocity* velocity = ecs_column(it, Velocity, 2);

    for (int32_t i = 0; i < it->count; ++i) {
        vec2 vel = vec2_scale(velocity[i], it->delta_time);
        position[i] = vec2_add(position[i], vel);
    }
}

void ColliderView(ecs_iter_t* it)
{
    Position* position = ecs_column(it, Position, 1);
    Collider* collider = ecs_column(it, Collider, 2);

    vec4 cols[2] = {
        (vec4){0.0f, 1.0f, 1.0f, 1.0f},
        (vec4){1.0f, 0.0f, 1.0f, 1.0f},
    };

    for (int32_t i = 0; i < it->count; ++i) {
        vec2 half_extents;
        int8_t coll_layer;
        if (ecs_is_owned(it, 2)) {
            half_extents = collider[i].half_extents;
            coll_layer = collider[i].layer;
        } else {
            half_extents = collider->half_extents;
            coll_layer = collider->layer;
        }

        vec2 p0 = vec2_sub(position[i], half_extents);
        vec2 p1 = vec2_add(position[i], half_extents);

        draw_line_rect_col(p0, p1, cols[coll_layer]);
    }
}

void ProjectileView(ecs_iter_t* it)
{
    vec4 cols[4];
    for (int c = 0; c < 4; ++c) {
        cols[c] = (vec4){.y = c / 4.0f, .z = 1.0f, .w = 1.0f};
    }

    vec2 base = (vec2){-16, -9};
    vec2 size = (vec2){0.5f, 0.5f};

    for (int32_t i = 0; i < it->count; ++i) {
        vec2 p0 = vec2_add(base, vec2_scale((vec2){0.5f, 0}, (float)i));
        vec2 p1 = vec2_add(p0, size);
        draw_rect_col4(p0, p1, cols);
    }
}

void ExpireAfterUpdate(ecs_iter_t* it)
{
    ExpireAfter* expire = ecs_column(it, ExpireAfter, 1);

    for (int32_t i = 0; i < it->count; ++i) {
        expire[i].seconds -= it->delta_time;
        if (expire[i].seconds <= 0) {
            ecs_delete(it->world, it->entities[i]);
        }
    }
}