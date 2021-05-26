#include "color.h"
#include "debug_gui.h"
#include "game_components.h"
#include "sprite_renderer.h"
#include "strhash.h"
#include "system_imgui.h"
#include "system_sdl2.h"
#include "tx_input.h"
#include "tx_math.h"
#include "tx_rand.h"
#include <ccimgui.h>
#include <time.h>

//// COMPONENTS
typedef vec2 Target;
typedef struct Facing {
    float pos;
    float vel;
} Facing;

typedef struct TankInput {
    float move;
    bool fire;
} TankInput;

typedef struct TankConfig {
    float bounds_x;
} TankConfig;

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

typedef struct InvadersConfig {
    ecs_entity_t invader_prefab;
    vec2 spacing;
    float min_speed;
    float max_speed;
    float step_dist;
    int32_t invader_rows;
    int32_t invader_cols;
} InvadersConfig;

typedef struct InvaderControlContext {
    ecs_query_t* q_invaders;
    float move_dir;
    int32_t invaders_alive;
} InvaderControlContext;

typedef struct InvaderPosition {
    uint16_t x, y;
} InvaderPosition;

typedef struct Health {
    float health;
} Health;

typedef struct Damage {
    float amount;
    ecs_entity_t source;
    ecs_entity_t target;
} Damage;

ECS_COMPONENT_DECLARE(BoxCollider);
ECS_COMPONENT_DECLARE(ExpireAfter);
ECS_COMPONENT_DECLARE(TankConfig);
ECS_COMPONENT_DECLARE(InvadersConfig);
ECS_COMPONENT_DECLARE(InvaderControlContext);
ECS_COMPONENT_DECLARE(InvaderPosition);

//// TAGS
ECS_TAG_DECLARE(Projectile);
ECS_TAG_DECLARE(Friendly);
ECS_TAG_DECLARE(Hostile);

//// SYSTEMS
void InvaderControl(ecs_iter_t* it);
void AddInvaders(ecs_iter_t* it);
void RemoveInvaders(ecs_iter_t* it);
void TankGatherInput(ecs_iter_t* it);
void TankControl(ecs_iter_t* it);
void Move(ecs_iter_t* it);
void ExpireAfterUpdate(ecs_iter_t* it);
void ColliderView(ecs_iter_t* it);
void CollisionTrigger(ecs_iter_t* it);
void CreateColliderQueries(ecs_iter_t* it);
void CheckColliderIntersections(ecs_iter_t* it);

vec2 rnd_dir()
{
    float ang = txrng_rangef(0.0f, TX_PI * 2.0f);
    return (vec2){.x = cosf(ang), .y = sinf(ang)};
}

typedef struct tank_debug_context {
    ecs_entity_t e_tank;
} tank_debug_context;

void tank_debug_gui(ecs_world_t* world, void* ctx)
{
    tank_debug_context* context = (tank_debug_context*)ctx;

    ECS_IMPORT(world, GameComp);

    const Position* pos = ecs_get(world, context->e_tank, Position);
    TankConfig* config = ecs_get_mut(world, context->e_tank, TankConfig, false);

    igLabelText("POS", "%0.2f, %0.2f", pos->x, pos->y);
    igSliderFloat(
        "Boundary", &config->bounds_x, 0.0f, 32.0f, "%0.1f", ImGuiSliderFlags_AlwaysClamp);

    draw_line_col(
        (vec2){-config->bounds_x, 0.0f},
        (vec2){-config->bounds_x, 9.0f},
        (vec4){1.0f, 1.0f, 0.0f, 1.0f});
    draw_line_col(
        (vec2){config->bounds_x, 0.0f},
        (vec2){config->bounds_x, 9.0f},
        (vec4){1.0f, 1.0f, 0.0f, 1.0f});
}

typedef struct invader_control_debug_context {
    ecs_entity_t e_control;
} invader_control_debug_context;

void invader_control_debug_gui(ecs_world_t* world, void* ctx)
{
    invader_control_debug_context* context = (invader_control_debug_context*)ctx;

    const InvaderControlContext* control =
        ecs_get(world, context->e_control, InvaderControlContext);
    InvadersConfig* config = ecs_get_mut(world, context->e_control, InvadersConfig, false);

    bool needs_reset = false;

    if (igButton("RESET", (ImVec2){-1, 30})) {
        needs_reset = true;
    }

    igLabelText("Alive", "%d", control->invaders_alive);
    igLabelText("Total", "%d", config->invader_rows * config->invader_cols);
    needs_reset |= igInputInt("Rows", &config->invader_rows, 1, 5, ImGuiInputTextFlags_None);
    needs_reset |= igInputInt("Cols", &config->invader_cols, 1, 5, ImGuiInputTextFlags_None);
    igInputFloat("Min Speed", &config->min_speed, 0.1f, 1.0f, "%.02f", ImGuiInputTextFlags_None);
    igInputFloat("Max Speed", &config->max_speed, 0.1f, 1.0f, "%.02f", ImGuiInputTextFlags_None);
    igInputFloat("Step Dist", &config->step_dist, 0.1f, 1.0f, "%.02f", ImGuiInputTextFlags_None);

    if (needs_reset) {
        ecs_remove(world, context->e_control, InvadersConfig);
        ecs_set_id(
            world, context->e_control, ecs_id(InvadersConfig), sizeof(InvadersConfig), config);
    }
}

int main(int argc, char* argv[])
{
    txrng_seed((uint32_t)time(NULL));
    strhash_init();

    ecs_tracing_enable(1);

    ecs_world_t* world = ecs_init_w_args(argc, argv);
    ecs_set_target_fps(world, 144.0f);

    ECS_IMPORT(world, GameComp);
    ECS_IMPORT(world, SystemSdl2);
    ECS_IMPORT(world, SystemImgui);
    ECS_IMPORT(world, SpriteRenderer);
    ECS_IMPORT(world, DebugGui);

    ECS_ENTITY(world, Root, system.sdl2.WindowConfig);

    ecs_entity_t window =
        ecs_set(world, Root, WindowConfig, {.title = "crypt", .width = 1920, .height = 1080});

    ecs_entity_t render_config = ecs_set(
        world,
        Root,
        SpriteRenderConfig,
        {
            .e_window = window,
            .pixels_per_meter = 8.0f,
            .canvas_width = 256,
            .canvas_height = 144,
        });

    ecs_set(world, Root, ImguiDesc, {0});

    ECS_COMPONENT(world, Target);
    ECS_COMPONENT(world, Facing);
    ECS_COMPONENT(world, TankInput);
    ECS_COMPONENT_DEFINE(world, TankConfig);
    ECS_COMPONENT(world, TankControlContext);
    ECS_COMPONENT_DEFINE(world, BoxCollider);
    ECS_COMPONENT_DEFINE(world, ExpireAfter);
    ECS_COMPONENT(world, ColliderQuery);
    ECS_COMPONENT_DEFINE(world, InvaderPosition);
    ECS_COMPONENT_DEFINE(world, InvadersConfig);
    ECS_COMPONENT_DEFINE(world, InvaderControlContext);

    ECS_TAG_DEFINE(world, Projectile);
    ECS_TAG_DEFINE(world, Friendly);
    ECS_TAG_DEFINE(world, Hostile);
    ECS_TAG(world, NoAutoMove);

    ECS_ENTITY(world, InvaderRoot, game.comp.Position);
    ecs_set(world, InvaderRoot, EcsName, {.value = "InvaderRoot"});

    // clang-format off
    ECS_SYSTEM(world, TankGatherInput, EcsPostLoad, TankInput);
    ECS_SYSTEM(world, TankControl, EcsOnUpdate, game.comp.Position, game.comp.Velocity, TankInput, TankConfig, SYSTEM:TankControlContext);
    ECS_SYSTEM(world, InvaderControl, EcsOnUpdate, SYSTEM:InvaderControlContext, SYSTEM:InvadersConfig, InvaderRoot:game.comp.Position);
    ECS_SYSTEM(world, AddInvaders, EcsOnSet, InvaderControlContext, InvadersConfig, InvaderRoot:game.comp.Position);
    ECS_SYSTEM(world, RemoveInvaders, EcsUnSet, InvaderControlContext, InvadersConfig);
    ECS_SYSTEM(world, Move, EcsOnUpdate, game.comp.Position, game.comp.Velocity, !NoAutoMove);
    ECS_SYSTEM(world, ColliderView, EcsPostUpdate, game.comp.Position, ANY:BoxCollider);
    ECS_SYSTEM(world, ExpireAfterUpdate, EcsOnUpdate, ExpireAfter);
    // ECS_SYSTEM(world, CreateColliderQueries, EcsOnSet, ColliderQuery);
    ECS_SYSTEM(world, CheckColliderIntersections, EcsPostUpdate, game.comp.Position, SHARED:BoxCollider, SHARED:ColliderQuery);
    // clang-format on

    ECS_PREFAB(world, InvaderPrefab, sprite.renderer.Sprite, BoxCollider, Hostile);
    ecs_set(
        world,
        InvaderPrefab,
        Sprite,
        {.sprite_id = 2, .layer = 2.0f, .origin = (vec2){0.5f, 0.5f}, .width = 1, .height = 1});
    ecs_set(world, InvaderPrefab, BoxCollider, {.layer = 1, .size = {0.5f, 0.5f}});

    ecs_set(
        world,
        InvaderControl,
        InvaderControlContext,
        {
            .q_invaders = ecs_query_new(world, "game.comp.Position, InvaderPosition"),
        });
    ecs_set(
        world,
        InvaderControl,
        InvadersConfig,
        {
            .invader_prefab = InvaderPrefab,
            .min_speed = 2.0f,
            .max_speed = 16.0f,
            .step_dist = 0.25f,
            .spacing = {.x = 1.75f, .y = 1.25f},
            .invader_rows = 6,
            .invader_cols = 14,
        });

    ECS_ENTITY(
        world,
        Tank,
        game.comp.Position,
        game.comp.Velocity,
        TankInput,
        sprite.renderer.Sprite,
        NoAutoMove);
    ecs_set(world, Tank, Position, {.x = 0.0f, .y = 9.0f});
    ecs_set(world, Tank, Velocity, {.x = 0.0f, .y = 0.0f});
    ecs_set(world, Tank, TankInput, {0});
    ecs_set(
        world,
        Tank,
        Sprite,
        {.sprite_id = 0, .layer = 1.0f, .origin = (vec2){0.5f, 1.0f}, .width = 2, .height = 1});
    ecs_set(world, Tank, TankConfig, {.bounds_x = 16.0f});

    ECS_PREFAB(world, TankProjectilePrefab, Projectile, BoxCollider, Friendly);
    ecs_set(world, TankProjectilePrefab, BoxCollider, {.size = {.x = 0.125f, .y = 0.25f}});

    {
        ecs_query_t* query = ecs_query_new(
            world, "[in] game.comp.Position, [in] ANY:BoxCollider, !ANY:Friendly, ANY:Hostile");
        ecs_set(world, TankProjectilePrefab, ColliderQuery, {.q_targets = query});
    }

    ecs_set(world, TankControl, TankControlContext, {.bullet_prefab = TankProjectilePrefab});

    DEBUG_PANEL(world, Tank, "shift+1", tank_debug_gui, tank_debug_context, {.e_tank = Tank});
    DEBUG_PANEL(
        world,
        InvaderControl,
        "shift+2",
        invader_control_debug_gui,
        invader_control_debug_context,
        {.e_control = InvaderControl});

    while (ecs_progress(world, 0.0f)) {
    }

    return ecs_fini(world);
}

void InvaderControl(ecs_iter_t* it)
{
    InvaderControlContext* context = ecs_term(it, InvaderControlContext, 1);
    InvadersConfig* config = ecs_term(it, InvadersConfig, 2);
    Position* root = ecs_term(it, Position, 3);

    int32_t num_alive_invaders = 0;
    {
        ecs_iter_t qit = ecs_query_iter(context->q_invaders);
        while (ecs_query_next(&qit)) {
            num_alive_invaders += qit.count;
        }
    }

    int32_t num_invaders = config->invader_rows * config->invader_cols;
    float invader_ratio = (float)num_alive_invaders / num_invaders;
    float speed = lerpf(config->max_speed, config->min_speed, invader_ratio);

    if (context->move_dir == 0.0f) {
        context->move_dir = 1.0f;
    }

    *root = vec2_add(*root, (vec2){.x = speed * context->move_dir * it->delta_time});

    context->invaders_alive = 0;
    float bounds[4] = {INFINITY, -INFINITY, INFINITY, -INFINITY};

    ecs_iter_t qit = ecs_query_iter(context->q_invaders);
    while (ecs_query_next(&qit)) {
        Position* pos = ecs_term(&qit, Position, 1);
        InvaderPosition* ipos = ecs_term(&qit, InvaderPosition, 2);

        context->invaders_alive += qit.count;

        for (int32_t i = 0; i < qit.count; ++i) {
            vec2 local = (vec2){
                .x = config->spacing.x * ipos[i].x,
                .y = config->spacing.y * ipos[i].y,
            };

            pos[i] = vec2_add(*root, local);

            if (pos[i].x < bounds[0]) bounds[0] = pos[i].x;
            if (pos[i].x > bounds[1]) bounds[1] = pos[i].x;
            if (pos[i].y < bounds[2]) bounds[2] = pos[i].y;
            if (pos[i].y > bounds[3]) bounds[3] = pos[i].y;
        }
    }

    bool change_dir = false;
    if ((context->move_dir > 0 && bounds[1] > 15.0f)
        || (context->move_dir < 0 && bounds[0] < -15.0f)) {
        change_dir = true;
    }

    if (change_dir) {
        context->move_dir *= -1;
        root->y += config->step_dist;
    }
}

void RemoveInvaders(ecs_iter_t* it)
{
    InvaderControlContext* context = ecs_term(it, InvaderControlContext, 1);
    InvadersConfig* config = ecs_term(it, InvadersConfig, 2);

    if (ecs_should_quit(it->world)) {
        return;
    }

    ecs_iter_t qit = ecs_query_iter(context->q_invaders);
    while (ecs_query_next(&qit)) {
        for (int32_t i = 0; i < qit.count; ++i) {
            if (ecs_is_alive(it->world, qit.entities[i])) {
                ecs_delete(it->world, qit.entities[i]);
            }
        }
    }
}

void AddInvaders(ecs_iter_t* it)
{
    ECS_IMPORT(it->world, GameComp);

    InvaderControlContext* context = ecs_term(it, InvaderControlContext, 1);
    InvadersConfig* config = ecs_term(it, InvadersConfig, 2);
    Position* root = ecs_term(it, Position, 3);

    int32_t num_invaders = config->invader_cols * config->invader_rows;
    for (int32_t i = 0; i < num_invaders; ++i) {
        uint16_t row = i / config->invader_cols;
        uint16_t col = i % config->invader_cols;

        ecs_entity_t invader = ecs_new_w_pair(it->world, EcsIsA, config->invader_prefab);
        ecs_set(it->world, invader, Position, {.x = 0.0f, .y = 0.0f});
        ecs_set(it->world, invader, InvaderPosition, {.x = col, .y = row});
    }

    *root = (vec2){-15, -8};
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
    TankConfig* config = ecs_term(it, TankConfig, 4);
    TankControlContext* context = ecs_term(it, TankControlContext, 5);

    ECS_IMPORT(it->world, GameComp);
    // draw_set_prim_layer(10.0f);
    // draw_line_col((vec2){-15, -8}, (vec2){15, 8}, (vec4){1, 0, 0, 1});
    // draw_set_prim_layer(5.0f);
    // draw_rect_col((vec2){-5, -5}, (vec2){5, 5}, (vec4){1, 1, 0, 1});
    // draw_set_prim_layer(0.0f);
    // draw_rect_col((vec2){-2, -3}, (vec2){3, 2}, (vec4){1, 0, 1, 1});

    for (int32_t i = 0; i < it->count; ++i) {
        velocity[i].x = input[i].move * 32.0f;
        position[i] = vec2_add(position[i], vec2_scale(velocity[i], it->delta_time));

        position[i].x = clampf(position[i].x, -config->bounds_x + 0.9f, config->bounds_x - 0.9f);

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