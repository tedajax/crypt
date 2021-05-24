#include "system_imgui.h"
#include "sprite_renderer.h"
#include "system_sdl2.h"
#include <SDL2/SDL.h>
#include <ccimgui.h>
#include <cimgui_impl.h>

static void AttachImgui(ecs_iter_t* it)
{
    ecs_id_t ecs_typeid(Sdl2Window) = ecs_term_id(it, 1);
    ecs_id_t ecs_typeid(Sdl2GlContext) = ecs_term_id(it, 2);
    ecs_entity_t ecs_typeid(ImguiContext) = ecs_term_id(it, 4);

    for (int32_t i = 0; i < it->count; ++i) {
        ecs_entity_t ent = it->entities[i];

        const Sdl2Window* window = ecs_get(it->world, ent, Sdl2Window);
        const Sdl2GlContext* gl = ecs_get(it->world, ent, Sdl2GlContext);

        igCreateContext(NULL);
        ImGuiIO* imgui = igGetIO();
        imgui->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui_ImplSDL2_InitForOpenGL(window->window, gl->gl);
        ImGui_ImplOpenGL3_Init(NULL);
        igStyleColorsDark(NULL);

        ecs_set(it->world, ent, ImguiContext, {.io = imgui});
    }
}

static void DetachImgui(ecs_iter_t* it)
{
}

static void ImguiNewFrame(ecs_iter_t* it)
{
    Sdl2Window* window = ecs_term(it, Sdl2Window, 1);
    ImguiContext* imgui = ecs_term(it, ImguiContext, 2);

    for (int32_t i = 0; i < it->count; ++i) {
        int win_w, win_h;
        SDL_GetWindowSize(window[i].window, &win_w, &win_h);
        imgui[i].io->DisplaySize = (ImVec2){.x = (float)win_w, .y = (float)win_h};
        imgui[i].io->DeltaTime = it->delta_time;
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(window->window);
        igNewFrame();
    }
}

static void ImguiRender(ecs_iter_t* it)
{
    igRender();
    ImGui_ImplOpenGL3_RenderDrawData(igGetDrawData());
}

void SystemImguiImport(ecs_world_t* world)
{
    ECS_MODULE(world, SystemImgui);

    ECS_IMPORT(world, SystemSdl2);

    ECS_COMPONENT(world, ImguiContext);
    ECS_COMPONENT(world, ImguiDesc);

    // clang-format off
    ECS_SYSTEM(world, AttachImgui, EcsOnSet,
        [in] system.sdl2.Window,
        [in] system.sdl2.GlContext,
        [in] system.imgui.ImguiDesc,
        [out] :system.imgui.ImguiContext);

    ECS_SYSTEM(world, ImguiNewFrame, EcsPostLoad, system.sdl2.Window, system.imgui.ImguiContext);
    ECS_SYSTEM(world, ImguiRender, EcsPostStore, system.imgui.ImguiContext);
    // clang-format on

    ECS_EXPORT_COMPONENT(ImguiContext);
    ECS_EXPORT_COMPONENT(ImguiDesc);
}