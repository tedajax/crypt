#include "physics.h"
#include "debug_gui.h"
#include "game_components.h"
#include "sprite_renderer.h"
#include "stb_ds.h"
#include <ccimgui.h>

bool phys_bounds_overlap(const PhysWorldBounds* a, const PhysWorldBounds* b)
{
    return a->left <= b->right && a->right >= b->left && a->top >= b->bottom && a->bottom <= b->top;
}

void box_to_bounds(vec2 center, vec2 size, float bounds[4])
{
    bounds[0] = center.x - size.x;
    bounds[1] = center.x + size.x;
    bounds[2] = center.y + size.y;
    bounds[3] = center.y - size.y;
}

/////////////////////////////////////////////////
// Entities in contact mapping
// --------------------------------
// Maps all entities currently in contact with each other.
// relationship is map<ent, set<ent>> where set is expressed as an stb_ds.h hashmap of ent->bool
// which prevents duplicate ents being tracked.
// Manipulation of the contacts_map should be done through the contact_map_(add/del)_contact
// functions.

struct contact_ent {
    ecs_entity_t key;
    bool value;
};

struct ent_contacts {
    ecs_entity_t key;
    struct contact_ent* value; // map<entity, bool>
};

struct ent_contacts* contacts_map = NULL; // map<entity, map<entity, bool>>

void contacts_map_free(void)
{
    size_t len = hmlen(contacts_map);
    for (size_t i = 0; i < len; ++i) {
        hmfree(contacts_map[i].value);
    }
    hmfree(contacts_map);
}

struct contact_ent* contact_map_get_ent_contact_set(ecs_entity_t ent)
{
    if (hmgeti(contacts_map, ent) < 0) {
        struct contact_ent* contact_map = NULL;
        hmdefault(contact_map, false);
        hmput(contacts_map, ent, contact_map);
        return contact_map;
    }

    return hmget(contacts_map, ent);
}

bool contact_map_add_contact(ecs_entity_t e0, ecs_entity_t e1)
{
    struct contact_ent* contact_set0 = contact_map_get_ent_contact_set(e0);
    struct contact_ent* contact_set1 = contact_map_get_ent_contact_set(e1);

    TX_ASSERT(hmget(contact_set0, e1) == hmget(contact_set1, e0));

    if (hmget(contact_set0, e1) == false) {
        hmput(contact_set0, e1, true);
        hmput(contacts_map, e0, contact_set0);
        hmput(contact_set1, e0, true);
        hmput(contacts_map, e1, contact_set1);
        return true;
    }

    return false;
}

bool contact_map_del_contact(ecs_entity_t e0, ecs_entity_t e1)
{
    struct contact_ent* contact_set0 = contact_map_get_ent_contact_set(e0);
    struct contact_ent* contact_set1 = contact_map_get_ent_contact_set(e1);

    TX_ASSERT(hmget(contact_set0, e1) == hmget(contact_set1, e0));

    if (hmget(contact_set0, e1) == true) {
        hmdel(contact_set0, e1);
        hmput(contacts_map, e0, contact_set0);
        hmdel(contact_set1, e0);
        hmput(contacts_map, e1, contact_set1);
        return true;
    }

    return false;
}

bool contact_map_ents_in_contact(ecs_entity_t e0, ecs_entity_t e1)
{
    struct contact_ent* contact_set0 = contact_map_get_ent_contact_set(e0);
    struct contact_ent* contact_set1 = contact_map_get_ent_contact_set(e1);

    TX_ASSERT(hmget(contact_set0, e1) == hmget(contact_set1, e0));

    return hmget(contact_set0, e1) == true;
}

bool contact_map_ent_has_contacts(ecs_entity_t ent)
{
    struct contact_ent* contact_set = contact_map_get_ent_contact_set(ent);
    return hmlen(contact_set) > 0;
}

int32_t contact_map_remove_ent(ecs_entity_t removed, ecs_entity_t* contacts, int32_t n)
{
    int32_t ret = 0;

    struct contact_ent* removed_set = contact_map_get_ent_contact_set(removed);
    size_t len = hmlen(removed_set);
    for (size_t i = 0; i < len; ++i) {
        ecs_entity_t contacted = removed_set[i].key;
        struct contact_ent* contacted_set = contact_map_get_ent_contact_set(contacted);

        if (contacts && ret < n) {
            contacts[ret++] = contacted;
        }

        hmdel(contacted_set, removed);
        hmput(contacts_map, contacted, contacted_set);
    }

    hmdel(contacts_map, removed);

    return ret;
}

struct physics_ent {
    ecs_entity_t ent;
    PhysWorldBounds bounds;
    uint8_t layer;
};

void on_contact_start(ecs_iter_t* it, PhysReceiver* r, ecs_entity_t e0, ecs_entity_t e1);
void on_contact_continue(ecs_iter_t* it, PhysReceiver* r, ecs_entity_t e0, ecs_entity_t e1);
void on_contact_stop(ecs_iter_t* it, PhysReceiver* r, ecs_entity_t e0, ecs_entity_t e1);
void on_remove_ent(ecs_iter_t* it, PhysReceiver* r, ecs_entity_t e0, ecs_entity_t e1);

typedef enum contact_type {
    ContactType_None,
    ContactType_Start,
    ContactType_Continue,
    ContactType_Stop,
    ContactType_Remove, // Only ent0 is used in this case
    ContactType_Count,
} contact_type;

typedef void (*notify_receiver_action_t)(ecs_iter_t*, PhysReceiver*, ecs_entity_t, ecs_entity_t);
static const notify_receiver_action_t contact_type_notify_actions[ContactType_Count] = {
    NULL,
    on_contact_start,
    on_contact_continue,
    on_contact_stop,
    on_remove_ent,
};

const char* contact_type_names[ContactType_Count] = {
    "None",
    "Start",
    "Continue",
    "Stop",
    "Remove",
};

struct ent_contact {
    ecs_entity_t ent0, ent1;
};

struct physics_ent* physics_ents = NULL;
struct ent_contact* contact_queues[ContactType_Count] = {0};

void contact_queues_init(void)
{
    // Skip None, no need for a queue
    for (int i = ContactType_Start; i < ContactType_Count; ++i) {
        arrsetcap(contact_queues[i], 256);
    }
}

void contact_queues_free(void)
{
    for (int i = ContactType_Start; i < ContactType_Count; ++i) {
        arrfree(contact_queues[i]);
    }
}

void contact_queues_push(ecs_entity_t e0, ecs_entity_t e1, contact_type type)
{
    if (type > ContactType_None && type < ContactType_Count) {
        arrput(contact_queues[type], ((struct ent_contact){.ent0 = e0, .ent1 = e1}));
    }
}

void contact_queues_push_ent_remove(ecs_entity_t ent)
{
    contact_queues_push(ent, 0, ContactType_Remove);
}

// TODO: Surely I can figure out a more ECS way to do this...
void on_contact_start(ecs_iter_t* it, PhysReceiver* r, ecs_entity_t e0, ecs_entity_t e1)
{
    for (int32_t i = 0; i < it->count; ++i) {
        if (ecs_filter_match_entity(it->world, &r[i].filter, e0)) {
            r[i].on_contact_start(it->world, e0, e1);
        }
        if (ecs_filter_match_entity(it->world, &r[i].filter, e1)) {
            r[i].on_contact_start(it->world, e1, e0);
        }
    }
}

void on_contact_continue(ecs_iter_t* it, PhysReceiver* r, ecs_entity_t e0, ecs_entity_t e1)
{
    for (int32_t i = 0; i < it->count; ++i) {
        if (ecs_filter_match_entity(it->world, &r[i].filter, e0)) {
            r[i].on_contact_continue(it->world, e0, e1);
        }
        if (ecs_filter_match_entity(it->world, &r[i].filter, e1)) {
            r[i].on_contact_continue(it->world, e1, e0);
        }
    }
}

void on_contact_stop(ecs_iter_t* it, PhysReceiver* r, ecs_entity_t e0, ecs_entity_t e1)
{
    for (int32_t i = 0; i < it->count; ++i) {
        if (ecs_filter_match_entity(it->world, &r[i].filter, e0)) {
            r[i].on_contact_stop(it->world, e0, e1);
        }
        if (ecs_filter_match_entity(it->world, &r[i].filter, e1)) {
            r[i].on_contact_stop(it->world, e1, e0);
        }
    }
}

void on_remove_ent(ecs_iter_t* it, PhysReceiver* r, ecs_entity_t e0, ecs_entity_t e1)
{
    ecs_entity_t removed = e0; // the entity that was deleted, we're going to notify entities in
                               // contact with the deleted entity that the contact has stopped.
    ecs_entity_t nothing = e1; // e1 is only here to match the function pointer signature

    // queue for storing entities we'll need to notify
    ecs_entity_t notify_queue[32] = {0};

    // update contact map and add contacted entities to the queue
    int32_t notify_len = contact_map_remove_ent(removed, notify_queue, 32);

    // iterate the queue
    for (int32_t n = 0; n < notify_len; ++n) {
        ecs_entity_t notify = notify_queue[n];
        for (int32_t i = 0; i < it->count; ++i) {
            if (ecs_filter_match_entity(it->world, &r[i].filter, notify)) {
                r[i].on_contact_stop(it->world, notify, removed);
            }
        }
    }
}

void PhysicsNewFrame(ecs_iter_t* it)
{
    arrsetlen(physics_ents, 0);

    for (int i = ContactType_Start; i < ContactType_Count; ++i) {
        arrsetlen(contact_queues[i], 0);
    }
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
            contact_type type = ContactType_None;

            if (phys_bounds_overlap(&a->bounds, &b->bounds)) {
                bool started = contact_map_add_contact(ent0, ent1);

                if (started) {
                    type = ContactType_Start;
                } else {
                    type = ContactType_Continue;
                }
            } else {
                bool ended = contact_map_del_contact(ent0, ent1);

                if (ended) {
                    type = ContactType_Stop;
                }
            }
        }
    }
}

void ProcessContactQueues(ecs_iter_t* it)
{
    PhysReceiver* r = ecs_term(it, PhysReceiver, 1);

    for (contact_type qtype = ContactType_Start; qtype < ContactType_Count; ++qtype) {
        struct ent_contact* queue = contact_queues[qtype];
        size_t len = arrlen(queue);
        for (size_t i = 0; i < len; ++i) {
            ecs_entity_t ent0 = queue[i].ent0, ent1 = queue[i].ent1;
            contact_type_notify_actions[qtype](it, r, ent0, ent1);
        }
    }
}

void RemovePhysEnt(ecs_iter_t* it)
{
    for (int32_t i = 0; i < it->count; ++i) {
        contact_queues_push_ent_remove(it->entities[i]);
    }
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
        if (contact_map_ent_has_contacts(it->entities[i])) {
            ecs_entity_t asdfasdf = contact_map_ent_has_contacts(it->entities[i]);
            col = k_color_violet;
        }

        draw_line_rect_col(
            (vec2){bounds[i].left, bounds[i].top}, (vec2){bounds[i].right, bounds[i].bottom}, col);
    }
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

    contacts_map_free();
    contact_queues_free();
}

void PhysicsImport(ecs_world_t* world)
{
    ECS_MODULE(world, Physics);

    contact_queues_init();

    ecs_atfini(world, physics_fini, NULL);

    ecs_set_name_prefix(world, "Phys");
    ECS_COMPONENT(world, PhysReceiver);
    ECS_COMPONENT(world, PhysCollider);
    ECS_COMPONENT(world, PhysBox);
    ECS_COMPONENT(world, PhysWorldBounds);

    ECS_TAG(world, ClearPhysEnts);
    ECS_ENTITY(world, ClearEnt, ClearPhysEnts);

    // clang-format off
    // Basic physics (really collision right now) pipeline
    // - Reset internal state for new frame
    // - Gather colliders into internal list of colliders
    // - Process colliders contact state and push contact events to the relevant queues
    // - Process contact events and fire callbacks on receivers.
    ECS_SYSTEM(world, PhysicsNewFrame, EcsPreUpdate, ClearPhysEnts);
    ECS_SYSTEM(world, GatherColliders, EcsOnValidate, physics.WorldBounds, game.comp.Position, [in] PAIR | physics.Collider);
    ECS_SYSTEM(world, UpdateContactEvents, EcsPostValidate, :UpdateContactEvents);
    ECS_SYSTEM(world, ProcessContactQueues, EcsPreStore, OWNED:physics.Receiver);

    // When an entity no longer matches the physics world remove it and fire appropriate contact events
    ECS_SYSTEM(world, RemovePhysEnt, EcsUnSet, physics.WorldBounds, game.comp.Position, [in] PAIR | physics.Collider);
    
    // Create and update world bounds for box colliders, other collider types would need similar systems
    ECS_SYSTEM(world, AttachBoxWorldBounds, EcsOnSet, [in] game.comp.Position, [in] PAIR | physics.Collider > physics.Box, [out] !physics.WorldBounds);
    ECS_SYSTEM(world, UpdateBoxWorldBounds, EcsPostUpdate, game.comp.Position, [in] PAIR | physics.Collider > physics.Box, [out] physics.WorldBounds);
    
    // Debug view systems
    ECS_SYSTEM(world, BoxColliderView, EcsPreStore,
        game.comp.Position, PAIR | physics.Collider > physics.Box);
    ECS_SYSTEM(world, WorldBoundsView, EcsPreStore, [in] physics.WorldBounds);
    // clang-format on

    ecs_enable(world, BoxColliderView, false);
    ecs_enable(world, WorldBoundsView, false);

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

    ECS_EXPORT_COMPONENT(PhysReceiver);
    ECS_EXPORT_COMPONENT(PhysCollider);
    ECS_EXPORT_COMPONENT(PhysBox);
}
