#version 450

layout (binding = 0) uniform CameraUniform {
  mat4 view;
  mat4 proj;
} cam;

layout (location = 0) in vec2 inPos;
layout (location = 1) in vec3 inColor;

layout (location = 0) out vec3 fragColor;

layout (push_constant) uniform constants
{
  mat4 model;
} mesh;


void main()
{
  gl_Position = cam.proj * cam.view * mesh.model * vec4(inPos, 0.0, 1.0);
  fragColor = inColor;
}
