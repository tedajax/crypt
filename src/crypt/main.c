#include "color.h"
#include "debug_gui.h"
#include "game_components.h"
#include "physics.h"
#include "sprite_renderer.h"
#include "strhash.h"
#include "system_imgui.h"
#include "system_sdl2.h"
#include "tx_input.h"
#include "tx_math.h"
#include "tx_rand.h"
#include <SDL2/SDL.h>
#include <ccimgui.h>
#include <time.h>

//// COMPONENTS
typedef vec2 Target;
typedef struct Facing {
    float pos;
    float vel;
} Facing;

typedef struct Bounds {
    float l, r, t, b;
} Bounds;

typedef struct TankInput {
    float move;
    bool fire;
} TankInput;

typedef struct TankConfig {
    float bounds_x;
} TankConfig;

typedef struct GunInput {
    bool is_firing;
} GunInput;

typedef struct GunState {
    float shot_timer;
} GunState;

typedef struct GunConfig {
    ecs_entity_t projectile_prefab;
    float shot_interval;
} GunConfig;

typedef struct ExpireAfter {
    float seconds;
} ExpireAfter;

typedef struct TankControlContext {
    ecs_entity_t bullet_prefab;
} TankControlContext;

typedef struct InvadersConfig {
    ecs_entity_t invader_prefab;
    vec2 spacing;
    float max_step_interval;
    float min_step_interval;
    vec2 step_dist;
    int32_t invader_rows;
    int32_t invader_cols;
} InvadersConfig;

typedef struct InvaderConfig {
    float smooth;
} InvaderConfig;

typedef struct InvaderControlContext {
    ecs_query_t* q_invaders;
    vec2 move_dir;
    float next_x_dir;
    float last_step_y;
    float step_timer;
    int32_t invaders_alive;
} InvaderControlContext;

typedef struct InvaderPosition {
    uint16_t x, y;
} InvaderPosition;

typedef struct InvaderTarget {
    ecs_entity_t target_ent;
} InvaderTarget;

typedef struct MaxHealth {
    float value;
} MaxHealth;

typedef struct Health {
    float value;
} Health;

typedef struct Damage {
    float amount;
} Damage;

typedef struct DamageConfig {
    float amount;
} DamageConfig;

ECS_COMPONENT_DECLARE(Target);
ECS_COMPONENT_DECLARE(Bounds);
ECS_COMPONENT_DECLARE(ExpireAfter);
ECS_COMPONENT_DECLARE(TankConfig);
ECS_COMPONENT_DECLARE(InvadersConfig);
ECS_COMPONENT_DECLARE(InvaderControlContext);
ECS_COMPONENT_DECLARE(InvaderPosition);
ECS_COMPONENT_DECLARE(InvaderTarget);
ECS_COMPONENT_DECLARE(InvaderConfig);

ECS_COMPONENT_DECLARE(Health);
ECS_COMPONENT_DECLARE(MaxHealth);
ECS_COMPONENT_DECLARE(Damage);
ECS_COMPONENT_DECLARE(DamageConfig);

//// TAGS
ECS_TAG_DECLARE(Projectile);
ECS_TAG_DECLARE(Friendly);
ECS_TAG_DECLARE(Hostile);

//// SYSTEMS
void UpdatePositionHeirarchy(ecs_iter_t* it);
void InvaderRootControl(ecs_iter_t* it);
void InvaderMovement(ecs_iter_t* it);
void AddInvaders(ecs_iter_t* it);
void RemoveInvaders(ecs_iter_t* it);
void TankGatherInput(ecs_iter_t* it);
void TankControl(ecs_iter_t* it);
void Move(ecs_iter_t* it);
void ExpireAfterUpdate(ecs_iter_t* it);
void ExpireAfterTraitUpdate(ecs_iter_t* it);
void TankGunControl(ecs_iter_t* it);
void CopyExpireAfter(ecs_iter_t* it);
void UpdateBounds(ecs_iter_t* it);
void OnInvaderRemoved(ecs_iter_t* it);
void ApplyDamage(ecs_iter_t* it);
void InitializeHealth(ecs_iter_t* it);

vec2 rnd_dir()
{
    float ang = txrng_rangef(0.0f, TX_PI * 2.0f);
    return (vec2){.x = cosf(ang), .y = sinf(ang)};
}

typedef struct tank_debug_context {
    ecs_entity_t e_tank;
    DEBUG_PANEL_DECLARE_COMPONENT(Position);
} tank_debug_context;

void tank_debug_gui(ecs_world_t* world, void* ctx)
{
    tank_debug_context* context = (tank_debug_context*)ctx;

    DEBUG_PANEL_LOAD_COMPONENT(context, Position);

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
    igInputFloat2("Spacing", &config->spacing.x, "%0.2f", ImGuiInputTextFlags_None);
    igInputFloat(
        "Min Interval", &config->min_step_interval, 0.01f, 0.1f, "%.05f", ImGuiInputTextFlags_None);
    igInputFloat(
        "Max Interval", &config->max_step_interval, 0.01f, 0.1f, "%.05f", ImGuiInputTextFlags_None);
    igInputFloat2("Step Dist", &config->step_dist.x, "%0.2f", ImGuiInputTextFlags_None);

    if (needs_reset) {
        ecs_remove(world, context->e_control, InvadersConfig);
        ecs_set_id(
            world, context->e_control, ecs_id(InvadersConfig), sizeof(InvadersConfig), config);
    }
}

void ecs_logf(ecs_world_t* world, const char* fmt, ...)
{
    char buffer[512];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, 512, fmt, args);
    va_end(args);

    const ecs_world_info_t* info = ecs_get_world_info(world);
    printf("%04.2fs | %s\n", info->world_time_total, buffer);
}

void bullet_contact_start(ecs_world_t* world, ecs_entity_t self, ecs_entity_t other)
{
    const DamageConfig* damage = ecs_get(world, self, DamageConfig);
    if (damage) {
        ecs_set(world, other, Damage, {.amount = damage->amount});
    }

    ecs_delete(world, self);
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
    ECS_IMPORT(world, DebugGui);
    ECS_IMPORT(world, SpriteRenderer);
    ECS_IMPORT(world, Physics);

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

    ECS_COMPONENT_DEFINE(world, Target);
    ECS_COMPONENT_DEFINE(world, Bounds);
    ECS_COMPONENT(world, Facing);
    ECS_COMPONENT(world, TankInput);
    ECS_COMPONENT_DEFINE(world, TankConfig);
    ECS_COMPONENT(world, TankControlContext);
    ECS_COMPONENT_DEFINE(world, ExpireAfter);
    ECS_COMPONENT_DEFINE(world, InvaderPosition);
    ECS_COMPONENT_DEFINE(world, InvaderTarget);
    ECS_COMPONENT_DEFINE(world, InvadersConfig);
    ECS_COMPONENT_DEFINE(world, InvaderControlContext);
    ECS_COMPONENT_DEFINE(world, InvaderConfig);
    ECS_COMPONENT(world, GunConfig);
    ECS_COMPONENT(world, GunState);
    ECS_COMPONENT(world, GunInput);
    ECS_COMPONENT_DEFINE(world, Health);
    ECS_COMPONENT_DEFINE(world, MaxHealth);
    ECS_COMPONENT_DEFINE(world, Damage);
    ECS_COMPONENT_DEFINE(world, DamageConfig);

    ECS_TAG_DEFINE(world, Projectile);
    ECS_TAG_DEFINE(world, Friendly);
    ECS_TAG_DEFINE(world, Hostile);
    ECS_TAG(world, NoAutoMove);

    ECS_ENTITY(world, InvaderRoot, game.comp.Position, Bounds);
    ecs_set(world, InvaderRoot, EcsName, {.value = "InvaderRoot"});
    ecs_set(world, InvaderRoot, Bounds, {INFINITY, -INFINITY, INFINITY, -INFINITY});

    // clang-format off
    ECS_SYSTEM(world, UpdatePositionHeirarchy, EcsPostUpdate, CASCADE:game.comp.Position, OWNED:game.comp.LocalPosition, OWNED:game.comp.Position);
    ECS_SYSTEM(world, TankGatherInput, EcsPostLoad, TankInput);
    ECS_SYSTEM(world, TankControl, EcsOnUpdate, game.comp.Position, game.comp.Velocity, TankInput, TankConfig, SYSTEM:TankControlContext);
    ECS_SYSTEM(world, InvaderRootControl, EcsOnUpdate, SYSTEM:InvaderControlContext, SYSTEM:InvadersConfig, InvaderRoot:game.comp.Position, InvaderRoot:Bounds);
    ECS_SYSTEM(world, InvaderMovement, EcsOnUpdate, game.comp.Position, game.comp.Velocity, InvaderTarget, ANY:InvaderConfig);
    ECS_SYSTEM(world, AddInvaders, EcsOnSet, InvaderControlContext, InvadersConfig, InvaderRoot:game.comp.Position, :game.comp.LocalPosition, :game.comp.Velocity, :physics.Box, :physics.Collider);
    ECS_SYSTEM(world, RemoveInvaders, EcsUnSet, InvaderControlContext, InvadersConfig);
    ECS_SYSTEM(world, Move, EcsOnUpdate, game.comp.Position, game.comp.Velocity, !NoAutoMove);
    ECS_SYSTEM(world, ExpireAfterUpdate, EcsPostUpdate, ExpireAfter);
    ECS_SYSTEM(world, ExpireAfterTraitUpdate, EcsPostUpdate, PAIR | ExpireAfter);
    ECS_SYSTEM(world, TankGunControl, EcsOnUpdate, PARENT:TankInput, GunConfig, GunState, OWNED:game.comp.Position, :game.comp.Velocity, :physics.Box, :physics.Collider);
    ECS_SYSTEM(world, UpdateBounds, EcsPostUpdate, game.comp.Position, Bounds);
    ECS_SYSTEM(world, ApplyDamage, EcsOnSet, Health, Damage, :sprite.renderer.SpriteColor, :ExpireAfter);
    ECS_SYSTEM(world, InitializeHealth, EcsOnSet, [in] ANY:MaxHealth, !Health);

    // entities using an expire after component from a prerab will need to copy the prefab value into their own instance
    ECS_SYSTEM(world, CopyExpireAfter, EcsOnSet, SHARED:ExpireAfter);
    ECS_TRIGGER(world, OnInvaderRemoved, EcsOnRemove, InvaderTarget);
    // clang-format on

    ECS_PREFAB(world, InvaderPrefab, sprite.renderer.Sprite, physics.Box, Hostile, NoAutoMove);
    ecs_set(
        world,
        InvaderPrefab,
        Sprite,
        {.sprite_id = 2, .layer = 10.0f, .origin = (vec2){0.5f, 0.5f}, .width = 1, .height = 1});
    ecs_set(world, InvaderPrefab, PhysBox, {.size = {0.5f, 0.5f}});
    ecs_set(world, InvaderPrefab, InvaderConfig, {.smooth = 0.4f});
    ecs_set(world, InvaderPrefab, MaxHealth, {.value = 3.0f});

    ecs_set(
        world,
        InvaderRootControl,
        InvaderControlContext,
        {
            .q_invaders = ecs_query_new(world, "InvaderTarget, game.comp.Position"),
        });
    ecs_set(
        world,
        InvaderRootControl,
        InvadersConfig,
        {
            .invader_prefab = InvaderPrefab,
            .min_step_interval = 1.0f / 30.0f,
            .max_step_interval = 1.0f,
            .step_dist = {.x = 2.0f, .y = 1.0f},
            .spacing = {.x = 1.33f, .y = 1.25f},
            .invader_rows = 8,
            .invader_cols = 10,
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
        {.sprite_id = 0, .layer = 5.0f, .origin = (vec2){0.5f, 1.0f}, .width = 2, .height = 1});
    ecs_set(world, Tank, TankConfig, {.bounds_x = 16.0f});

    ECS_PREFAB(world, TankProjectilePrefab, Projectile, ExpireAfter, physics.Box, Friendly);
    ecs_set(world, TankProjectilePrefab, DamageConfig, {.amount = 1.0f});
    ecs_set(world, TankProjectilePrefab, PhysBox, {.size = {.x = 0.125f, .y = 0.25f}});
    ecs_set(world, TankProjectilePrefab, ExpireAfter, {.seconds = 1.0f});
    ecs_set(world, TankProjectilePrefab, PhysQuery, {.sig = "!ANY:Friendly, ANY:Hostile"});
    ecs_set(world, TankProjectilePrefab, PhysReceiver, {.on_contact_start = bullet_contact_start});
    ecs_set(
        world,
        TankProjectilePrefab,
        Sprite,
        {.sprite_id = 16, .width = 1, .height = 1, .origin = {0.5f, 0.5f}, .layer = 6.0f});

    ECS_ENTITY(world, TankGun, CHILDOF | Tank, game.comp.Position, GunConfig, GunState);
    ecs_set(
        world,
        TankGun,
        GunConfig,
        {
            .projectile_prefab = TankProjectilePrefab,
            .shot_interval = 0.08f,
        });
    ecs_set(world, TankGun, GunState, {0});
    ecs_set(world, TankGun, LocalPosition, {0, 0.25f});

    ecs_set(world, TankControl, TankControlContext, {.bullet_prefab = TankProjectilePrefab});

    DEBUG_PANEL(
        world,
        Tank,
        "shift+1",
        tank_debug_gui,
        tank_debug_context,
        {
            .e_tank = Tank,
            DEBUG_PANEL_STORE_COMPONENT(world, Position, "game.comp.Position"),
        });
    DEBUG_PANEL(
        world,
        InvaderRootControl,
        "shift+2",
        invader_control_debug_gui,
        invader_control_debug_context,
        {.e_control = InvaderRootControl});

    while (ecs_progress(world, 0.0f)) {
    }

    return ecs_fini(world);
}

void UpdatePositionHeirarchy(ecs_iter_t* it)
{
    Position* parent = ecs_term(it, Position, 1);
    LocalPosition* local = ecs_term(it, LocalPosition, 2);
    Position* self = ecs_term(it, Position, 3);

    if (parent) {
        for (int32_t i = 0; i < it->count; ++i) {
            self[i] = vec2_add(*parent, local[i]);
        }
    }
}

void InvaderRootControl(ecs_iter_t* it)
{
    InvaderControlContext* context = ecs_term(it, InvaderControlContext, 1);
    InvadersConfig* config = ecs_term(it, InvadersConfig, 2);
    Position* root = ecs_term(it, Position, 3);
    Bounds* root_bounds = ecs_term(it, Bounds, 4);

    if (context->step_timer > 0.0f) {
        context->step_timer -= it->delta_time;
        return;
    }

    int32_t num_alive_invaders = 0;
    {
        ecs_iter_t qit = ecs_query_iter(context->q_invaders);
        while (ecs_query_next(&qit)) {
            num_alive_invaders += qit.count;
        }
    }
    context->invaders_alive = num_alive_invaders;

    int32_t num_invaders = config->invader_rows * config->invader_cols;
    float invader_ratio = (float)num_alive_invaders / num_invaders;
    vec2 vel = (vec2){
        .x = context->move_dir.x * config->step_dist.x,
        .y = context->move_dir.y * config->step_dist.y,
    };

    const float min_step_scalar = 0.25f;

    if (vel.x > 0.0f && root_bounds->r + vel.x > 14.0f) {
        vel.x = 14.0f - root_bounds->r;
        if (abs(vel.x) < config->step_dist.x * min_step_scalar) {
            vel.y = config->step_dist.y;
            context->move_dir.x = -context->move_dir.x;
        } else {
            context->next_x_dir = -context->move_dir.x;
            context->move_dir.x = 0.0f;
            context->move_dir.y = 1.0f;
        }
    } else if (vel.x < 0.0f && root_bounds->l + vel.x < -14.0f) {
        vel.x = -14.0f - root_bounds->l;
        if (abs(vel.x) < config->step_dist.x * min_step_scalar) {
            vel.y = config->step_dist.y;
            context->move_dir.x = -context->move_dir.x;
        } else {
            context->next_x_dir = -context->move_dir.x;
            context->move_dir.x = 0.0f;
            context->move_dir.y = 1.0f;
        }
    } else if (vel.y > 0.0f) {
        context->move_dir.x = context->next_x_dir;
        context->move_dir.y = 0.0f;
    } else if (context->move_dir.x == 0.0f && context->move_dir.y == 0.0f) {
        context->move_dir.x = 1.0f;
    }

    *root = vec2_add(*root, vel);

    float interval = lerpf(config->min_step_interval, config->max_step_interval, invader_ratio);
    context->step_timer = interval;
}

void InvaderMovement(ecs_iter_t* it)
{
    Position* pos = ecs_term(it, Position, 1);
    Velocity* vel = ecs_term(it, Velocity, 2);
    InvaderTarget* targ = ecs_term(it, InvaderTarget, 3);
    const InvaderConfig* config = ecs_term(it, InvaderConfig, 4);

    ecs_entity_t ecs_id(Position) = ecs_term_id(it, 1);

    for (int32_t i = 0; i < it->count; ++i) {
        const Position* target_pos = ecs_get(it->world, targ[i].target_ent, Position);
        if (!target_pos) {
            continue;
        }

        vec2 target = *target_pos;

        float smooth;
        if (ecs_is_owned(it, 4)) {
            smooth = config[i].smooth;
        } else {
            smooth = config->smooth;
        }
        pos[i].x = smooth_damp(pos[i].x, target.x, &vel[i].x, smooth, INFINITY, it->delta_time);
        pos[i].y = smooth_damp(pos[i].y, target.y, &vel[i].y, smooth, INFINITY, it->delta_time);

        // draw_point_col(target, k_color_chartreuse);
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
    InvaderControlContext* context = ecs_term(it, InvaderControlContext, 1);
    InvadersConfig* config = ecs_term(it, InvadersConfig, 2);
    Position* root = ecs_term(it, Position, 3);

    *root = (vec2){-14, -8};

    ecs_entity_t root_ent = ecs_lookup(it->world, "InvaderRoot");

    ecs_id_t ecs_id(Position) = ecs_term_id(it, 3);
    ecs_id_t ecs_id(LocalPosition) = ecs_term_id(it, 4);
    ecs_id_t ecs_id(Velocity) = ecs_term_id(it, 5);
    ecs_id_t ecs_id(PhysBox) = ecs_term_id(it, 6);
    ecs_id_t ecs_id(PhysCollider) = ecs_term_id(it, 7);

    int32_t num_invaders = config->invader_cols * config->invader_rows;
    for (int32_t i = 0; i < num_invaders; ++i) {
        uint16_t row = i / config->invader_cols;
        uint16_t col = i % config->invader_cols;

        vec2 local_pos = (vec2){.x = col * config->spacing.x, .y = row * config->spacing.y};
        vec2 world_pos = vec2_add(*root, local_pos);

        ecs_entity_t invader_target = ecs_new_w_pair(it->world, EcsChildOf, root_ent);
        ecs_set_ptr(it->world, invader_target, LocalPosition, &local_pos);
        ecs_set(it->world, invader_target, Position, {0});

        ecs_entity_t invader = ecs_new_w_pair(it->world, EcsIsA, config->invader_prefab);
        ecs_set(it->world, invader, InvaderTarget, {.target_ent = invader_target});
        ecs_set_ptr(it->world, invader, Position, &world_pos);
        ecs_set(it->world, invader, Velocity, {0});

        ecs_set(it->world, invader, InvaderConfig, {.smooth = (row + 1) * 0.15f});
        ecs_set_trait(it->world, invader, PhysBox, PhysCollider, {.layer = 1});
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
        input[i].fire = txinp_get_key(TXINP_KEY_Z);
    }
}

void TankControl(ecs_iter_t* it)
{
    Position* position = ecs_term(it, Position, 1);
    Velocity* velocity = ecs_term(it, Velocity, 2);
    TankInput* input = ecs_term(it, TankInput, 3);
    TankConfig* config = ecs_term(it, TankConfig, 4);
    TankControlContext* context = ecs_term(it, TankControlContext, 5);

    for (int32_t i = 0; i < it->count; ++i) {
        velocity[i].x = input[i].move * 32.0f;
        position[i] = vec2_add(position[i], vec2_scale(velocity[i], it->delta_time));

        position[i].x = clampf(position[i].x, -config->bounds_x + 0.9f, config->bounds_x - 0.9f);

        if (input[i].fire) {
        }
    }
}

void TankGunControl(ecs_iter_t* it)
{
    TankInput* tank_input = ecs_term(it, TankInput, 1);
    GunConfig* config = ecs_term(it, GunConfig, 2);
    GunState* state = ecs_term(it, GunState, 3);
    Position* gun_pos = ecs_term(it, Position, 4);

    ecs_entity_t ecs_id(Position) = ecs_term_id(it, 4);
    ecs_entity_t ecs_id(Velocity) = ecs_term_id(it, 5);
    ecs_entity_t ecs_id(PhysBox) = ecs_term_id(it, 6);
    ecs_entity_t ecs_id(PhysCollider) = ecs_term_id(it, 7);

    for (int32_t i = 0; i < it->count; ++i) {
        if (state[i].shot_timer > 0.0f) {
            state[i].shot_timer -= it->delta_time;
        }

        bool fire = tank_input[i].fire && state[i].shot_timer <= 0.0f;

        if (fire) {
            state[i].shot_timer += config[i].shot_interval;

            vec2 pos = gun_pos[i]; // vec2_add(tank_pos[i], gun_pos[i]);

            ecs_entity_t projectile = ecs_new_w_pair(it->world, EcsIsA, config->projectile_prefab);
            ecs_set(it->world, projectile, Position, {.x = pos.x, .y = pos.y - 1.0f});
            ecs_set(it->world, projectile, Velocity, {.x = 0.0f, .y = -64.0f});
            ecs_set_trait(it->world, projectile, PhysBox, PhysCollider, {.layer = 0});
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

void ExpireAfterTraitUpdate(ecs_iter_t* it)
{
    ExpireAfter* expire = ecs_term(it, ExpireAfter, 1);
    ecs_entity_t trait = ecs_term_id(it, 1);
    ecs_entity_t comp = ecs_entity_t_lo(trait);

    for (int32_t i = 0; i < it->count; ++i) {
        expire[i].seconds -= it->delta_time;
        if (expire[i].seconds <= 0) {
            ecs_remove_id(it->world, it->entities[i], trait);
            ecs_remove_id(it->world, it->entities[i], comp);
        }
    }
}

void CopyExpireAfter(ecs_iter_t* it)
{
    ExpireAfter* shared = ecs_term(it, ExpireAfter, 1);

    for (int32_t i = 0; i < it->count; ++i) {
        ecs_set(it->world, it->entities[i], ExpireAfter, {.seconds = shared[i].seconds});
    }
}

void UpdateBounds(ecs_iter_t* it)
{
    Position* position = ecs_term(it, Position, 1);
    Bounds* bounds = ecs_term(it, Bounds, 2);

    ecs_id_t ecs_id(Position) = ecs_term_id(it, 1);
    ecs_type_t ecs_type(Position) = ecs_type_from_id(it->world, ecs_id(Position));

    ecs_filter_t filter = (ecs_filter_t){
        .include = ecs_type(Position),
    };

    for (int32_t i = 0; i < it->count; ++i) {
        ecs_iter_t scope_it = ecs_scope_iter_w_filter(it->world, it->entities[i], &filter);
        bounds[i] = (Bounds){INFINITY, -INFINITY, INFINITY, -INFINITY};

        while (ecs_scope_next(&scope_it)) {
            for (int32_t s = 0; s < scope_it.count; ++s) {
                if (ecs_is_alive(it->world, scope_it.entities[s])) {
                    const Position* pos = ecs_get(it->world, scope_it.entities[s], Position);
                    if (pos->x < bounds[i].l) bounds[i].l = pos->x;
                    if (pos->x > bounds[i].r) bounds[i].r = pos->x;
                    if (pos->y < bounds[i].t) bounds[i].t = pos->y;
                    if (pos->y > bounds[i].b) bounds[i].b = pos->y;
                }
            }
        }

        // draw_line_rect_col(
        //     (vec2){bounds[i].l, bounds[i].t}, (vec2){bounds[i].r, bounds[i].b}, k_color_orange);
    }
}

void OnInvaderRemoved(ecs_iter_t* it)
{
    InvaderTarget* targ = ecs_term(it, InvaderTarget, 1);

    for (int32_t i = 0; i < it->count; ++i) {
        ecs_delete(it->world, targ[i].target_ent);
    }
}

void ApplyDamage(ecs_iter_t* it)
{
    Health* health = ecs_term(it, Health, 1);
    Damage* damage = ecs_term(it, Damage, 2);

    ecs_id_t ecs_id(SpriteColor) = ecs_term_id(it, 3);
    ecs_id_t ecs_id(ExpireAfter) = ecs_term_id(it, 4);

    for (int32_t i = 0; i < it->count; ++i) {
        health[i].value -= damage[i].amount;
        ecs_remove(it->world, it->entities[i], Damage);

        ecs_set(it->world, it->entities[i], SpriteColor, {k_color_white});
        ecs_set_trait(it->world, it->entities[i], SpriteColor, ExpireAfter, {.seconds = 1 / 15.0f});

        if (health[i].value <= 0) {
            ecs_delete(it->world, it->entities[i]);
        }
    }
}

void InitializeHealth(ecs_iter_t* it)
{
    MaxHealth* max_health = ecs_term(it, MaxHealth, 1);

    if (ecs_is_owned(it, 1)) {
        for (int32_t i = 0; i < it->count; ++i) {
            float health = max_health[i].value;
            ecs_set(it->world, it->entities[i], Health, {.value = health});
        }
    } else {
        float health = max_health->value;
        for (int32_t i = 0; i < it->count; ++i) {
            ecs_set(it->world, it->entities[i], Health, {.value = health});
        }
    }
}
