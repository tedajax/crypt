#pragma once

#include "tx_math.h"

inline vec4 color_rgba(float r, float g, float b, float a)
{
    (vec4){r, g, b, a};
}

inline vec4 color_rgb(float r, float g, float b)
{
    return (vec4){r, g, b};
}

static const vec4 k_color_clear = {1, 1, 1, 0};
static const vec4 k_color_white = {1, 1, 1, 1};
static const vec4 k_color_grey_light = {0.75f, 0.75f, 0.75f, 1};
static const vec4 k_color_grey = {0.5f, 0.5f, 0.5f, 1};
static const vec4 k_color_grey_dark = {0.25f, 0.25f, 0.25f, 1};
static const vec4 k_color_black = {0, 0, 0, 1};
static const vec4 k_color_red = {1, 0, 0, 1};
static const vec4 k_color_green = {0, 1, 0, 1};
static const vec4 k_color_blue = {0, 0, 1, 1};
static const vec4 k_color_yellow = {1, 1, 0, 1};
static const vec4 k_color_magenta = {1, 0, 1, 1};
static const vec4 k_color_cyan = {0, 1, 1, 1};
static const vec4 k_color_orange = {1, 0.5f, 0, 1};
static const vec4 k_color_rose = {1, 0, 0.5f, 1};
static const vec4 k_color_violet = {0.5f, 0, 1, 1};
static const vec4 k_color_azure = {0, 0.5f, 1, 1};
static const vec4 k_color_spring = {0, 1, 0.5f, 1};
static const vec4 k_color_chartreuse = {0.5f, 1, 0, 1};