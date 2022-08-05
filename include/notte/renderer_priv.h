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

struct Static_Mesh
{
  const Vertex *verts;
  const u16 *indices;
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
} Technique;

typedef struct
{
  Dict *dict;
} Technique_Manager;

typedef struct
{
  VkCommandPool commandPool;
  VkCommandBuffer commandBuffers[MAX_FRAMES_IN_FLIGHT];
  VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
  VkSemaphore renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
  VkFence inFlightFences[MAX_FRAMES_IN_FLIGHT];
  VkFramebuffer *swapFbs;
} Render_Graph;

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
    };
  };
} Draw_Call;

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

  Fs_Driver *fs;

  Shader_Manager shaders;
  Technique_Manager techs;
  Render_Graph graph;

  Static_Mesh *mesh;

  VkCommandPool utilPool;

  Vector drawCalls;
};

#endif /* NOTTE_RENDERER_PRIV_H */
