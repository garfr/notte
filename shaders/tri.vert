#version 450

layout (binding = 0) uniform CameraUniform {
  mat4 view;
  mat4 proj;
} cam;

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNor;
layout (location = 2) in vec2 inTex;

layout (location = 0) out vec4 fragColor;

layout (push_constant) uniform constants
{
  mat4 model;
} mesh;

void main()
{
  gl_Position = cam.proj * cam.view * mesh.model * vec4(inPos, 1.0);
  fragColor = vec4(inNor, 1.0);
}
