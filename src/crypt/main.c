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

typedef struct BoxCollider {
    uint8_t layer; // TEMP
    vec2 size;
} BoxCollider;

typedef struct ColliderQuery {
    const char* expr;
    ecs_query_t* q_targets;
} ColliderQuery;

typedef struct ExpireAfter {
    float seconds;
} ExpireAfter;

typedef struct TankControlContext {
    ecs_entity_t bullet_prefab;
} TankControlContext;

typedef struct Health {
    float health;
} Health;

typedef struct Damage {
    float amount;
    ecs_entity_t source;
    ecs_entity_t target;
} Damage;
//// TAGS

//// SYSTEMS
void InvaderControl(ecs_iter_t* it);
void TankGatherInput(ecs_iter_t* it);
void TankControl(ecs_iter_t* it);
void Move(ecs_iter_t* it);
void ExpireAfterUpdate(ecs_iter_t* it);
void ColliderView(ecs_iter_t* it);
void CollisionTrigger(ecs_iter_t* it);
void CreateColliderQueries(ecs_iter_t* it);
void CheckColliderIntersections(ecs_iter_t* it);

ECS_COMPONENT_DECLARE(BoxCollider);
ECS_COMPONENT_DECLARE(ExpireAfter);

ECS_TAG_DECLARE(Projectile);
ECS_TAG_DECLARE(Friendly);
ECS_TAG_DECLARE(Hostile);

int main(int argc, char* argv[])
{
    txrng_seed((uint32_t)time(NULL));
    strhash_init();

    ecs_tracing_enable(1);

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
    ECS_COMPONENT(world, TankControlContext);
    ECS_COMPONENT_DEFINE(world, BoxCollider);
    ECS_COMPONENT_DEFINE(world, ExpireAfter);
    ECS_COMPONENT(world, ColliderQuery);

    ECS_TAG_DEFINE(world, Projectile);
    ECS_TAG_DEFINE(world, Friendly);
    ECS_TAG_DEFINE(world, Hostile);

    // "[in] flecs.components.transform.Position3,"\
//     "[in] ANY:flecs.components.physics.Collider FOR flecs.components.geometry.Box || ANY:flecs.components.geometry.Box,"

    // clang-format off
    ECS_SYSTEM(world, TankGatherInput, EcsPostLoad, TankInput);
    ECS_SYSTEM(world, TankControl, EcsOnUpdate, game.comp.Position, game.comp.Velocity, TankInput, SYSTEM:TankControlContext);
    ECS_SYSTEM(world, InvaderControl, EcsOnUpdate, game.comp.Position, game.comp.Velocity, Target);
    ECS_SYSTEM(world, Move, EcsOnUpdate, game.comp.Position, game.comp.Velocity);
    ECS_SYSTEM(world, ColliderView, EcsPostUpdate, game.comp.Position, ANY:BoxCollider);
    ECS_SYSTEM(world, ExpireAfterUpdate, EcsOnUpdate, ExpireAfter);
    // ECS_SYSTEM(world, CreateColliderQueries, EcsOnSet, ColliderQuery);
    ECS_SYSTEM(world, CheckColliderIntersections, EcsPostUpdate, game.comp.Position, SHARED:BoxCollider, SHARED:ColliderQuery);
    // clang-format on

    ECS_ENTITY(
        world, Tank, game.comp.Position, game.comp.Velocity, TankInput, sprite.renderer.Sprite);
    ecs_set(world, Tank, Position, {.x = 0.0f, .y = 9.0f});
    ecs_set(world, Tank, Velocity, {.x = 0.0f, .y = 0.0f});
    ecs_set(world, Tank, TankInput, {0});
    ecs_set(
        world,
        Tank,
        Sprite,
        {.sprite_id = 0, .layer = 1.0f, .origin = (vec2){0.5f, 1.0f}, .width = 2, .height = 1});

    ECS_PREFAB(world, InvaderPrefab, sprite.renderer.Sprite, BoxCollider, Hostile);
    ecs_set(
        world,
        InvaderPrefab,
        Sprite,
        {.sprite_id = 2, .layer = 2.0f, .origin = (vec2){0.5f, 0.5f}, .width = 1, .height = 1});
    ecs_set(world, InvaderPrefab, BoxCollider, {.layer = 1, .size = {0.5f, 0.5f}});

    for (int i = 0; i < 30; ++i) {
        ecs_entity_t invader = ecs_new_w_pair(world, EcsIsA, InvaderPrefab);
        ecs_set(world, invader, Position, {.x = txrng_rangef(-16, 16), .y = txrng_rangef(-9, 6)});
        ecs_set(world, invader, Velocity, {.x = 0.0f, .y = 0.0f});
        ecs_set(world, invader, Target, {.x = txrng_rangef(-16, 16), .y = txrng_rangef(-9, 6)});
    }

    ECS_PREFAB(world, TankProjectilePrefab, Projectile, BoxCollider, Friendly);
    ecs_set(world, TankProjectilePrefab, BoxCollider, {.size = {.x = 0.125f, .y = 0.25f}});

    {
        ecs_query_t* query = ecs_query_new(
            world, "[in] game.comp.Position, [in] ANY:BoxCollider, !ANY:Friendly, ANY:Hostile");
        ecs_set(world, TankProjectilePrefab, ColliderQuery, {.q_targets = query});
    }

    ecs_set(world, TankControl, TankControlContext, {.bullet_prefab = TankProjectilePrefab});

    while (ecs_progress(world, 0.0f)) {

        if (txinp_get_key_down(TXINP_KEY_I)) {
            ecs_entity_t invader = ecs_new_w_pair(world, EcsIsA, InvaderPrefab);
            ecs_set(
                world, invader, Position, {.x = txrng_rangef(-16, 16), .y = txrng_rangef(-9, 6)});
            ecs_set(world, invader, Velocity, {.x = 0.0f, .y = 0.0f});
            ecs_set(world, invader, Target, {.x = txrng_rangef(-16, 16), .y = txrng_rangef(-9, 6)});
        }
    }

    return ecs_fini(world);
}

void InvaderControl(ecs_iter_t* it)
{
    Position* position = ecs_term(it, Position, 1);
    Velocity* velocity = ecs_term(it, Velocity, 2);
    Target* target = ecs_term(it, Target, 3);

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
    }
}

void TankGatherInput(ecs_iter_t* it)
{
    TankInput* input = ecs_term(it, TankInput, 1);

    for (int32_t i = 0; i < it->count; ++i) {
        float move = 0.0f;

        if (txinp_get_key(TXINP_KEY_LEFT)) move -= 1.0f;
        if (txinp_get_key(TXINP_KEY_RIGHT)) move += 1.0f;

        input[i].move = move;
        input[i].fire = txinp_get_key_down(TXINP_KEY_Z);
    }
}

void TankControl(ecs_iter_t* it)
{
    Position* position = ecs_term(it, Position, 1);
    Velocity* velocity = ecs_term(it, Velocity, 2);
    TankInput* input = ecs_term(it, TankInput, 3);
    TankControlContext* context = ecs_term(it, TankControlContext, 4);

    ecs_id_t ecs_typeid(Position) = ecs_term_id(it, 1);
    ecs_id_t ecs_typeid(Velocity) = ecs_term_id(it, 2);
    // draw_set_prim_layer(10.0f);
    // draw_line_col((vec2){-15, -8}, (vec2){15, 8}, (vec4){1, 0, 0, 1});
    // draw_set_prim_layer(5.0f);
    // draw_rect_col((vec2){-5, -5}, (vec2){5, 5}, (vec4){1, 1, 0, 1});
    // draw_set_prim_layer(0.0f);
    // draw_rect_col((vec2){-2, -3}, (vec2){3, 2}, (vec4){1, 0, 1, 1});

    for (int32_t i = 0; i < it->count; ++i) {
        velocity[i].x = input[i].move * 32.0f;

        if (input[i].fire) {
            ecs_entity_t projectile = ecs_new_w_pair(it->world, EcsIsA, context->bullet_prefab);
            ecs_set(
                it->world, projectile, Position, {.x = position[i].x, .y = position[i].y - 1.0f});
            ecs_set(it->world, projectile, Velocity, {.x = 0.0f, .y = -32.0f});
            ecs_set(it->world, projectile, ExpireAfter, {.seconds = 1.0f});
        }
    }
}

void Move(ecs_iter_t* it)
{
    Position* position = ecs_term(it, Position, 1);
    Velocity* velocity = ecs_term(it, Velocity, 2);

    for (int32_t i = 0; i < it->count; ++i) {
        vec2 vel = vec2_scale(velocity[i], it->delta_time);
        position[i] = vec2_add(position[i], vel);
    }
}

void ColliderView(ecs_iter_t* it)
{
    Position* position = ecs_term(it, Position, 1);
    BoxCollider* box = ecs_term(it, BoxCollider, 2);

    vec4 cols[2] = {
        (vec4){0.0f, 1.0f, 1.0f, 1.0f},
        (vec4){1.0f, 0.0f, 1.0f, 1.0f},
    };

    if (ecs_is_owned(it, 2)) {
        for (int32_t i = 0; i < it->count; ++i) {
            vec2 size = box[i].size;
            uint8_t layer = box[i].layer;

            vec2 p0 = vec2_sub(position[i], size);
            vec2 p1 = vec2_add(position[i], size);

            draw_line_rect_col(p0, p1, cols[layer]);
        }
    } else {
        for (int32_t i = 0; i < it->count; ++i) {
            vec2 size = box->size;
            uint8_t layer = box->layer;

            vec2 p0 = vec2_sub(position[i], size);
            vec2 p1 = vec2_add(position[i], size);

            draw_line_rect_col(p0, p1, cols[layer]);
        }
    }
}

void ExpireAfterUpdate(ecs_iter_t* it)
{
    ExpireAfter* expire = ecs_term(it, ExpireAfter, 1);

    for (int32_t i = 0; i < it->count; ++i) {
        expire[i].seconds -= it->delta_time;
        if (expire[i].seconds <= 0) {
            ecs_delete(it->world, it->entities[i]);
        }
    }
}

static const char* kColliderQueryExpr = "[in] game.comp.Position, [in] ANY:BoxCollider,";

void CreateColliderQueries(ecs_iter_t* it)
{
    ColliderQuery* query = ecs_term(it, ColliderQuery, 1);

    if (ecs_is_owned(it, 1)) {
        char sig[512];

        for (int32_t i = 0; i < it->count; ++i) {
            snprintf(sig, 512, "%s %s", kColliderQueryExpr, query[i].expr);
            query[i].q_targets = ecs_query_new(it->world, sig);
        }
    } else {
        char sig[512];
        snprintf(sig, 512, "%s %s", kColliderQueryExpr, query->expr);
        query->q_targets = ecs_query_new(it->world, sig);
    }
}

struct world_rect {
    float left, top, right, bottom;
};

bool world_rect_overlap(const struct world_rect* a, const struct world_rect* b)
{
    return a->left <= b->right && a->right >= b->left && a->top >= b->bottom && a->bottom <= b->top;
}

void box_to_world_rect(vec2 center, vec2 size, struct world_rect* out)
{
    out->left = center.x - size.x;
    out->right = center.x + size.x;
    out->bottom = center.y - size.y;
    out->top = center.y + size.y;
}

void CheckColliderIntersections(ecs_iter_t* it)
{
    Position* position = ecs_term(it, Position, 1);
    BoxCollider* box = ecs_term(it, BoxCollider, 2);
    ColliderQuery* query = ecs_term(it, ColliderQuery, 3);

    for (int32_t i = 0; i < it->count; ++i) {
        struct world_rect r0;
        box_to_world_rect(position[i], box->size, &r0);

        ecs_iter_t qit = ecs_query_iter(query->q_targets);
        while (ecs_query_next(&qit)) {
            for (int32_t j = 0; j < qit.count; ++j) {
                Position* targ_pos = ecs_term(&qit, Position, 1);
                BoxCollider* targ_box = ecs_term(&qit, BoxCollider, 2);

                struct world_rect r1;
                if (ecs_is_owned(&qit, 2)) {
                    box_to_world_rect(targ_pos[j], targ_box[j].size, &r1);
                } else {
                    box_to_world_rect(targ_pos[j], targ_box->size, &r1);
                }

                if (world_rect_overlap(&r0, &r1)) {
                    ecs_delete(it->world, it->entities[i]);
                    ecs_delete(it->world, qit.entities[j]);
                }
            }
        }
    }
}