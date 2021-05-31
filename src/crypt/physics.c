#include "physics.h"
#include "debug_gui.h"
#include "game_components.h"
#include "sprite_renderer.h"
#include "stb_ds.h"
#include <ccimgui.h>

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

bool add_contact(ecs_entity_t ent, ecs_entity_t contact)
{
    struct contact* contact_map = get_contact_map(ent);
    if (hmget(contact_map, contact) == false) {
        hmput(contact_map, contact, true);
        hmput(contacts_map, ent, contact_map);
        return true;
    }
    return false;
}

bool del_contact(ecs_entity_t ent, ecs_entity_t contact)
{
    struct contact* contact_map = get_contact_map(ent);

    if (hmget(contact_map, contact) == true) {
        hmput(contact_map, contact, false);
        hmput(contacts_map, ent, contact_map);
        return true;
    }
    return false;
}

#define K_SHAPE_EXPR "[in] game.comp.Position, [in] PAIR | physics.Collider > %s"

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
        create_collider_queries(it->world, query);
    }
}

void TestQueryContacts(ecs_iter_t* it)
{
    PhysQuery* query = ecs_term(it, PhysQuery, 1);
    PhysReceiver* receiver = ecs_term(it, PhysReceiver, 2);
    PhysCollider* collider = ecs_term(it, PhysCollider, 3);
    ecs_entity_t trait = ecs_term_id(it, 3);
    ecs_entity_t comp = ecs_entity_t_lo(trait);
    Position* pos = ecs_term(it, Position, 4);

    for (int32_t i = 0; i < it->count; ++i) {
        const PhysBox* box0 = ecs_get_w_id(it->world, it->entities[i], comp);
        if (!box0) {
            continue;
        }

        struct world_rect rect0;
        box_to_world_rect(pos[i], box0->size, &rect0);

        PhysQuery* q = query;
        if (ecs_is_owned(it, 1)) {
            q = &query[i];
        }

        PhysReceiver* r = receiver;
        if (ecs_is_owned(it, 2)) {
            r = &receiver[i];
        }

        ecs_iter_t qit = ecs_query_iter(q->boxes);
        while (ecs_query_next(&qit)) {
            Position* pos1 = ecs_term(&qit, Position, 1);
            for (int32_t j = 0; j < qit.count; ++j) {
                const PhysBox* box1 = ecs_get_w_id(it->world, qit.entities[j], comp);
                struct world_rect rect1;
                box_to_world_rect(pos1[j], box1->size, &rect1);

                ecs_entity_t ent0 = it->entities[i];
                ecs_entity_t ent1 = qit.entities[j];

                if (world_rect_overlap(&rect0, &rect1)) {
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

typedef struct physics_debug_gui_context {
    ecs_entity_t debug_view_ent;
} physics_debug_gui_context;

void physics_debug_gui(ecs_world_t* world, void* ctx)
{
    physics_debug_gui_context* context = (physics_debug_gui_context*)ctx;

    bool enabled = !ecs_has_entity(world, context->debug_view_ent, EcsDisabled);

    if (igCheckbox("Debug Colliders", &enabled)) {
        ecs_enable(world, context->debug_view_ent, enabled);
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

void PhysicsImport(ecs_world_t* world)
{
    ECS_MODULE(world, Physics);

    ecs_atfini(world, physics_fini, NULL);

    ecs_set_name_prefix(world, "Phys");

    ECS_COMPONENT(world, PhysQuery);
    ECS_COMPONENT(world, PhysReceiver);
    ECS_COMPONENT(world, PhysCollider);
    ECS_COMPONENT(world, PhysBox);

    // clang-format off
    ECS_SYSTEM(world, BoxColliderView, EcsPostUpdate,
        game.comp.Position, PAIR | physics.Collider > physics.Box);
    ECS_SYSTEM(world, CreatePhysicsQueries, EcsOnSet, ANY:physics.Query);
    ECS_SYSTEM(world, TestQueryContacts, EcsOnValidate,
        [in] ANY:physics.Query, [in] ANY:physics.Receiver,
        [in] PAIR | physics.Collider, game.comp.Position);
    // clang-format on

    ecs_enable(world, BoxColliderView, false);

    ECS_IMPORT(world, DebugGui);

    DEBUG_PANEL(
        world,
        PhysicsDebug,
        "shift+3",
        physics_debug_gui,
        physics_debug_gui_context,
        {.debug_view_ent = BoxColliderView});

    ECS_EXPORT_COMPONENT(PhysQuery);

    ECS_EXPORT_COMPONENT(PhysReceiver);
    ECS_EXPORT_COMPONENT(PhysCollider);
    ECS_EXPORT_COMPONENT(PhysBox);
}