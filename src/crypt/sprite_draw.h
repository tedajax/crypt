#pragma once

#include "sokol_gfx.h"
#include "tx_math.h"
#include "tx_types.h"

typedef enum sprite_flip {
    SPRITE_FLIP_NONE = 0,
    SPRITE_FLIP_X = 1,
    SPRITE_FLIP_Y = 2,
} sprite_flip;

struct sprite {
    vec3 pos;
    vec4 rect;
    vec2 origin;
    vec2 scale;
};

void spr_init(void);
void spr_term(void);
void spr_render(int width, int height);
void spr_push_sprite(const struct sprite* sprite);
vec4 spr_calc_rect(uint32_t sprite_id, sprite_flip flip, uint16_t sw, uint16_t sh);