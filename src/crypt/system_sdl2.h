#pragma once

#include "flecs.h"

typedef struct SDL_Window SDL_Window;
typedef void* SDL_GLContext;
typedef union SDL_Event SDL_Event;

typedef struct Sdl2Window {
    SDL_Window* window;
} Sdl2Window;

typedef struct Sdl2GlContext {
    SDL_GLContext gl;
} Sdl2GlContext;

typedef void (*process_sdl_event_fn)(const SDL_Event* event);

typedef struct Sdl2Input {
    process_sdl_event_fn event_fn;
} Sdl2Input;

typedef struct WindowConfig {
    const char* title;
    int32_t width;
    int32_t height;
    bool fullscreen;
} WindowConfig;

typedef struct SystemSdl2 {
    ECS_DECLARE_ENTITY(Sdl2);
    ECS_DECLARE_COMPONENT(Sdl2Input);
    ECS_DECLARE_COMPONENT(WindowConfig);
} SystemSdl2;

void SystemSdl2Import(ecs_world_t* world);

#define SystemSdl2ImportHandles(handles)                                                           \
    ECS_IMPORT_ENTITY(handles, Sdl2);                                                              \
    ECS_IMPORT_COMPONENT(handles, Sdl2Input);                                                      \
    ECS_IMPORT_COMPONENT(handles, WindowConfig);