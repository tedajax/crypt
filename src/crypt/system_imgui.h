#pragma once

#include "flecs.h"

typedef struct ImGuiIO ImGuiIO;

typedef struct ImguiDesc {
    int dummy;
} ImguiDesc;

typedef struct ImguiContext {
    ImGuiIO* io;
} ImguiContext;

typedef struct SystemImgui {
    ECS_DECLARE_COMPONENT(ImguiContext);
    ECS_DECLARE_COMPONENT(ImguiDesc);
} SystemImgui;

void SystemImguiImport(ecs_world_t* world);

#define SystemImguiImportHandles(handles)                                                          \
    ECS_IMPORT_COMPONENT(handles, ImguiContext);                                                   \
    ECS_IMPORT_COMPONENT(handles, ImguiDesc);