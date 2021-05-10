#version 330

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;

uniform mat4 view_proj;
out vec4 vert_color;

void main()
{
    vert_color = color;
    gl_Position = view_proj * vec4(position, 1.0f);
}
