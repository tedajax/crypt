#include "game_components.h"

void GameCompImport(ecs_world_t* world)
{
    ECS_MODULE(world, GameComp);

    ecs_set_name_prefix(world, "Tdjx");

    ECS_COMPONENT(world, Position);
    ECS_COMPONENT(world, Velocity);

    ECS_EXPORT_COMPONENT(Position);
    ECS_EXPORT_COMPONENT(Velocity);
}