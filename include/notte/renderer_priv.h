/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Vulkan renderer internal header.
 */

#ifndef NOTTE_RENDERER_PRIV_H
#define NOTTE_RENDERER_PRIV_H

#include <vulkan/vulkan.h>
#include <shaderc/shaderc.h>

#include <notte/renderer.h>
#include <notte/dict.h>
#include <notte/vector.h>

/* === MACROS === */

#define MAX_FRAMES_IN_FLIGHT 2

/* === TYPES === */

typedef enum
{
  RENDER_PASS_GBUFFER,
  RENDER_PASS_COUNT,
} Render_Pass;

struct Static_Mesh
{
  const Static_Vert *verts;
  const u32 *indices;
  usize nVerts, nIndices;
  VkBuffer vertexBuffer, indexBuffer;
  VkDeviceMemory vertexMemory, indexMemory;
};

typedef struct
{
  u32 graphicsFamily, presentFamily;
} Queue_Family_Info;

typedef struct
{
  VkSurfaceFormatKHR format;
  VkPresentModeKHR presentMode;
  VkExtent2D extent;
  u32 nImages;
  VkSwapchainKHR swapchain;
  VkImage *images;
  VkImageView *imageViews;
} Swapchain;

typedef enum
{
  SHADER_VERT,
  SHADER_FRAG,
} Shader_Type;

typedef struct Shader
{
  VkShaderModule mod;
  String name;
  Shader_Type type;
} Shader;

typedef struct
{
  String path;
  Dict *dict;
  shaderc_compiler_t compiler;
  Fs_Driver *fs;
  Fs_Dir_Monitor *monitor;
} Shader_Manager;

typedef struct 
{
  const u8 *path;
  Shader *vert, *frag;
  VkPipeline pipeline;
  VkPipelineLayout layout;
  VkRenderPass fakePass;
  VkDescriptorSetLayout descriptorLayout;
  VkDescriptorSet descriptorSets[MAX_FRAMES_IN_FLIGHT];
} Technique;

typedef struct
{
  Dict *dict;
} Technique_Manager;

typedef struct
{
  Technique *techs[RENDER_PASS_COUNT];
} Effect;

typedef struct
{
  Dict *dict;
} Effect_Manager;

struct Material
{
  Effect *effect;
  VkDescriptorSet descriptors[RENDER_PASS_COUNT];
};

typedef struct
{
  Dict *dict;
} Material_Manager;

typedef struct
{
  bool isSwapchain;
  VkFramebuffer fbs[MAX_FRAMES_IN_FLIGHT];
} Render_Graph_Texture;

typedef void (*Render_Graph_Record_Fn)(Renderer *ren, VkCommandBuffer buffer);

typedef struct
{
  Vector writes, reads;
  Render_Graph_Record_Fn fn;
  int mark; /* For topological sort. */
} Render_Graph_Pass;

typedef struct
{
  Renderer *ren;
  VkCommandPool commandPool;
  VkCommandBuffer commandBuffers[MAX_FRAMES_IN_FLIGHT];
  VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
  VkSemaphore renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
  VkFence inFlightFences[MAX_FRAMES_IN_FLIGHT];
  Render_Graph_Texture swap;
  VkFramebuffer *swapFbs;
  Vector passes, bakedPasses;
} Render_Graph;

struct Camera 
{
  Transform trans;
  float fov;
  Mat4 view, proj;
};

typedef enum
{
  DRAW_CALL_STATIC_MESH,
} Draw_Call_Type;

typedef struct
{
  Draw_Call_Type t;
  union
  {
    struct
    {
      Static_Mesh *staticMesh;
      Transform transform;
      Material *material;
    };
  };
} Draw_Call;

typedef struct
{
  Mat4 view, proj;
} Camera_Uniform;

typedef struct
{
  Mat4 model;
} Mesh_Push_Constant;

struct Renderer
{
  uint32_t currentFrame;
  Plat_Window *win;
  VkAllocationCallbacks *allocCbs;
  VkInstance vk;
  VkPhysicalDevice pDev;
  VkDevice dev;
  VkQueue graphicsQueue, presentQueue;
  VkSurfaceKHR surface;
  Queue_Family_Info queueInfo;
  Swapchain swapchain;
  Allocator alloc;
  VkDescriptorPool descriptorPool;

  Fs_Driver *fs;

  Shader_Manager shaders;
  Technique_Manager techs;
  Effect_Manager effects;
  Material_Manager materials;
  Render_Graph graph;

  Static_Mesh *mesh;

  VkCommandPool utilPool;

  VkBuffer uniformBuffers[MAX_FRAMES_IN_FLIGHT];
  VkDeviceMemory uniformMemory[MAX_FRAMES_IN_FLIGHT];

  VkImage texture;
  VkDeviceMemory textureMemory;
  VkImageView textureView;
  VkSampler textureSampler;

  VkImage depthImage;
  VkDeviceMemory depthMemory;
  VkImageView depthView;

  Vector drawCalls;

  Camera *cam;
};

#endif /* NOTTE_RENDERER_PRIV_H */
