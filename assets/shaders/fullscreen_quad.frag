#version 330

uniform sampler2D screen_texture;
in vec2 uv;

out vec4 frag_color;

void main()
{
    int scanline_interval = 16;
    int scanline_size = 12;
    
    frag_color = texture(screen_texture, uv);
    // if (int(gl_FragCoord.y) % scanline_interval < scanline_size) {
    //     frag_color = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    //     // frag_color = mix(vec4(0.0f, 0.0f, 0.0f, 1.0f), frag_color, 0.5f);
    // }
}
