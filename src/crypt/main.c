#include "sprite_draw.h"
#include "strhash.h"
#include "system_sdl2.h"
#include "system_window_sdl2.h"
#include "tdjx_game_comp.h"
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

    ECS_IMPORT(world, TdjxGameComp);
    ECS_IMPORT(world, SystemSdl2);
    ECS_IMPORT(world, SystemSdl2Window);
    ECS_IMPORT(world, TdjxSpriteRenderer);

    ecs_entity_t window =
        ecs_set(world, 0, WindowDesc, {.title = "crypt", .width = 1920, .height = 1080});

    ecs_entity_t render_config = ecs_set(
        world,
        0,
        SpriteRenderConfig,
        {
            .e_window = window,
            .pixels_per_meter = 16.0f,
            .canvas_width = 512 / 2,
            .canvas_height = 288 / 2,
        });

    ECS_COMPONENT(world, Target);

    ECS_SYSTEM(
        world,
        InvaderMovement,
        EcsOnUpdate,
        tdjx.game.comp.Position,
        tdjx.game.comp.Velocity,
        Target);

    ECS_ENTITY(
        world,
        PlayerEnt,
        tdjx.game.comp.Position,
        tdjx.game.comp.Velocity,
        tdjx.sprite.renderer.Sprite);
    ecs_set(world, PlayerEnt, Position, {.x = 0.0f, .y = 0.0f});
    ecs_set(world, PlayerEnt, Velocity, {.x = 0.0f, .y = 0.0f});
    ecs_set(world, PlayerEnt, TdjxSprite, {.sprite_id = 0});
    ecs_set(world, PlayerEnt, TdjxSpriteSize, {.width = 2, .height = 1});
    ecs_set(world, PlayerEnt, TdjxSpriteFlags, {.flags = SpriteFlags_FlipY});

    ECS_ENTITY(world, PlayerEn2, tdjx.game.comp.Position, tdjx.sprite.renderer.Sprite);
    ecs_set(world, PlayerEn2, Position, {.x = 8.0f, .y = 4.0f});
    ecs_set(world, PlayerEn2, TdjxSprite, {.sprite_id = 2});

    // ECS_ENTITY(world, PlayerEnt2, Position, SpriteDraw);
    // ecs_set(world, PlayerEnt2, Position, {.x = 4.0f, .y = 2.0f});
    // ecs_set(world, PlayerEnt2, SpriteDraw, {.sprite_id = 2, .swidth = 1});

    ECS_PREFAB(world, InvaderPrefab, tdjx.sprite.renderer.Sprite);
    ecs_set(world, InvaderPrefab, TdjxSprite, {.sprite_id = 2});

    for (int i = 0; i < 100; ++i) {
        // char idbuf[32] = {0};
        // snprintf(idbuf, 32, "invader_%03d", i);
        // strhash id = strhash_get(idbuf);

        // ecs_entity_t invader = ecs_new_entity(
        //     world,
        //     0,
        //     strhash_cstr(id),
        //     "tdjx.game.comp.Position, tdjx.game.comp.Velocity, Target, "
        //     "tdjx.sprite.renderer.Sprite");

        // ecs_set(world, invader, TdjxSprite, {.sprite_id = 2});
        // ecs_set(world, invader, Position, {.x = 0.0f, .y = 0.0f});
        // ecs_set(world, invader, Velocity, {.x = 0.0f, .y = 0.0f});
        // ecs_set(
        //     world,
        //     invader,
        //     Target,
        //     {.x = txrng_rangef(0.0f, 16.0f), .y = txrng_rangef(0.0f, 9.0f)});

        ecs_entity_t invader = ecs_new_w_pair(world, EcsIsA, InvaderPrefab);
        ecs_set(world, invader, Position, {.x = 0.0f, .y = 0.0f});
        ecs_set(world, invader, Velocity, {.x = 0.0f, .y = 0.0f});
        ecs_set(
            world,
            invader,
            Target,
            {.x = txrng_rangef(0.0f, 16.0f), .y = txrng_rangef(0.0f, 9.0f)});
    }

    // for (int i = 0; i < 1000; ++i) {
    //     ecs_entity_t invader = ecs_new_w_pair(world, EcsIsA, InvaderPrefab);
    //     Position pos = (Position){.x = txrng_rangef(0.0f, 16.0f), .y = txrng_rangef(0.0f, 9.0f)};
    //     ecs_set(world, invader, Target, {.x = pos.x, .y = pos.y});
    //     ecs_set(world, invader, Position, {.x = 0.0f, .y = 0.0f});
    //     ecs_set(world, invader, Velocity, {.x = 0.0f, .y = 0.0f});
    //     ecs_set(world, invader, SpriteDraw, {.sprite_id = 2});
    // }

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