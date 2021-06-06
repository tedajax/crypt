#include "system_imgui.h"
#include "sprite_renderer.h"
#include "system_sdl2.h"
#include <SDL2/SDL.h>
#include <ccimgui.h>
#include <cimgui_impl.h>

static void AttachImgui(ecs_iter_t* it)
{
    Sdl2Window* window = ecs_term(it, Sdl2Window, 1);
    Sdl2GlContext* gl = ecs_term(it, Sdl2GlContext, 2);
    ecs_id_t ecs_typeid(ImguiContext) = ecs_term_id(it, 4);

    if (ecs_singleton_get(it->world, ImguiContext)) {
        return;
    }

    igCreateContext(NULL);
    ImGuiIO* imgui = igGetIO();
    imgui->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui_ImplSDL2_InitForOpenGL(window->window, gl->gl);
    ImGui_ImplOpenGL3_Init(NULL);
    igStyleColorsDark(NULL);

    ImFont* editor_font = ImFontAtlas_AddFontFromFileTTF(
        imgui->Fonts, "assets/fonts/FiraCode-Regular.ttf", 24.0f, NULL, NULL);

    ecs_singleton_set(it->world, ImguiContext, {.io = imgui, editor_font = editor_font});
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

void on_sdl2_event(const SDL_Event* event)
{
    ImGui_ImplSDL2_ProcessEvent(event);
}

void SystemImguiImport(ecs_world_t* world)
{
    ECS_MODULE(world, SystemImgui);

    ECS_IMPORT(world, SystemSdl2);

    ecs_set(world, Sdl2, Sdl2Input, {.event_fn = on_sdl2_event});

    ecs_set_name_prefix(world, "Imgui");

    ECS_COMPONENT(world, ImguiContext);
    ECS_COMPONENT(world, ImguiDesc);

    // clang-format off
    ECS_SYSTEM(world, AttachImgui, EcsOnSet,
        [in] system.sdl2.Window,
        [in] system.sdl2.GlContext,
        [in] system.imgui.ImguiDesc,
        [out] :system.imgui.ImguiContext);

    ECS_SYSTEM(world, ImguiNewFrame, EcsPostLoad, system.sdl2.Window, $system.imgui.Context);
    ECS_SYSTEM(world, ImguiRender, EcsPostStore, $system.imgui.Context);
    // clang-format on

    ECS_EXPORT_COMPONENT(ImguiContext);
    ECS_EXPORT_COMPONENT(ImguiDesc);
}