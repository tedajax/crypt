#include "physics.h"
#include "game_components.h"
#include "sprite_renderer.h"

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
    ECS_IMPORT(it->world, GameComp);

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

void PhysicsImport(ecs_world_t* world)
{
    ECS_MODULE(world, Physics);

    ECS_COMPONENT(world, BoxCollider);
    ECS_COMPONENT(world, ColliderQuery);

    // clang-format off
    ECS_SYSTEM(world, CheckColliderIntersections, EcsPostUpdate, game.comp.Position, SHARED:physics.BoxCollider, SHARED:physics.ColliderQuery);
    ECS_SYSTEM(world, ColliderView, EcsPostUpdate, game.comp.Position, ANY:physics.BoxCollider);
    // clang-format on

    ECS_EXPORT_COMPONENT(BoxCollider);
    ECS_EXPORT_COMPONENT(ColliderQuery);
}