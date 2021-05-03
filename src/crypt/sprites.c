#include "sprites.h"

void TdjxSystemsSpritesImport(ecs_world_t* world)
{
    ECS_MODULE(world, TdjxSystemsSprites);

    ecs_entity_t scope = ecs_lookup_fullpath(world, "tdjx.systems.sprites.renderer");
    ecs_set_scope(world, scope);

    ecs_set_name_prefix(world, "Tdjx");

    ECS_COMPONENT(world, TdjxSprite);
}