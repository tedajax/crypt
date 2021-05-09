#version 330

layout(location = 0) in vec2 position;
layout(location = 1) in vec4 color;

uniform mat4 view_proj;
out vec4 vert_color;

void main()
{
    vert_color = color;
    gl_Position = view_proj * vec4(position, 0.0f, 1.0f);
}
