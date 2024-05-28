#version 450

layout (location = 0) in vec3 Position;
layout (location = 1) in vec4 Color;

layout (location = 0) out vec4 outColor;

layout (set = 1, binding = 0) uniform UBO
{
    mat4x4 matrix;
};

void main()
{
    outColor = vec4(Color);
    gl_Position = matrix * vec4(Position, 1.0);
}
