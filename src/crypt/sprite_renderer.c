#include "sprite_renderer.h"
#include "futils.h"
#include "game_components.h"
#include "stb_ds.h"
#include "stb_image.h"
#include "string.h"
#include "system_sdl2.h"
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

struct vertex_color {
    vec3 pos;
    vec4 col;
};

struct prim_line {
    struct vertex_color v[2];
};

struct prim_rect {
    struct vertex_color v[4];
};

typedef struct uniform_block {
    mat4 view_proj;
} uniform_block;

typedef struct renderer_resources {
    // sprite atlas
    sg_image atlas;

    // sprite buffers
    sg_buffer geom_vbuf;
    sg_buffer geom_ibuf;
    sg_buffer inst_vbuf;

    // primitive line buffers
    sg_buffer line_vbuf;

    // primitive rect buffers
    sg_buffer rect_vbuf;
    sg_buffer rect_ibuf;

    struct {
        sg_shader sprite_shader;
        sg_shader prim_shader;
        sg_pipeline pip;
        sg_bindings bindings;
        sg_pipeline line_pip;
        sg_bindings line_bindings;
        sg_pipeline rect_pip;
        sg_bindings rect_bindings;
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

typedef struct Renderer {
    SDL_Window* sdl_window;
    SDL_GLContext gl_context;
    renderer_resources resources;
    float pixels_per_meter;
    uint32_t canvas_width;
    uint32_t canvas_height;
    struct sprite* sprites;
    struct prim_line* lines;
    struct prim_rect* rects;
    uint32_t* rect_indices;
    struct {
        float prim_layer;
    } prim_draw_state;
    ecs_query_t* q_sprites;
} Renderer;

// private state
ecs_query_t* q_renderer = NULL;

// private interface
vec4 spr_calc_rect(uint32_t sprite_id, sprite_flags flip, uint16_t sw, uint16_t sh);
renderer_resources init_renderer_resources(
    SDL_Window* window,
    int initial_cap,
    uint32_t canvas_width,
    uint32_t canvas_height);

vec4 spr_calc_rect(uint32_t sprite_id, sprite_flags flip, uint16_t sw, uint16_t sh)
{
    const int tc = 16;
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

renderer_resources init_renderer_resources(
    SDL_Window* window,
    int initial_cap,
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
        .size = sizeof(struct sprite) * initial_cap,
    });

    resources.line_vbuf = sg_make_buffer(&(sg_buffer_desc){
        .usage = SG_USAGE_STREAM,
        .size = sizeof(struct prim_line) * 256,
    });

    resources.rect_vbuf = sg_make_buffer(&(sg_buffer_desc){
        .usage = SG_USAGE_STREAM,
        .size = sizeof(struct prim_rect) * 256,
    });

    resources.rect_ibuf = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .usage = SG_USAGE_STREAM,
        .size = sizeof(uint32_t) * 256 * 6,
    });

    // load sprite shader
    {
        char* vs_buffer;
        char* fs_buffer;
        size_t vs_len, fs_len;

        enum tx_result vs_result =
            read_file_to_buffer("assets/shaders/sprite.vert", &vs_buffer, &vs_len);
        enum tx_result fs_result =
            read_file_to_buffer("assets/shaders/sprite.frag", &fs_buffer, &fs_len);

        TX_ASSERT(vs_result == TX_SUCCESS && fs_result == TX_SUCCESS);

        resources.canvas.sprite_shader = sg_make_shader(&(sg_shader_desc){
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

    // load primitive shader
    {
        char* vs_buffer;
        char* fs_buffer;
        size_t vs_len, fs_len;

        enum tx_result vs_result =
            read_file_to_buffer("assets/shaders/primitive.vert", &vs_buffer, &vs_len);
        enum tx_result fs_result =
            read_file_to_buffer("assets/shaders/primitive.frag", &fs_buffer, &fs_len);

        TX_ASSERT(vs_result == TX_SUCCESS && fs_result == TX_SUCCESS);

        resources.canvas.prim_shader = sg_make_shader(&(sg_shader_desc){
            .vs.uniform_blocks[0] =
                {
                    .size = sizeof(uniform_block),
                    .uniforms =
                        {
                            [0] = {.name = "view_proj", .type = SG_UNIFORMTYPE_MAT4},
                        },
                },
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
        .pixel_format = SG_PIXELFORMAT_RGBA8,
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
        .shader = resources.canvas.sprite_shader,
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
                .color_format = SG_PIXELFORMAT_RGBA8,
            },
        .rasterizer.cull_mode = SG_CULLMODE_NONE,
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

    // primitive pipelines share many similarities so create a base structure here
    sg_pipeline_desc base_prim_pip = (sg_pipeline_desc){
        .shader = resources.canvas.prim_shader,
        .layout =
            {
                .buffers[0] = {.stride = sizeof(struct vertex_color)},
                .attrs =
                    {
                        [0] =
                            {
                                .format = SG_VERTEXFORMAT_FLOAT3,
                                .offset = 0,
                                .buffer_index = 0,
                            },
                        [1] =
                            {
                                .format = SG_VERTEXFORMAT_FLOAT4,
                                .offset = 12,
                                .buffer_index = 0,
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
    };

    // Setup primtive line drawing pipeline and bindings
    sg_pipeline_desc prim_line_pip = base_prim_pip;
    prim_line_pip.primitive_type = SG_PRIMITIVETYPE_LINES;
    prim_line_pip.index_type = SG_INDEXTYPE_NONE;

    resources.canvas.line_pip = sg_make_pipeline(&prim_line_pip);
    resources.canvas.line_bindings = (sg_bindings){
        .vertex_buffers[0] = resources.line_vbuf,
    };

    // Setup primitive rect drawing pipeline and bindings
    sg_pipeline_desc prim_rect_pip = base_prim_pip;
    prim_rect_pip.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    prim_rect_pip.index_type = SG_INDEXTYPE_UINT32;

    resources.canvas.rect_pip = sg_make_pipeline(&prim_rect_pip);
    resources.canvas.rect_bindings = (sg_bindings){
        .vertex_buffers[0] = resources.rect_vbuf,
        .index_buffer = resources.rect_ibuf,
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

    static const float data = 0.0f;
    resources.screen.bindings = (sg_bindings){
        .vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
            .usage = SG_USAGE_IMMUTABLE,
            .size = sizeof(float),
            .content = &data,
        }),
        .fs_images[0] = resources.canvas.color_img,
    };

    resources.screen.pass_action = (sg_pass_action){
        .colors[0] = {.action = SG_ACTION_CLEAR, .val = {0.0f, 0.0f, 0.0f}},
    };

    return resources;
}

void draw_line_col2(vec2 from, vec2 to, vec4 col0, vec4 col1)
{
    if (!q_renderer) {
        return;
    }

    ecs_iter_t it = ecs_query_iter(q_renderer);
    while (ecs_query_next(&it)) {
        Renderer* r = ecs_column(&it, Renderer, 1);

        float layer = r->prim_draw_state.prim_layer;
        struct prim_line line = {
            .v[0] = {.pos = (vec3){.x = from.x, .y = from.y, .z = layer}, .col = col0},
            .v[1] = {.pos = (vec3){.x = to.x, .y = to.y, .z = layer}, .col = col1},
        };

        size_t prev_cap = arrcap(r->lines);

        arrput(r->lines, line);

        size_t cap = arrcap(r->lines);
        if (cap > prev_cap) {
            sg_destroy_buffer(r->resources.line_vbuf);
            r->resources.line_vbuf = sg_make_buffer(&(sg_buffer_desc){
                .usage = SG_USAGE_STREAM,
                .size = (int)(sizeof(struct prim_line) * cap),
            });
            r->resources.canvas.line_bindings.vertex_buffers[0] = r->resources.line_vbuf;
        }
    }
}

void draw_line_col(vec2 from, vec2 to, vec4 col)
{
    draw_line_col2(from, to, col, col);
}

void draw_line(vec2 from, vec2 to)
{
    static vec4 white = {1.0f, 1.0f, 1.0f, 1.0f};
    draw_line_col2(from, to, white, white);
}

void draw_rect_col4(vec2 p0, vec2 p1, vec4 cols[4])
{
    if (!q_renderer) {
        return;
    }
    const static uint32_t index_offsets[6] = {0, 2, 3, 0, 1, 2};

    ecs_iter_t it = ecs_query_iter(q_renderer);
    while (ecs_query_next(&it)) {
        Renderer* r = ecs_column(&it, Renderer, 1);

        float layer = r->prim_draw_state.prim_layer;
        struct prim_rect rect = {
            .v[0] = {.pos = (vec3){.x = p0.x, .y = p0.y, .z = layer}, .col = cols[0]},
            .v[1] = {.pos = (vec3){.x = p1.x, .y = p0.y, .z = layer}, .col = cols[1]},
            .v[2] = {.pos = (vec3){.x = p1.x, .y = p1.y, .z = layer}, .col = cols[2]},
            .v[3] = {.pos = (vec3){.x = p0.x, .y = p1.y, .z = layer}, .col = cols[3]},
        };

        size_t prev_cap = arrcap(r->rects);

        uint32_t base_idx = (uint32_t)(arrlen(r->rect_indices) / 6) * 4;

        arrput(r->rects, rect);

        for (int32_t j = 0; j < 6; ++j) {
            arrput(r->rect_indices, base_idx + index_offsets[j]);
        }

        size_t cap = arrcap(r->rects);
        if (cap > prev_cap) {
            sg_destroy_buffer(r->resources.rect_vbuf);
            sg_destroy_buffer(r->resources.rect_ibuf);

            r->resources.rect_vbuf = sg_make_buffer(&(sg_buffer_desc){
                .usage = SG_USAGE_STREAM,
                .size = (int)(sizeof(struct prim_rect) * cap),
            });
            r->resources.rect_ibuf = sg_make_buffer(&(sg_buffer_desc){
                .usage = SG_USAGE_STREAM,
                .size = (int)(sizeof(uint32_t) * 6 * cap),
            });

            r->resources.canvas.rect_bindings.vertex_buffers[0] = r->resources.rect_vbuf;
            r->resources.canvas.rect_bindings.index_buffer = r->resources.rect_ibuf;
        }
    }
}

void draw_rect_col(vec2 p0, vec2 p1, vec4 col)
{
    vec4 cols[4] = {col, col, col, col};
    draw_rect_col4(p0, p1, cols);
}

void draw_vgrad(vec2 p0, vec2 p1, vec4 top, vec4 bot)
{
    vec4 cols[4] = {top, top, bot, bot};
    draw_rect_col4(p0, p1, cols);
}

void draw_hgrad(vec2 p0, vec2 p1, vec4 left, vec4 right)
{
    vec4 cols[4] = {left, right, left, right};
    draw_rect_col4(p0, p1, cols);
}

void draw_set_prim_layer(float layer)
{
    if (!q_renderer) {
        return;
    }

    ecs_iter_t it = ecs_query_iter(q_renderer);
    while (ecs_query_next(&it)) {
        Renderer* r = ecs_column(&it, Renderer, 1);
        r->prim_draw_state.prim_layer = layer;
    }
}

void draw_reset_prim_layer()
{
    draw_set_prim_layer(0.0f);
}

void AttachRenderer(ecs_iter_t* it)
{
    ecs_world_t* world = it->world;
    SpriteRenderConfig* config = ecs_column(it, SpriteRenderConfig, 1);
    ecs_entity_t ecs_typeid(Renderer) = ecs_column_entity(it, 2);

    ecs_entity_t ecs_typeid(Sdl2Window) = ecs_lookup_fullpath(world, "system.sdl2.Window");

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

        struct prim_line* lines = NULL;
        arrsetcap(lines, 256);

        struct prim_rect* rects = NULL;
        uint32_t* rect_indices = NULL;
        arrsetcap(rects, 256);
        arrsetcap(rect_indices, 256 * 6);

        renderer_resources resources = init_renderer_resources(
            sdl_window, (int)arrcap(sprites), config[i].canvas_width, config[i].canvas_height);
        ecs_query_t* q_sprites = ecs_query_new(
            world,
            "game.comp.Position, ANY:sprite.renderer.Sprite, "
            "?sprite.renderer.SpriteFlags, ?sprite.renderer.SpriteSize");

        ecs_singleton_set(
            world,
            Renderer,
            {
                .resources = resources,
                .sdl_window = sdl_window,
                .sprites = sprites,
                .lines = lines,
                .rects = rects,
                .rect_indices = rect_indices,
                .gl_context = ctx,
                .pixels_per_meter = config[i].pixels_per_meter,
                .canvas_width = config[i].canvas_width,
                .canvas_height = config[i].canvas_height,
                .q_sprites = q_sprites,
                .prim_draw_state =
                    {
                        .prim_layer = 0,
                    },
            });
    }
}

void DetachRenderer(ecs_iter_t* it)
{
    Renderer* r = ecs_column(it, Renderer, 1);

    for (int i = 0; i < it->count; ++i) {
        arrfree(r[i].sprites);
        arrfree(r[i].lines);
        arrfree(r[i].rects);
        arrfree(r[i].rect_indices);

        ecs_query_free(r[i].q_sprites);

        sg_shutdown();
        SDL_GL_DeleteContext(r[i].gl_context);
    }
}

void RendererNewFrame(ecs_iter_t* it)
{
    Renderer* r = ecs_column(it, Renderer, 1);
    arrsetlen(r->lines, 0);
    arrsetlen(r->rects, 0);
    arrsetlen(r->rect_indices, 0);
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
            Sprite* spr = ecs_column(&qit, Sprite, 2);
            SpriteFlags* flag = ecs_column(&qit, SpriteFlags, 3);
            SpriteSize* ssize = ecs_column(&qit, SpriteSize, 4);

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

        sg_update_buffer(
            r[i].resources.line_vbuf,
            r[i].lines,
            (int)(sizeof(struct prim_line) * arrlenu(r[i].lines)));

        sg_update_buffer(
            r[i].resources.rect_vbuf,
            r[i].rects,
            (int)(sizeof(struct prim_rect) * arrlenu(r[i].rects)));

        sg_update_buffer(
            r[i].resources.rect_ibuf,
            r[i].rect_indices,
            (int)(sizeof(uint32_t) * arrlenu(r[i].rect_indices)));
    }
}

void Render(ecs_iter_t* it)
{
    ecs_world_t* world = it->world;
    Renderer* r = ecs_column(it, Renderer, 1);

    for (int32_t i = 0; i < it->count; ++i) {
        int width, height;
        SDL_GL_GetDrawableSize(r[i].sdl_window, &width, &height);

        const float aspect = (float)r[i].canvas_width / r[i].canvas_height;
        float view_width = r[i].canvas_width / r[i].pixels_per_meter;
        float view_height = r[i].canvas_height / r[i].pixels_per_meter;

        mat4 view = mat4_look_at((vec3){0, 0, 100}, (vec3){0, 0, -1}, (vec3){0, 1, 0});
        mat4 projection = mat4_ortho(0, view_width, view_height, 0, 0.0f, 250.0f);
        mat4 view_proj = mat4_mul(projection, view);

        // The first pass is the canvas pass which writes to the low resolution render target
        sg_begin_pass(
            r[i].resources.canvas.pass,
            &(sg_pass_action){
                .colors[0] =
                    {
                        .action = SG_ACTION_CLEAR,
                        // .val = {0.392f, 0.584f, 0.929f, 1.0f},
                        .val = {0.1f, 0.1f, 0.1f, 1.0f},
                    },
            });

        // same uniforms across all canvas renders
        uniform_block uniforms = {.view_proj = view_proj};

        // primtive rects
        sg_apply_pipeline(r[i].resources.canvas.rect_pip);
        sg_apply_bindings(&r[i].resources.canvas.rect_bindings);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &uniforms, sizeof(uniform_block));
        int rect_idx_ct = (int)arrlen(r[i].rect_indices);
        int rect_ct = (int)arrlen(r[i].rects);
        sg_draw(0, rect_idx_ct, rect_ct);

        // primitive lines
        sg_apply_pipeline(r[i].resources.canvas.line_pip);
        sg_apply_bindings(&r[i].resources.canvas.line_bindings);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &uniforms, sizeof(uniform_block));
        int line_elems = (int)arrlen(r[i].lines);
        sg_draw(0, line_elems * 2, line_elems);

        // sprites
        sg_apply_pipeline(r[i].resources.canvas.pip);
        sg_apply_bindings(&r[i].resources.canvas.bindings);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &uniforms, sizeof(uniform_block));
        sg_draw(0, 6, (int)arrlen(r[i].sprites));

        sg_end_pass();

        // Render the canvas to the full screen window on a fullscreen quad
        sg_begin_default_pass(&r[i].resources.screen.pass_action, width, height);
        sg_apply_pipeline(r[i].resources.screen.pip);
        sg_apply_bindings(&r[i].resources.screen.bindings);
        sg_draw(0, 6, 1);
        sg_end_pass();

        sg_commit();
        SDL_GL_SwapWindow(r[i].sdl_window);
    }
}

void renderer_fini(ecs_world_t* world, void* ctx)
{
    ecs_query_free(q_renderer);
}

void SpriteRendererImport(ecs_world_t* world)
{
    ECS_MODULE(world, SpriteRenderer);

    ecs_atfini(world, renderer_fini, NULL);

    ECS_IMPORT(world, GameComp);

    ECS_COMPONENT(world, Sprite);
    ECS_COMPONENT(world, SpriteFlags);
    ECS_COMPONENT(world, SpriteSize);
    ECS_COMPONENT(world, SpriteRenderConfig);

    ECS_COMPONENT(world, Renderer);

    // clang-format off
    ECS_SYSTEM(world, AttachRenderer, EcsOnSet,
        [in] SpriteRenderConfig,
        [out] :Renderer);
    ECS_SYSTEM(world, DetachRenderer, EcsUnSet, Renderer);

    ECS_SYSTEM(world, RendererNewFrame, EcsPreFrame, Renderer);
    ECS_SYSTEM(world, UpdateBuffers, EcsOnStore, Renderer);
    ECS_SYSTEM(world, Render, EcsPostFrame, Renderer);
    // clang-format on

    q_renderer = ecs_query_new(world, "$sprite.renderer.Renderer");

    ECS_EXPORT_COMPONENT(Sprite);
    ECS_EXPORT_COMPONENT(SpriteFlags);
    ECS_EXPORT_COMPONENT(SpriteSize);
    ECS_EXPORT_COMPONENT(SpriteRenderConfig);
}