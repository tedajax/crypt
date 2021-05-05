#include "game_components.h"
#include "sprite_renderer.h"
#include "strhash.h"
#include "system_sdl2.h"
#include "tx_math.h"
#include "tx_rand.h"
#include <time.h>

//// COMPONENTS
typedef vec2 Target;

//// TAGS

//// SYSTEMS
void InvaderMovement(ecs_iter_t* it);

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
            .pixels_per_meter = 16.0f,
            .canvas_width = 256,
            .canvas_height = 144,
        });

    ECS_COMPONENT(world, Target);

    ECS_SYSTEM(world, InvaderMovement, EcsOnUpdate, game.comp.Position, game.comp.Velocity, Target);

    ECS_ENTITY(world, PlayerEnt, game.comp.Position, game.comp.Velocity, sprite.renderer.Sprite);
    ecs_set(world, PlayerEnt, Position, {.x = 0.0f, .y = 0.0f});
    ecs_set(world, PlayerEnt, Velocity, {.x = 0.0f, .y = 0.0f});
    ecs_set(world, PlayerEnt, Sprite, {.sprite_id = 0});
    ecs_set(world, PlayerEnt, SpriteSize, {.width = 2, .height = 1});
    ecs_set(world, PlayerEnt, SpriteFlags, {.flags = SpriteFlags_FlipY});

    ECS_ENTITY(world, PlayerEn2, game.comp.Position, sprite.renderer.Sprite);
    ecs_set(world, PlayerEn2, Position, {.x = 8.0f, .y = 4.0f});
    ecs_set(world, PlayerEn2, Sprite, {.sprite_id = 0});
    ecs_set(world, PlayerEn2, SpriteSize, {.width = 2, .height = 1});

    ECS_PREFAB(world, InvaderPrefab, sprite.renderer.Sprite);
    ecs_set(world, InvaderPrefab, Sprite, {.sprite_id = 2});

    for (int i = 0; i < 100; ++i) {
        ecs_entity_t invader = ecs_new_w_pair(world, EcsIsA, InvaderPrefab);
        ecs_set(world, invader, Position, {.x = 0.0f, .y = 0.0f});
        ecs_set(world, invader, Velocity, {.x = 0.0f, .y = 0.0f});
        ecs_set(
            world,
            invader,
            Target,
            {.x = txrng_rangef(0.0f, 16.0f), .y = txrng_rangef(0.0f, 9.0f)});
    }

    while (ecs_progress(world, 0.0f)) {
    }

    return ecs_fini(world);
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
        if (vec2_len(velocity[i]) > 0.0f) {
            vec2 normVel = vec2_norm(velocity[i]);
            if (vec2_dot(normDir, normVel) < 0.0f && vec2_len(delta) < 1.0f) {
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