#version 330

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord0;
layout(location = 2) in vec3 inst_pos;
layout(location = 3) in vec4 spr_rect;   // pos: x,y -- size: z,w
layout(location = 4) in vec2 spr_origin; // 0,0 : top left, 0.5,0.5: center, 1,1: bottom right
layout(location = 5) in vec2 inst_scale;

uniform mat4 view_proj;
out vec2 uv;

void main()
{
    uv = texcoord0 * spr_rect.zw + spr_rect.xy;
    vec2 origin = spr_origin * inst_scale;
    vec2 vert_pos = position * inst_scale;

    vec3 pos = vec3(vert_pos + inst_pos.xy - origin, inst_pos.z);
    gl_Position = view_proj * vec4(pos, 1.0f);
}
