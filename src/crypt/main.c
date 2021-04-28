#include "system_sdl2.h"
#include "system_window_sdl2.h"

int main(int argc, char* argv[])
{
    ecs_world_t* world = ecs_init_w_args(argc, argv);
    ecs_set_target_fps(world, 144.0f);

    ECS_IMPORT(world, SystemSdl2);
    ECS_IMPORT(world, SystemSdl2Window);

    ecs_entity_t window =
        ecs_set(world, 0, WindowDesc, {.title = "crypt", .width = 1280, .height = 720});

    while (ecs_progress(world, 0.0f)) {
    }

    return ecs_fini(world);
}