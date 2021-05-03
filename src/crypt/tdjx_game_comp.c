#include "tdjx_game_comp.h"

void TdjxGameCompImport(ecs_world_t* world)
{
    ECS_MODULE(world, TdjxGameComp);

    ecs_set_name_prefix(world, "Tdjx");

    ECS_COMPONENT(world, Position);
    ECS_COMPONENT(world, Velocity);

    ECS_EXPORT_COMPONENT(Position);
    ECS_EXPORT_COMPONENT(Velocity);
}