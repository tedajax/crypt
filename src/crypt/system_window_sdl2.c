#include "system_window_sdl2.h"

#include <SDL2/SDL.h>

static void Sdl2CreateWindow(ecs_iter_t* it)
{
    WindowDesc* window_desc = ecs_column(it, WindowDesc, 1);
    ecs_entity_t ecs_typeid(Sdl2Window) = ecs_column_entity(it, 2);

    for (int i = 0; i < it->count; ++i) {
        ecs_entity_t e = it->entities[i];

        const char* title = (window_desc[i].title) ? window_desc[i].title : "SDL2 Window";

        {
            SDL_Window* window_ptr = SDL_CreateWindow(
                title,
                SDL_WINDOWPOS_CENTERED,
                SDL_WINDOWPOS_CENTERED,
                window_desc->width,
                window_desc->height,
                SDL_WINDOW_OPENGL);

            if (!window_ptr) {
                ecs_err("SDL2 window creation failed: %s", SDL_GetError());
                break;
            }

            ecs_set(
                it->world,
                e,
                Sdl2Window,
                {
                    .window = window_ptr,
                });
        }
    }
}

static void Sdl2DestroyWindow(ecs_iter_t* it)
{
    Sdl2Window* window = ecs_column(it, Sdl2Window, 1);

    for (int i = 0; i < it->count; ++i) {
        SDL_DestroyWindow(window->window);
    }
}

void SystemSdl2WindowImport(ecs_world_t* world)
{
    ECS_MODULE(world, SystemSdl2Window);

    ecs_set_name_prefix(world, "Sdl2");

    ECS_COMPONENT(world, WindowDesc);
    ECS_COMPONENT(world, Sdl2Window);

    // clang-format off
    ECS_SYSTEM(world, Sdl2CreateWindow, EcsOnSet, 
        [in] system.sdl2.window.WindowDesc,
        [out] :system.sdl2.window.Window);
    // clang-format on

    ECS_SYSTEM(world, Sdl2DestroyWindow, EcsUnSet, Sdl2Window);

    ECS_EXPORT_COMPONENT(WindowDesc);
}