#include "sprite_draw.h"
#include "system_sdl2.h"
#include "system_window_sdl2.h"
#include "tx_math.h"
#include "tx_rand.h"
#include <time.h>

//// COMPONENTS
typedef vec2 Position;
typedef vec2 Velocity;
typedef vec2 Target;

typedef struct SpriteDraw {
    uint32_t sprite_id;
    sprite_flip flip;
    uint16_t swidth;
    uint16_t sheight;
} SpriteDraw;

//// TAGS

//// SYSTEMS
void PushSpritesToRenderQueue(ecs_iter_t* it);
void InvaderMovement(ecs_iter_t* it);

int main(int argc, char* argv[])
{
    txrng_seed((uint32_t)time(NULL));

    ecs_world_t* world = ecs_init_w_args(argc, argv);
    ecs_set_target_fps(world, 144.0f);

    ECS_IMPORT(world, SystemSdl2);
    ECS_IMPORT(world, SystemSdl2Window);

    ecs_entity_t window =
        ecs_set(world, 0, WindowDesc, {.title = "crypt", .width = 1280, .height = 720});

    spr_init();

    ECS_COMPONENT(world, Position);
    ECS_COMPONENT(world, Velocity);
    ECS_COMPONENT(world, Target);
    ECS_COMPONENT(world, SpriteDraw);

    ECS_SYSTEM(world, PushSpritesToRenderQueue, EcsOnStore, Position, SpriteDraw);
    ECS_SYSTEM(world, InvaderMovement, EcsOnUpdate, Position, Velocity, Target);

    ECS_ENTITY(world, PlayerEnt, Position, SpriteDraw);
    ecs_set(world, PlayerEnt, Position, {.x = 0.0f, .y = 0.0f});
    ecs_set(world, PlayerEnt, SpriteDraw, {.sprite_id = 0, .swidth = 2});

    ECS_ENTITY(world, PlayerEnt2, Position, SpriteDraw);
    ecs_set(world, PlayerEnt2, Position, {.x = 4.0f, .y = 2.0f});
    ecs_set(world, PlayerEnt2, SpriteDraw, {.sprite_id = 2, .swidth = 1});

    ECS_PREFAB(world, InvaderPrefab, Position, Velocity, SpriteDraw, Target);
    ecs_set(world, InvaderPrefab, SpriteDraw, {.sprite_id = 2});

    for (int i = 0; i < 1000; ++i) {
        ecs_entity_t invader = ecs_new_w_pair(world, EcsIsA, InvaderPrefab);
        Position pos = (Position){.x = txrng_rangef(0.0f, 16.0f), .y = txrng_rangef(0.0f, 9.0f)};
        ecs_set(world, invader, Target, {.x = pos.x, .y = pos.y});
        ecs_set(world, invader, Position, {.x = 0.0f, .y = 0.0f});
        ecs_set(world, invader, Velocity, {.x = 0.0f, .y = 0.0f});
        ecs_set(world, invader, SpriteDraw, {.sprite_id = 2});
    }

    while (ecs_progress(world, 0.0f)) {
        spr_render(1280, 720);
    }

    spr_term();

    return ecs_fini(world);
}

void PushSpritesToRenderQueue(ecs_iter_t* it)
{
    Position* position = ecs_column(it, Position, 1);
    SpriteDraw* sprite = ecs_column(it, SpriteDraw, 2);

    for (int i = 0; i < it->count; ++i) {
        uint32_t sprite_id = sprite[i].sprite_id;
        sprite_flip flip = sprite[i].flip;
        uint16_t swidth = (sprite[i].swidth) ? sprite[i].swidth : 1;
        uint16_t sheight = (sprite[i].sheight) ? sprite[i].sheight : 1;
        float layer = 0.0f;
        vec2 origin = {.x = 0.0f, .y = 0.0f};

        spr_push_sprite(&(struct sprite){
            .pos =
                (vec3){
                    .x = position[i].x,
                    .y = position[i].y,
                    .z = layer,
                },
            .rect = spr_calc_rect(sprite_id, flip, swidth, sheight),
            .scale = {.x = (float)swidth, .y = (float)sheight},
            .origin = origin,
        });
    }
}

void InvaderMovement(ecs_iter_t* it)
{
    Position* position = ecs_column(it, Position, 1);
    Velocity* velocity = ecs_column(it, Velocity, 2);
    Target* target = ecs_column(it, Target, 3);

    for (int i = 0; i < it->count; ++i) {
        vec2 delta = vec2_sub(target[i], position[i]);
        vec2 normDir = vec2_norm(delta);
        float accel = 10.0f;
        vec2 normVel = vec2_norm(velocity[i]);
        if (vec2_dot(normDir, normVel) < 0.0f && vec2_len(delta) < 1.0f) {
            accel *= 2.0f;
        }

        velocity[i] = vec2_add(velocity[i], vec2_scale(normDir, it->delta_time * accel));
        position[i] = vec2_add(position[i], vec2_scale(velocity[i], it->delta_time));
    }
}