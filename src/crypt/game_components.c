#include "game_components.h"

void GameCompImport(ecs_world_t* world)
{
    ECS_MODULE(world, GameComp);

    ECS_COMPONENT(world, Position);
    ECS_COMPONENT(world, LocalPosition);
    ECS_COMPONENT(world, Velocity);
    ECS_TAG(world, Highlight);

    ECS_EXPORT_COMPONENT(Position);
    ECS_EXPORT_COMPONENT(LocalPosition);
    ECS_EXPORT_COMPONENT(Velocity);
    ECS_EXPORT_ENTITY(Highlight);
}