#version 330

uniform sampler2D screen_texture;
in vec2 uv;

out vec4 frag_color;

void main()
{
    
    frag_color = texture(screen_texture, uv);
    if (int(gl_FragCoord.y) % 4 == 0) {
        frag_color = mix(vec4(0.0f, 0.0f, 0.0f, 1.0f), frag_color, 0.1f);
    }
}
