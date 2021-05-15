#include "system_sdl2.h"

#include "tx_input.h"
#include <SDL2/SDL.h>

void sdl2_fini(ecs_world_t* world, void* ctx)
{
    SDL_Quit();
}

static void Sdl2ProcessEvents(ecs_iter_t* it)
{
    txinp_update();

    for (int i = 0; i < it->count; ++i) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            // ImGui_ImplSDL2_ProcessEvent(&event);
            switch (event.type) {
            case SDL_QUIT:
                ecs_quit(it->world);
                break;

            case SDL_KEYDOWN:
                if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                    ecs_quit(it->world);
                    break;
                }

                txinp_on_key_event((txinp_event_key){
                    .key = (txinp_key)event.key.keysym.scancode,
                    .is_down = true,
                });
                break;

            case SDL_KEYUP:
                txinp_on_key_event((txinp_event_key){
                    .key = (txinp_key)event.key.keysym.scancode,
                    .is_down = false,
                });
                break;
            }
        }
    }
}

static void Sdl2CreateWindow(ecs_iter_t* it)
{
    WindowConfig* window_desc = ecs_term(it, WindowConfig, 1);
    ecs_entity_t ecs_typeid(Sdl2Window) = ecs_term_id(it, 2);

    for (int i = 0; i < it->count; ++i) {
        ecs_entity_t e = it->entities[i];

        const char* title = (window_desc[i].title) ? window_desc[i].title : "SDL2 Window";

        {
            uint32_t flags = SDL_WINDOW_OPENGL;

            if (window_desc->fullscreen) {
                flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
            }

            SDL_Window* window_ptr = SDL_CreateWindow(
                title,
                SDL_WINDOWPOS_CENTERED,
                SDL_WINDOWPOS_CENTERED,
                window_desc->width,
                window_desc->height,
                flags);

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
    Sdl2Window* window = ecs_term(it, Sdl2Window, 1);

    for (int i = 0; i < it->count; ++i) {
        SDL_DestroyWindow(window->window);
    }
}

void SystemSdl2Import(ecs_world_t* world)
{
    ECS_MODULE(world, SystemSdl2);
    ecs_set_name_prefix(world, "Sdl2");

    ecs_atfini(world, sdl2_fini, NULL);

    ECS_TAG(world, Sdl2Input);

    ECS_SYSTEM(world, Sdl2ProcessEvents, EcsOnLoad, Sdl2Input);

    ECS_ENTITY(world, Sdl2, Sdl2Input);

    ECS_EXPORT_ENTITY(Sdl2);

    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        ecs_err("Unable to initialize SDL: %s", SDL_GetError());
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    txinp_init();
    ecs_trace_1("SDL Initialized");

    ECS_COMPONENT(world, WindowConfig);
    ECS_COMPONENT(world, Sdl2Window);

    // clang-format off
    ECS_SYSTEM(world, Sdl2CreateWindow, EcsOnSet, 
        [in] system.sdl2.WindowConfig,
        [out] :system.sdl2.Window);
    // clang-format on

    ECS_SYSTEM(world, Sdl2DestroyWindow, EcsUnSet, Sdl2Window);

    ECS_EXPORT_COMPONENT(WindowConfig);
}