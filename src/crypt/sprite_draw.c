#include "sprite_draw.h"
#include "futils.h"
#include "stb_ds.h"
#include "stb_image.h"
#include "string.h"
#include "system_window_sdl2.h"
#include "tdjx_game_comp.h"
#include <GL/gl3w.h>
#include <SDL2/SDL.h>

// private system structs
struct sprite {
    vec3 pos;
    vec4 rect;
    vec2 origin;
    vec2 scale;
};

struct vertex {
    vec2 pos;
    vec2 uv;
};

typedef struct uniform_block {
    mat4 view_proj;
} uniform_block;

typedef struct renderer_resources {
    sg_buffer geom_vbuf;
    sg_buffer geom_ibuf;
    sg_buffer inst_vbuf;
    sg_image atlas;

    struct {
        sg_shader shader;
        sg_pipeline pip;
        sg_bindings bindings;
        sg_pass pass;
        sg_pass_action pass_action;
        sg_image color_img;
        sg_image depth_img;
    } canvas;

    struct {
        sg_shader shader;
        sg_pipeline pip;
        sg_bindings bindings;
        sg_pass_action pass_action;
    } screen;
} renderer_resources;

vec4 spr_calc_rect(uint32_t sprite_id, sprite_flags flip, uint16_t sw, uint16_t sh)
{
    const int tc = 8;
    const float tcf = (float)tc;

    uint32_t row = sprite_id / tc;
    uint32_t col = sprite_id % tc;

    const float flip_offsets[3] = {0.0f, 1.0f, 1.0f};
    const float flip_scales[3] = {1.0f, -1.0f, -1.0f};

    int flip_id_x = flip & SpriteFlags_FlipX;
    int flip_id_y = flip & SpriteFlags_FlipY;

    float fx = flip_offsets[flip_id_x] * sw;
    float fy = flip_offsets[flip_id_y] * sh;

    float fw = flip_scales[flip_id_x] * sw;
    float fh = flip_scales[flip_id_y] * sh;

    return (vec4){(col + fx) / tcf, (row + fy) / tcf, fw / tc, fh / tc};
}

typedef struct Renderer {
    SDL_Window* sdl_window;
    SDL_GLContext gl_context;
    renderer_resources resources;
    float pixels_per_meter;
    uint32_t canvas_width;
    uint32_t canvas_height;
    struct sprite* sprites;
    ecs_query_t* q_sprites;
} Renderer;

// TODO: max_sprites refactor
renderer_resources init_renderer_resources(
    SDL_Window* window,
    int max_sprites,
    uint32_t canvas_width,
    uint32_t canvas_height)
{
    renderer_resources resources;
    memset(&resources, 0, sizeof(renderer_resources));

    // Configure render target render
    int iw, ih, ichan;
    stbi_uc* pixels = stbi_load("assets/atlas.png", &iw, &ih, &ichan, 4);
    TX_ASSERT(pixels);

    resources.atlas = sg_make_image(&(sg_image_desc){
        .width = iw,
        .height = ih,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .content.subimage[0][0] =
            {
                .ptr = pixels,
                .size = iw * ih * ichan,
            },
    });

    stbi_image_free(pixels);

    const float k_size = 1.0f;
    struct vertex quad_verts[] = {
        {.pos = {.x = 0, .y = 0}, .uv = {.x = 0.0f, .y = 0.0f}},
        {.pos = {.x = k_size, .y = 0}, .uv = {.x = 1.0f, .y = 0.0f}},
        {.pos = {.x = k_size, .y = k_size}, .uv = {.x = 1.0f, .y = 1.0f}},
        {.pos = {.x = 0, .y = k_size}, .uv = {.x = 0.0f, .y = 1.0f}},
    };

    const uint16_t quad_indices[] = {0, 2, 3, 0, 1, 2};

    resources.geom_vbuf = sg_make_buffer(&(sg_buffer_desc){
        .content = quad_verts,
        .size = sizeof(quad_verts),
    });

    resources.geom_ibuf = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .content = quad_indices,
        .size = sizeof(quad_indices),
    });

    resources.inst_vbuf = sg_make_buffer(&(sg_buffer_desc){
        .usage = SG_USAGE_STREAM,
        .size = sizeof(struct sprite) * max_sprites,
    });

    {
        char* vs_buffer;
        char* fs_buffer;
        size_t vs_len, fs_len;

        enum tx_result vs_result =
            read_file_to_buffer("assets/shaders/sprite.vert", &vs_buffer, &vs_len);
        enum tx_result fs_result =
            read_file_to_buffer("assets/shaders/sprite.frag", &fs_buffer, &fs_len);

        TX_ASSERT(vs_result == TX_SUCCESS && fs_result == TX_SUCCESS);

        resources.canvas.shader = sg_make_shader(&(sg_shader_desc){
            .vs.uniform_blocks[0] =
                {
                    .size = sizeof(uniform_block),
                    .uniforms =
                        {
                            [0] = {.name = "view_proj", .type = SG_UNIFORMTYPE_MAT4},
                        },
                },
            .fs.images[0] = {.name = "atlas", .type = SG_IMAGETYPE_2D},
            .vs.source = vs_buffer,
            .fs.source = fs_buffer,
        });

        free(vs_buffer);
        free(fs_buffer);
    }

    sg_image_desc image_desc = (sg_image_desc){
        .render_target = true,
        .width = canvas_width,
        .height = canvas_height,
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
    };

    resources.canvas.color_img = sg_make_image(&image_desc);
    image_desc.pixel_format = SG_PIXELFORMAT_DEPTH;
    resources.canvas.depth_img = sg_make_image(&image_desc);

    resources.canvas.pass = sg_make_pass(&(sg_pass_desc){
        .color_attachments[0].image = resources.canvas.color_img,
        .depth_stencil_attachment.image = resources.canvas.depth_img,
    });

    // default render states are fine for triangle
    resources.canvas.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = resources.canvas.shader,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout =
            {
                .buffers =
                    {
                        [0] = {.stride = sizeof(struct vertex)},
                        [1] =
                            {
                                .stride = sizeof(struct sprite),
                                .step_func = SG_VERTEXSTEP_PER_INSTANCE,
                            },
                    },
                .attrs =
                    {
                        [0] =
                            {
                                .format = SG_VERTEXFORMAT_FLOAT2,
                                .offset = 0,
                                .buffer_index = 0,
                            },
                        [1] =
                            {
                                .format = SG_VERTEXFORMAT_FLOAT2,
                                .offset = 8,
                                .buffer_index = 0,
                            },
                        [2] =
                            {
                                .format = SG_VERTEXFORMAT_FLOAT3,
                                .offset = 0,
                                .buffer_index = 1,
                            },
                        [3] =
                            {
                                .format = SG_VERTEXFORMAT_FLOAT4,
                                .offset = 12,
                                .buffer_index = 1,
                            },
                        [4] =
                            {
                                .format = SG_VERTEXFORMAT_FLOAT2,
                                .offset = 28,
                                .buffer_index = 1,
                            },
                        [5] =
                            {
                                .format = SG_VERTEXFORMAT_FLOAT2,
                                .offset = 36,
                                .buffer_index = 1,
                            },
                    },
            },
        .depth_stencil =
            {
                .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
                .depth_write_enabled = true,
            },
        .blend =
            {
                .enabled = true,
                .depth_format = SG_PIXELFORMAT_DEPTH,
                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                .src_factor_alpha = SG_BLENDFACTOR_ONE,
                .dst_factor_alpha = SG_BLENDFACTOR_ZERO,
            },
        .rasterizer.cull_mode = SG_CULLMODE_BACK,
    });

    resources.canvas.bindings = (sg_bindings){
        .vertex_buffers =
            {
                [0] = resources.geom_vbuf,
                [1] = resources.inst_vbuf,
            },
        .index_buffer = resources.geom_ibuf,
        .fs_images[0] = resources.atlas,
    };

    // Configure screen full-screen quad render
    {
        char* vs_buffer;
        char* fs_buffer;
        size_t vs_len, fs_len;

        enum tx_result vs_result =
            read_file_to_buffer("assets/shaders/fullscreen_quad.vert", &vs_buffer, &vs_len);
        enum tx_result fs_result =
            read_file_to_buffer("assets/shaders/fullscreen_quad.frag", &fs_buffer, &fs_len);

        TX_ASSERT(vs_result == TX_SUCCESS && fs_result == TX_SUCCESS);

        resources.screen.shader = sg_make_shader(&(sg_shader_desc){
            .fs.images[0] = {.name = "screen_texture", .type = SG_IMAGETYPE_2D},
            .vs.source = vs_buffer,
            .fs.source = fs_buffer,
        });

        free(vs_buffer);
        free(fs_buffer);
    }

    // Our fullscreen quad shader doesn't require any attributes but sokol has no mechanism for
    // for binding empty attribute arrays so define a per-instance float so *something* goes across
    // and sokol is happy.
    resources.screen.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = resources.screen.shader,
        .layout =
            {
                .buffers[0] =
                    {
                        .step_func = SG_VERTEXSTEP_PER_INSTANCE,
                    },
                .attrs[0] =
                    {
                        .format = SG_VERTEXFORMAT_FLOAT,
                    },
            },
    });

    float data = 0.0f;
    resources.screen.bindings = (sg_bindings){
        .vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
            .usage = SG_USAGE_IMMUTABLE,
            .size = sizeof(float),
            .content = &data,
        }),
        .fs_images[0] = resources.canvas.color_img,
    };

    resources.screen.pass_action = (sg_pass_action){
        .colors[0] = {.action = SG_ACTION_CLEAR, .val = {0.1f, 0.0f, 0.1f}},
    };

    return resources;
}

void AttachRenderer(ecs_iter_t* it)
{
    ecs_world_t* world = it->world;
    SpriteRenderConfig* config = ecs_column(it, SpriteRenderConfig, 1);
    ecs_entity_t ecs_typeid(Renderer) = ecs_column_entity(it, 2);

    ecs_entity_t ecs_typeid(Sdl2Window) = ecs_lookup_fullpath(world, "system.sdl2.window.Window");

    for (int i = 0; i < it->count; ++i) {
        const Sdl2Window* window = ecs_get(world, config->e_window, Sdl2Window);

        SDL_Window* sdl_window = window->window;
        SDL_GLContext ctx = SDL_GL_CreateContext(sdl_window);

        if (gl3wInit() != GL3W_OK) {
            ecs_err("Gl3w failed to init.");
            break;
        }

        sg_setup(&(sg_desc){0});
        TX_ASSERT(sg_isvalid());
        ecs_trace_1("sokol initialized");

        struct sprite* sprites = NULL;
        arrsetcap(sprites, 256);

        renderer_resources resources = init_renderer_resources(
            sdl_window, (int)arrcap(sprites), config[i].canvas_width, config[i].canvas_height);
        ecs_query_t* q_sprites = ecs_query_new(
            world,
            "tdjx.game.comp.Position, ANY:tdjx.sprite.renderer.Sprite, "
            "?tdjx.sprite.renderer.SpriteFlags, ?tdjx.sprite.renderer.SpriteSize");

        ecs_set(
            world,
            it->entities[i],
            Renderer,
            {
                .resources = resources,
                .sdl_window = sdl_window,
                .sprites = sprites,
                .gl_context = ctx,
                .pixels_per_meter = config[i].pixels_per_meter,
                .canvas_width = config[i].canvas_width,
                .canvas_height = config[i].canvas_height,
                .q_sprites = q_sprites,
            });
    }
}

void DetachRenderer(ecs_iter_t* it)
{
    Renderer* r = ecs_column(it, Renderer, 1);

    for (int i = 0; i < it->count; ++i) {
        arrfree(r[i].sprites);
        ecs_query_free(r[i].q_sprites);
        sg_shutdown();
        SDL_GL_DeleteContext(r[i].gl_context);
    }
}

void Render(ecs_iter_t* it)
{
    ecs_world_t* world = it->world;
    Renderer* r = ecs_column(it, Renderer, 1);

    for (int i = 0; i < it->count; ++i) {
        int width, height;
        SDL_GL_GetDrawableSize(r[i].sdl_window, &width, &height);

        const float aspect = (float)r[i].canvas_width / r[i].canvas_height;
        float view_width = r[i].canvas_width / r[i].pixels_per_meter;
        float view_height = r[i].canvas_height / r[i].pixels_per_meter;

        mat4 view = mat4_look_at((vec3){0, 0, 100}, (vec3){0, 0, -1}, (vec3){0, 1, 0});
        mat4 projection = mat4_ortho(0, view_width, view_height, 0, 0.0f, 250.0f);
        mat4 view_proj = mat4_mul(projection, view);

        sg_begin_pass(
            r->resources.canvas.pass,
            &(sg_pass_action){
                .colors[0] =
                    {
                        .action = SG_ACTION_CLEAR,
                        .val = {0.0f, 0.0f, 0.0f, 1.0f},
                    },
            });

        sg_apply_pipeline(r->resources.canvas.pip);
        sg_apply_bindings(&r->resources.canvas.bindings);
        uniform_block uniforms = {.view_proj = view_proj};
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &uniforms, sizeof(uniform_block));
        sg_draw(0, 6, (int)arrlen(r->sprites));
        sg_end_pass();

        sg_begin_default_pass(&r[i].resources.screen.pass_action, width, height);
        sg_apply_pipeline(r[i].resources.screen.pip);
        sg_apply_bindings(&r[i].resources.screen.bindings);
        sg_draw(0, 6, 1);
        sg_end_pass();

        sg_commit();
        SDL_GL_SwapWindow(r->sdl_window);
    }
}

void UpdateBuffers(ecs_iter_t* it)
{
    Renderer* r = ecs_column(it, Renderer, 1);

    for (int i = 0; i < it->count; ++i) {
        size_t prev_cap = arrcap(r[i].sprites);
        arrsetlen(r[i].sprites, 0);

        ecs_query_t* query = r[i].q_sprites;
        ecs_iter_t qit = ecs_query_iter(query);
        while (ecs_query_next(&qit)) {
            Position* pos = ecs_column(&qit, Position, 1);
            TdjxSprite* spr = ecs_column(&qit, TdjxSprite, 2);
            TdjxSpriteFlags* flag = ecs_column(&qit, TdjxSpriteFlags, 3);
            TdjxSpriteSize* ssize = ecs_column(&qit, TdjxSpriteSize, 4);

            for (int32_t j = 0; j < qit.count; ++j) {
                vec3 position = (vec3){.x = pos[j].x, .y = pos[j].y};
                uint32_t sprite_id;
                if (ecs_is_owned(&qit, 2)) {
                    sprite_id = spr[j].sprite_id;
                } else {
                    sprite_id = spr->sprite_id;
                }
                uint16_t flags = SpriteFlags_None;
                if (flag) {
                    flags = flag[j].flags;
                }
                uint16_t swidth = 1, sheight = 1;
                if (ssize) {
                    swidth = ssize[j].width;
                    sheight = ssize[j].height;
                }

                arrpush(
                    r[i].sprites,
                    ((struct sprite){
                        .pos = position,
                        .rect = spr_calc_rect(sprite_id, flags, swidth, sheight),
                        .scale = {.x = (float)swidth, .y = (float)sheight},
                        .origin = {.x = 0.0f, .y = 0.0f},
                    }));
            }
        }

        // resize gpu instance buffer if not large enough
        size_t cap = arrcap(r[i].sprites);
        if (prev_cap < cap) {
            sg_destroy_buffer(r[i].resources.inst_vbuf);

            r[i].resources.inst_vbuf = sg_make_buffer(&(sg_buffer_desc){
                .usage = SG_USAGE_STREAM,
                .size = (int)(sizeof(struct sprite) * cap),
            });

            r[i].resources.canvas.bindings.vertex_buffers[1] = r[i].resources.inst_vbuf;
        }

        sg_update_buffer(
            r[i].resources.inst_vbuf,
            r[i].sprites,
            (int)(sizeof(struct sprite) * arrlenu(r[i].sprites)));
    }
}

void TdjxSpriteRendererImport(ecs_world_t* world)
{
    ECS_MODULE(world, TdjxSpriteRenderer);

    ecs_set_name_prefix(world, "Tdjx");

    ECS_IMPORT(world, TdjxGameComp);

    ECS_COMPONENT(world, TdjxSprite);
    ECS_COMPONENT(world, TdjxSpriteFlags);
    ECS_COMPONENT(world, TdjxSpriteSize);
    ECS_COMPONENT(world, SpriteRenderConfig);

    ECS_COMPONENT(world, Renderer);

    // clang-format off
    ECS_SYSTEM(world, AttachRenderer, EcsOnSet,
        [in] SpriteRenderConfig,
        [out] :Renderer);
    ECS_SYSTEM(world, DetachRenderer, EcsUnSet, Renderer);

    ECS_SYSTEM(world, UpdateBuffers, EcsOnStore, Renderer);
    ECS_SYSTEM(world, Render, EcsPostFrame, Renderer);
    // clang-format on

    ECS_EXPORT_COMPONENT(TdjxSprite);
    ECS_EXPORT_COMPONENT(TdjxSpriteFlags);
    ECS_EXPORT_COMPONENT(TdjxSpriteSize);
    ECS_EXPORT_COMPONENT(SpriteRenderConfig);
}