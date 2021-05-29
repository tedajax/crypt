#version 330

uniform sampler2D atlas;

in vec2 uv;
in vec4 color;

out vec4 frag_color;

void main()
{
    vec4 tex_color = texture(atlas, uv);
    frag_color.rgb = mix(tex_color.rgb, color.rgb, color.a);
    frag_color.a = tex_color.a;
    // if (frag_color.rgb == vec3(0, 0, 0)) {
    //     frag_color.a = 0.0;
    // }
}
