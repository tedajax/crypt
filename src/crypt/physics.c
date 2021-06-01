#include "physics.h"
#include "debug_gui.h"
#include "game_components.h"
#include "sprite_renderer.h"
#include "stb_ds.h"
#include <ccimgui.h>

struct world_rect {
    float left, top, right, bottom;
};

bool phys_bounds_overlap(const PhysWorldBounds* a, const PhysWorldBounds* b)
{
    return a->left <= b->right && a->right >= b->left && a->top >= b->bottom && a->bottom <= b->top;
}

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

void box_to_bounds(vec2 center, vec2 size, float bounds[4])
{
    bounds[0] = center.x - size.x;
    bounds[1] = center.x + size.x;
    bounds[2] = center.y + size.y;
    bounds[3] = center.y - size.y;
}

void BoxColliderView(ecs_iter_t* it)
{
    Position* position = ecs_term(it, Position, 1);
    PhysCollider* collider = ecs_term(it, PhysCollider, 2);
    ecs_entity_t trait = ecs_term_id(it, 2);
    ecs_entity_t comp = ecs_entity_t_lo(trait);

    vec4 cols[2] = {
        k_color_azure,
        k_color_rose,
    };

    for (int32_t i = 0; i < it->count; ++i) {
        const PhysBox* box = ecs_get_w_id(it->world, it->entities[i], comp);
        vec2 size = box->size;

        vec2 p0 = vec2_sub(position[i], size);
        vec2 p1 = vec2_add(position[i], size);

        draw_line_rect_col(p0, p1, cols[collider[i].layer]);
    }
}

struct contact {
    ecs_entity_t key;
    bool value;
};

struct ent_contacts {
    ecs_entity_t key;
    struct contact* value; // map<entity, bool>
};

struct ent_contacts* contacts_map = NULL; // map<entity, map<entity, bool>>

struct contact* get_contact_map(ecs_entity_t ent)
{
    if (hmgeti(contacts_map, ent) < 0) {
        struct contact* contact_map = NULL;
        hmdefault(contact_map, false);
        hmput(contacts_map, ent, contact_map);
        return contact_map;
    }

    return hmget(contacts_map, ent);
}

bool add_contact(ecs_entity_t e0, ecs_entity_t e1)
{
    struct contact* map0 = get_contact_map(e0);
    struct contact* map1 = get_contact_map(e1);

    TX_ASSERT(hmget(map0, e1) == hmget(map1, e0));

    if (hmget(map0, e1) == false) {
        hmput(map0, e1, true);
        hmput(contacts_map, e0, map0);
        hmput(map1, e0, true);
        hmput(contacts_map, e1, map1);
        return true;
    }

    return false;
}

bool del_contact(ecs_entity_t e0, ecs_entity_t e1)
{
    struct contact* map0 = get_contact_map(e0);
    struct contact* map1 = get_contact_map(e1);

    TX_ASSERT(hmget(map0, e1) == hmget(map1, e0));

    if (hmget(map0, e1) == true) {
        hmdel(map0, e1);
        hmput(contacts_map, e0, map0);
        hmdel(map1, e0);
        hmput(contacts_map, e1, map1);
        return true;
    }

    return false;
}

bool in_contact(ecs_entity_t e0, ecs_entity_t e1)
{
    struct contact* map0 = get_contact_map(e0);
    struct contact* map1 = get_contact_map(e1);

    TX_ASSERT(hmget(map0, e1) == hmget(map1, e0));

    return hmget(map0, e1) == true;
}

bool has_contact(ecs_entity_t ent)
{
    struct contact* map = get_contact_map(ent);
    return hmlen(map) > 0;
}

void del_phys_ent(ecs_entity_t ent)
{
    struct contact* map = get_contact_map(ent);

    size_t len = hmlen(map);
    for (size_t i = 0; i < len; ++i) {
        struct contact* m = get_contact_map(map[i].value);
        hmdel(m, ent);
    }

    hmdel(contacts_map, ent);
}

#define K_SHAPE_EXPR                                                                               \
    "[in] game.comp.Position, [in] physics.WorldBounds, [in] PAIR | physics.Collider > %s"

void create_collider_queries(ecs_world_t* world, PhysQuery* query)
{
    char buffer[1024];
    int n = 0;

    if (query->sig) {
        n = snprintf(buffer, 1024, K_SHAPE_EXPR ", %s", "physics.Box", query->sig);
    } else {
        n = snprintf(buffer, 1024, K_SHAPE_EXPR, "physics.Box");
    }

    if (n < 0 || n >= 1024) {
        return;
    }

    query->boxes = ecs_query_new(world, buffer);
}

void CreatePhysicsQueries(ecs_iter_t* it)
{
    PhysQuery* query = ecs_term(it, PhysQuery, 1);

    if (ecs_is_owned(it, 1)) {
        for (int32_t i = 0; i < it->count; ++i) {
            create_collider_queries(it->world, &query[i]);
        }
    } else {
        PhysQuery* shared = ecs_term(it, PhysQuery, 2);
        if (shared && shared->boxes) {
            query->boxes = shared->boxes;
        } else {
            create_collider_queries(it->world, query);
        }
    }
}

void TestQueryContacts(ecs_iter_t* it)
{
    PhysQuery* query = ecs_term(it, PhysQuery, 1);
    PhysReceiver* receiver = ecs_term(it, PhysReceiver, 2);
    PhysWorldBounds* bounds = ecs_term(it, PhysWorldBounds, 3);
    PhysCollider* collider = ecs_term(it, PhysCollider, 4);
    ecs_entity_t trait = ecs_term_id(it, 4);
    ecs_entity_t comp = ecs_entity_t_lo(trait);
    Position* pos = ecs_term(it, Position, 5);

    if (!ecs_is_owned(it, 1)) {
        for (int32_t i = 0; i < it->count; ++i) {
            ecs_entity_t ent0 = it->entities[i];
            PhysReceiver* r = receiver;
            if (ecs_is_owned(it, 2)) {
                r = &receiver[i];
            }

            ecs_iter_t qit = ecs_query_iter(query->boxes);
            while (ecs_query_next(&qit)) {
                Position* other_pos = ecs_term(&qit, Position, 1);
                PhysWorldBounds* other_bounds = ecs_term(&qit, PhysWorldBounds, 2);
                for (int32_t j = 0; j < qit.count; ++j) {
                    ecs_entity_t ent1 = qit.entities[j];

                    if (phys_bounds_overlap(&bounds[i], &other_bounds[j])) {
                        bool started = add_contact(ent0, ent1);

                        if (started) {
                            if (r->on_contact_start) {
                                r->on_contact_start(it->world, ent0, ent1);
                            }
                        } else {
                            if (r->on_contact_continue) {
                                r->on_contact_continue(it->world, ent0, ent1);
                            }
                        }
                    } else {
                        bool ended = del_contact(ent0, ent1);

                        if (ended) {
                            if (r->on_contact_stop) {
                                r->on_contact_stop(it->world, ent0, ent1);
                            }
                        }
                    }
                }
            }
        }
    } else {
        for (int32_t i = 0; i < it->count; ++i) {
            ecs_entity_t ent0 = it->entities[i];
            PhysReceiver* r = receiver;
            if (ecs_is_owned(it, 2)) {
                r = &receiver[i];
            }

            ecs_iter_t qit = ecs_query_iter(query[i].boxes);
            while (ecs_query_next(&qit)) {
                Position* other_pos = ecs_term(&qit, Position, 1);
                PhysWorldBounds* other_bounds = ecs_term(&qit, PhysWorldBounds, 2);
                for (int32_t j = 0; j < qit.count; ++j) {
                    ecs_entity_t ent1 = qit.entities[j];

                    if (phys_bounds_overlap(&bounds[i], &other_bounds[j])) {
                        bool started = add_contact(ent0, ent1);

                        if (started) {
                            if (r->on_contact_start) {
                                r->on_contact_start(it->world, ent0, ent1);
                            }
                        } else {
                            if (r->on_contact_continue) {
                                r->on_contact_continue(it->world, ent0, ent1);
                            }
                        }
                    } else {
                        bool ended = del_contact(ent0, ent1);

                        if (ended) {
                            if (r->on_contact_stop) {
                                r->on_contact_stop(it->world, ent0, ent1);
                            }
                        }
                    }
                }
            }
        }
    }
}

typedef struct physics_debug_gui_context {
    ecs_entity_t box_collider_view_ent;
    ecs_entity_t world_bounds_view_ent;
} physics_debug_gui_context;

void physics_debug_gui(ecs_world_t* world, void* ctx)
{
    physics_debug_gui_context* context = (physics_debug_gui_context*)ctx;

    igText("Debug Toggles");
    {
        bool enabled = !ecs_has_entity(world, context->box_collider_view_ent, EcsDisabled);
        if (igCheckbox("Box Colliders", &enabled)) {
            ecs_enable(world, context->box_collider_view_ent, enabled);
        }
    }

    {
        bool enabled = !ecs_has_entity(world, context->world_bounds_view_ent, EcsDisabled);
        if (igCheckbox("World Bounds", &enabled)) {
            ecs_enable(world, context->world_bounds_view_ent, enabled);
        }
    }
}

void physics_fini(ecs_world_t* world, void* ctx)
{
    size_t len = hmlen(contacts_map);
    for (size_t i = 0; i < len; ++i) {
        hmfree(contacts_map[i].value);
    }
    hmfree(contacts_map);
}

void AttachBoxWorldBounds(ecs_iter_t* it)
{
    const Position* position = ecs_term(it, Position, 1);
    const PhysCollider* collider = ecs_term(it, PhysCollider, 2);
    ecs_id_t ecs_id(PhysCollider) = ecs_term_id(it, 2);
    ecs_id_t ecs_id(PhysWorldBounds) = ecs_term_id(it, 3);
    ecs_id_t ecs_id(PhysBox) = ecs_entity_t_lo(ecs_id(PhysCollider));

    for (int32_t i = 0; i < it->count; ++i) {
        const PhysBox* box = ecs_get_w_id(it->world, it->entities[i], ecs_id(PhysBox));
        PhysWorldBounds bounds;
        box_to_bounds(position[i], box->size, &bounds.left);

        ecs_set_ptr(it->world, it->entities[i], PhysWorldBounds, &bounds);
    }
}

void UpdateBoxWorldBounds(ecs_iter_t* it)
{
    const Position* position = ecs_term(it, Position, 1);
    const PhysCollider* collider = ecs_term(it, PhysCollider, 2);
    ecs_id_t ecs_id(PhysCollider) = ecs_term_id(it, 2);
    ecs_id_t ecs_id(PhysBox) = ecs_entity_t_lo(ecs_id(PhysCollider));

    PhysWorldBounds* bounds = ecs_term(it, PhysWorldBounds, 3);

    for (int32_t i = 0; i < it->count; ++i) {
        const PhysBox* box = ecs_get_w_id(it->world, it->entities[i], ecs_id(PhysBox));
        box_to_bounds(position[i], box->size, &bounds[i].left);
    }
}

void WorldBoundsView(ecs_iter_t* it)
{
    PhysWorldBounds* bounds = ecs_term(it, PhysWorldBounds, 1);

    draw_set_prim_layer(0.0f);

    for (int32_t i = 0; i < it->count; ++i) {
        vec4 col = k_color_spring;
        if (has_contact(it->entities[i])) col = k_color_violet;

        draw_line_rect_col(
            (vec2){bounds[i].left, bounds[i].top}, (vec2){bounds[i].right, bounds[i].bottom}, col);
    }
}

void contact_start(ecs_world_t* world, ecs_entity_t e0, ecs_entity_t e1)
{
    // const char* name0 = ecs_get_name(world, e0);
    // const char* name1 = ecs_get_name(world, e1);
    // ecs_logf(world, "start: %s(0x%08X), %s(0x%08X)", name0, e0, name1, e1);
}
void contact_continue(ecs_world_t* world, ecs_entity_t e0, ecs_entity_t e1)
{
    const char* name0 = ecs_get_name(world, e0);
    const char* name1 = ecs_get_name(world, e1);
    // ecs_logf(world, "stay: %s(0x%08X), %s(0x%08X)", name0, e0, name1, e1);
    // ecs_delete(world, e0);
    // ecs_delete(world, e1);
}
void contact_stop(ecs_world_t* world, ecs_entity_t e0, ecs_entity_t e1)
{
    const char* name0 = ecs_get_name(world, e0);
    const char* name1 = ecs_get_name(world, e1);
    // ecs_logf(world, "stop: %s(0x%08X), %s(0x%08X)", name0, e0, name1, e1);
    // ecs_delete(world, e0);
    // ecs_delete(world, e1);
}

struct physics_ent {
    ecs_entity_t ent;
    PhysWorldBounds bounds;
    uint8_t layer;
};

struct physics_ent* physics_ents = NULL;

void ClearColliders(ecs_iter_t* it)
{
    arrsetlen(physics_ents, 0);
}

void GatherColliders(ecs_iter_t* it)
{
    const PhysWorldBounds* bounds = ecs_term(it, PhysWorldBounds, 1);
    const PhysCollider* collider = ecs_term(it, PhysCollider, 3);

    for (int32_t i = 0; i < it->count; ++i) {
        arrput(
            physics_ents,
            ((struct physics_ent){
                .ent = it->entities[i],
                .bounds = bounds[i],
                .layer = collider[i].layer,
            }));
    }
}

void UpdateContactEvents(ecs_iter_t* it)
{
    PhysReceiver* r = ecs_term(it, PhysReceiver, 1);

    int32_t len = (int32_t)arrlen(physics_ents);
    for (int32_t i = 0; i < len - 1; ++i) {
        for (int32_t j = i + 1; j < len; ++j) {
            struct physics_ent* a = &physics_ents[i];
            struct physics_ent* b = &physics_ents[j];

            if (a->layer == b->layer) {
                continue;
            }

            ecs_entity_t ent0 = a->ent;
            ecs_entity_t ent1 = b->ent;

            if (phys_bounds_overlap(&a->bounds, &b->bounds)) {
                bool started = add_contact(ent0, ent1);

                if (started) {
                    // ecs_logf(
                    //     it->world,
                    //     "[a] l: %0.2f r: %0.2f t: %0.2f b: %0.2f",
                    //     a->bounds.left,
                    //     a->bounds.right,
                    //     a->bounds.top,
                    //     a->bounds.bottom);
                    // ecs_logf(
                    //     it->world,
                    //     "[b] l: %0.2f r: %0.2f t: %0.2f b: %0.2f",
                    //     b->bounds.left,
                    //     b->bounds.right,
                    //     b->bounds.top,
                    //     b->bounds.bottom);
                    if (r->on_contact_start) {
                        if (ecs_filter_match_entity(it->world, &r->filter, ent0)) {
                            r->on_contact_start(it->world, ent0, ent1);
                        }
                        if (ecs_filter_match_entity(it->world, &r->filter, ent1)) {
                            r->on_contact_start(it->world, ent1, ent0);
                        }
                    }
                } else {
                    if (r->on_contact_continue) {
                        r->on_contact_continue(it->world, ent0, ent1);
                    }
                }
            } else {
                bool ended = del_contact(ent0, ent1);

                if (ended) {
                    if (r->on_contact_stop) {
                        r->on_contact_stop(it->world, ent0, ent1);
                    }
                }
            }
        }
    }
}

void RemovePhysEnt(ecs_iter_t* it)
{
    for (int32_t i = 0; i < it->count; ++i) {
        del_phys_ent(it->entities[i]);
    }
}

void PhysicsImport(ecs_world_t* world)
{
    ECS_MODULE(world, Physics);

    ecs_atfini(world, physics_fini, NULL);

    ecs_set_name_prefix(world, "Phys");
    ECS_COMPONENT(world, PhysQuery);
    ECS_COMPONENT(world, PhysReceiver);
    ECS_COMPONENT(world, PhysCollider);
    ECS_COMPONENT(world, PhysBox);
    ECS_COMPONENT(world, PhysWorldBounds);

    ECS_TAG(world, ClearPhysEnts);
    ECS_ENTITY(world, ClearEnt, ClearPhysEnts);

    // clang-format off
    ECS_SYSTEM(world, BoxColliderView, EcsPreStore,
        game.comp.Position, PAIR | physics.Collider > physics.Box);
    ECS_SYSTEM(world, WorldBoundsView, EcsPreStore, [in] physics.WorldBounds);
    
    ECS_SYSTEM(world, CreatePhysicsQueries, EcsOnSet, ANY:physics.Query, ?SHARED:physics.Query);
    
    // ECS_SYSTEM(world, TestQueryContacts, EcsPostUpdate,
    //     [in] ANY:physics.Query, [in] ANY:physics.Receiver,
    //     [in] physics.WorldBounds,
    //     [in] PAIR | physics.Collider, game.comp.Position);
    ECS_SYSTEM(world, ClearColliders, EcsPreUpdate, ClearPhysEnts);
    ECS_SYSTEM(world, GatherColliders, EcsOnValidate, physics.WorldBounds, game.comp.Position, [in] PAIR | physics.Collider);
    ECS_SYSTEM(world, UpdateContactEvents, EcsPreStore, SYSTEM:PhysReceiver);
    ECS_SYSTEM(world, RemovePhysEnt, EcsUnSet, physics.WorldBounds, game.comp.Position, [in] PAIR | physics.Collider);
    
    ECS_SYSTEM(world, AttachBoxWorldBounds, EcsOnSet, [in] game.comp.Position, [in] PAIR | physics.Collider > physics.Box, [out] !physics.WorldBounds);
    ECS_SYSTEM(world, UpdateBoxWorldBounds, EcsPostUpdate, game.comp.Position, [in] PAIR | physics.Collider > physics.Box, [out] physics.WorldBounds);
    // clang-format on

    ecs_filter_t filter;
    ecs_filter_init(
        world,
        &filter,
        &(ecs_filter_desc_t){
            .expr = "ANY:game.comp.Highlight",
        });

    ecs_set(
        world,
        UpdateContactEvents,
        PhysReceiver,
        {
            .on_contact_start = contact_start,
            .on_contact_continue = contact_continue,
            .on_contact_stop = contact_stop,
            .filter = filter,
        });

    ecs_enable(world, BoxColliderView, false);

    ECS_IMPORT(world, DebugGui);

    DEBUG_PANEL(
        world,
        PhysicsDebug,
        "shift+3",
        physics_debug_gui,
        physics_debug_gui_context,
        {
            .box_collider_view_ent = BoxColliderView,
            .world_bounds_view_ent = WorldBoundsView,
        });

    ECS_EXPORT_COMPONENT(PhysQuery);

    ECS_EXPORT_COMPONENT(PhysReceiver);
    ECS_EXPORT_COMPONENT(PhysCollider);
    ECS_EXPORT_COMPONENT(PhysBox);
}