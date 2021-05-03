#pragma once

#include "flecs.h"

typedef struct SDL_Window SDL_Window;
typedef void* SDL_GL_Context;

typedef struct Sdl2Window {
    SDL_Window* window;
} Sdl2Window;

typedef struct WindowDesc {
    const char* title;
    int32_t width;
    int32_t height;
} WindowDesc;

typedef struct SystemSdl2Window {
    ECS_DECLARE_COMPONENT(WindowDesc);
    ECS_DECLARE_COMPONENT(Sdl2Window);
} SystemSdl2Window;

void SystemSdl2WindowImport(ecs_world_t* world);

#define SystemSdl2WindowImportHandles(handles)                                                     \
    ECS_IMPORT_COMPONENT(handles, WindowDesc);                                                     \
    ECS_IMPORT_COMPONENT(handles, Sdl2Window);