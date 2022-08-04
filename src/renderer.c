/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Vulkan renderer.
 */

#include <vulkan/vulkan.h>
#include <shaderc/shaderc.h>

#include <notte/renderer.h>
#include <notte/membuf.h>
#include <notte/bson.h>
#include <notte/dict.h>
#include <notte/math.h>

#define MAX_FRAMES_IN_FLIGHT 2

/* === TYPES === */

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
} Shader;

typedef struct
{
  String path;
  Dict *dict;
  shaderc_compiler_t compiler;
  Fs_Driver *fs;
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

  VkBuffer vertexBuffer, indexBuffer;
  VkDeviceMemory vertexBufferMemory, indexBufferMemory;

  VkCommandPool utilPool;
};

typedef struct
{
  Vec2 pos;
  Vec3 color;
} Vertex;

/* === MACROS === */

#define CLAMP(_val, _min, _max) ((_val) > (_max) ? (_max) : ((_val) < (_min) ? \
      (_min) : (_val)))

/* === CONSTANTS === */

const char *requiredLayers[] = {
  "VK_LAYER_KHRONOS_validation"
};

const char *requiredExtensions[] = {
  "VK_KHR_surface",
};

const char *requiredDeviceExtensions[] = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

const Vertex vertices[] =
{
  {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
  {{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
  {{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
  {{-0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}},
};

const u16 indices[] = {
  0, 1, 2, 2, 3, 0,
};

VkVertexInputBindingDescription vertexBindingDescription =
{
  .binding = 0,
  .stride = sizeof(Vertex),
  .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
};

VkVertexInputAttributeDescription vertexAttributeDescription[] =
{
  {
    .binding = 0,
    .location = 0,
    .format = VK_FORMAT_R32G32_SFLOAT,
    .offset = OFFSETOF(Vertex, pos),
  },
  {
    .binding = 0,
    .location = 1,
    .format = VK_FORMAT_R32G32B32_SFLOAT,
    .offset = OFFSETOF(Vertex, color),
  },
};

/* === PROTOTYPES === */

static Err_Code CreateInstance(Renderer *ren);
static Err_Code SelectPhysicalDevice(Renderer *ren);
static bool DeviceIsSuitable(Renderer *ren, VkPhysicalDevice dev, 
    Queue_Family_Info *info);
static Err_Code CreateLogicalDevice(Renderer *ren);
static Err_Code CreateSwapchain(Renderer *ren, Swapchain *swapchain);
static void DestroySwapchain(Renderer *ren, Swapchain *swapchain);
static Err_Code RebuildSwapchain(Renderer *ren);
static Err_Code CreatePipeline(Renderer *ren);
static Err_Code ShaderManagerInit(Renderer *ren, Shader_Manager *shaders);
static Shader *ShaderManagerOpen(Renderer *ren, Shader_Manager *shaders, String path, 
    Shader_Type type);
static void ShaderManagerDeinit(Renderer *ren, Shader_Manager *shaders);
static void ShaderDestroy(void *ud, String name, void *item);
static Err_Code TechniqueManagerInit(Renderer *ren, Technique_Manager *techs);
static Err_Code TechniqueManagerOpen(Renderer *ren, Technique_Manager *techs, 
    String name);
static Technique *TechniqueManagerLookup(Technique_Manager *techs, String name);
static void TechniqueManagerDeinit(Renderer *ren, Technique_Manager *techs);
static void TechDestroy(void *ud, String name, void *ptr);
static Err_Code RenderGraphInit(Renderer *ren, Render_Graph *graph);
static void RenderGraphDeinit(Renderer *ren, Render_Graph *graph);
static void RenderGraphRecord(Renderer *ren, Render_Graph *graph,
                  u32 imageIndex);
static Err_Code CreateFramebuffers(Renderer *ren, Render_Graph *graph);
static Err_Code CreateBuffers(Renderer *ren);
static void DestroyBuffers(Renderer *ren);
static uint32_t FindMemoryType(Renderer *ren, uint32_t typeFilter, 
    VkMemoryPropertyFlags props);
static Err_Code CreateBuffer(Renderer *ren, VkDeviceSize size, 
    VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, 
    VkBuffer *buffer, VkDeviceMemory *bufferMemory);
static void DestroyBuffer(Renderer *ren, VkBuffer buffer, 
    VkDeviceMemory memory);
static void CopyBuffer(Renderer *ren, VkBuffer srcBuffer, VkBuffer dstBuffer, 
    VkDeviceSize size);
static Err_Code CreateCommandPools(Renderer *ren);
static void DestroyCommandPools(Renderer *ren);

/* === PUBLIC FUNCTIONS === */

Err_Code 
RendererCreate(Renderer_Create_Info *createInfo, 
               Renderer **renOut)
{
  Err_Code err;
  Technique *tech;
  Renderer *ren = NEW(createInfo->alloc, Renderer, MEMORY_TAG_RENDERER);

  ren->win = createInfo->win;
  ren->allocCbs = NULL;
  ren->alloc = createInfo->alloc;
  ren->fs = createInfo->fs;
  ren->currentFrame = 0;

  err = CreateInstance(ren);
  if (err)
  {
    return err;
  }
  LOG_DEBUG("created vulkan instance");

  err = PlatWindowCreateVulkanSurface(ren->win, ren->vk, ren->allocCbs, 
      &ren->surface);
  if (err)
  {
    return err;
  }
  LOG_DEBUG("created surface");

  err = SelectPhysicalDevice(ren);
  if (err)
  {
    return err;
  }
  LOG_DEBUG("selected suitable physical device");

  err = CreateLogicalDevice(ren);
  if (err)
  {
    return err;
  }
  LOG_DEBUG("created logical device");

  err = CreateSwapchain(ren, &ren->swapchain);
  if (err)
  {
    return err;
  }
  LOG_DEBUG("created swapchain");

  err = CreateCommandPools(ren);
  if (err)
  {
    return err;
  }
  LOG_DEBUG("created command pools");

  err = ShaderManagerInit(ren, &ren->shaders);
  if (err)
  {
    return err;
  }
  LOG_DEBUG("created shader manager");

  err = TechniqueManagerInit(ren, &ren->techs);
  if (err)
  {
    return err;
  }
  LOG_DEBUG("created technique manager");

  err = TechniqueManagerOpen(ren, &ren->techs, STRING_CSTR("tri.bson"));
  if (err)
  {
    return err;
  }
  LOG_DEBUG("loaded 'tri.bson' file");

  err = RenderGraphInit(ren, &ren->graph);
  if (err)
  {
    return err;
  }
  LOG_DEBUG("created render graph");

  err = CreateBuffers(ren);
  if (err)
  {
    return err;
  }
  LOG_DEBUG("created buffers");

  *renOut = ren;
  return ERR_OK;
}

Err_Code 
RendererDraw(Renderer *ren)
{
  VkResult vkErr;
  uint32_t imageIndex;

  vkWaitForFences(ren->dev, 1, &ren->graph.inFlightFences[ren->currentFrame], 
      VK_TRUE, UINT64_MAX);
  vkErr = vkAcquireNextImageKHR(ren->dev, ren->swapchain.swapchain, UINT64_MAX, 
      ren->graph.imageAvailableSemaphores[ren->currentFrame], VK_NULL_HANDLE, 
      &imageIndex);
  if (vkErr == VK_ERROR_OUT_OF_DATE_KHR)
  {
    RebuildResize(ren);
    return ERR_OK;
  } else if (vkErr && vkErr != VK_SUBOPTIMAL_KHR)
  {
    return ERR_LIBRARY_FAILURE;
  }

  vkResetFences(ren->dev, 1, &ren->graph.inFlightFences[ren->currentFrame]);

  vkResetCommandBuffer(ren->graph.commandBuffers[ren->currentFrame], 0);

  RenderGraphRecord(ren, &ren->graph, imageIndex);

  VkSemaphore waitSemaphores[] = 
    {ren->graph.imageAvailableSemaphores[ren->currentFrame]};
  VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  VkSemaphore signalSemaphores[] = 
    {ren->graph.renderFinishedSemaphores[ren->currentFrame]};

  VkSubmitInfo submitInfo =
  {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = waitSemaphores,
    .pWaitDstStageMask = waitStages,
    .commandBufferCount = 1,
    .pCommandBuffers = &ren->graph.commandBuffers[ren->currentFrame],
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = signalSemaphores,
  };

  vkErr = vkQueueSubmit(ren->graphicsQueue, 1, &submitInfo, 
      ren->graph.inFlightFences[ren->currentFrame]);
  if (vkErr)
  {
    return ERR_LIBRARY_FAILURE;
  }

  VkSwapchainKHR swapchains[] = {ren->swapchain.swapchain};

  VkPresentInfoKHR presentInfo =
  {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = signalSemaphores,
    .swapchainCount = 1,
    .pSwapchains = swapchains,
    .pImageIndices= &imageIndex,
  };

  vkErr = vkQueuePresentKHR(ren->presentQueue, &presentInfo);
  if (vkErr == VK_ERROR_OUT_OF_DATE_KHR || vkErr == VK_SUBOPTIMAL_KHR)
  {
    RebuildResize(ren);
  } else if (vkErr)
  {
    return ERR_LIBRARY_FAILURE;
  }

  ren->currentFrame = (ren->currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
  return ERR_OK;
}

void 
RendererDestroy(Renderer *ren)
{
  /* First finish all GPU work. */
  vkDeviceWaitIdle(ren->dev);

  
  DestroyCommandPools(ren);
  DestroyBuffers(ren);
  RenderGraphDeinit(ren, &ren->graph);
  ShaderManagerDeinit(ren, &ren->shaders);
  TechniqueManagerDeinit(ren, &ren->techs);
  DestroySwapchain(ren, &ren->swapchain);
  vkDestroySurfaceKHR(ren->vk, ren->surface, ren->allocCbs);
  vkDestroyDevice(ren->dev, ren->allocCbs);
  vkDestroyInstance(ren->vk, ren->allocCbs);
  FREE(ren->alloc, ren, Renderer, MEMORY_TAG_RENDERER);
}

/* === PRIVATE FUNCTIONS === */

static Err_Code 
CreateInstance(Renderer *ren)
{
  VkResult vkErr;
  u32 totalExtensionCount, platExtensionCount;
  const char **extensions;
  VkExtensionProperties *supportedExtensions;
  u32 supportedExtensionCount = 0;

  if (!PlatWindowGetInstanceExtensions(ren->win, &platExtensionCount, NULL))
  {
    return ERR_LIBRARY_FAILURE;
  }

  totalExtensionCount = platExtensionCount + 
    (sizeof(requiredExtensions) / sizeof(requiredExtensions[0]));

  extensions = NEW_ARR(ren->alloc, const char *, totalExtensionCount, 
      MEMORY_TAG_ARRAY);

  if (!PlatWindowGetInstanceExtensions(ren->win, &platExtensionCount, 
        extensions))
  {
    return ERR_LIBRARY_FAILURE;
  }

  for (u32 i = platExtensionCount; i < totalExtensionCount; i++)
  {
    extensions[i] = requiredExtensions[i - platExtensionCount];
  }

  vkEnumerateInstanceExtensionProperties(NULL, &supportedExtensionCount, NULL);
  supportedExtensions = NEW_ARR(ren->alloc, VkExtensionProperties, 
      supportedExtensionCount, MEMORY_TAG_ARRAY);

  vkEnumerateInstanceExtensionProperties(NULL, &supportedExtensionCount, 
      supportedExtensions);

  VkApplicationInfo applicationInfo = {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pApplicationName = "notte",
    .applicationVersion = VK_MAKE_VERSION(0, 0, 1),
    .pEngineName = "notte engine",
    .engineVersion = VK_MAKE_VERSION(0, 0, 1),
    .apiVersion = VK_API_VERSION_1_0,
  };

  VkInstanceCreateInfo createInfo = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo = &applicationInfo,
    .enabledLayerCount = sizeof(requiredLayers) / sizeof(requiredLayers[0]),
    .ppEnabledLayerNames = requiredLayers,
    .enabledExtensionCount = totalExtensionCount,
    .ppEnabledExtensionNames = extensions,
  };

  vkErr = vkCreateInstance(&createInfo, ren->allocCbs, &ren->vk);
  if (vkErr)
  {
    return ERR_LIBRARY_FAILURE;
  }

  FREE_ARR(ren->alloc, (char *) extensions, const char *, totalExtensionCount, 
      MEMORY_TAG_ARRAY);
  FREE_ARR(ren->alloc, supportedExtensions, VkExtensionProperties, 
      supportedExtensionCount, MEMORY_TAG_ARRAY);
  return ERR_OK;
}

static bool 
DeviceIsSuitable(Renderer *ren, 
                 VkPhysicalDevice dev, 
                 Queue_Family_Info *info)
{
  u32 nQueueFamilies;
  VkQueueFamilyProperties *queueFamilies;
  bool hasGraphics = false, hasPresent = false;
  u32 nExtensions;
  VkExtensionProperties *extensions;
  u32 nFormats, nPresentModes;

  vkEnumerateDeviceExtensionProperties(dev, NULL, &nExtensions, NULL);
  extensions = NEW_ARR(ren->alloc, VkExtensionProperties, nExtensions, 
      MEMORY_TAG_ARRAY);
  vkEnumerateDeviceExtensionProperties(dev, NULL, &nExtensions, extensions);

  vkGetPhysicalDeviceSurfaceFormatsKHR(dev, ren->surface, &nFormats, 
      NULL);

  vkGetPhysicalDeviceSurfacePresentModesKHR(dev, ren->surface, 
      &nPresentModes, NULL);
  if (nFormats == 0 || nPresentModes == 0)
  {
    return false;
  }

  for (u32 i = 0; 
      i < sizeof(requiredDeviceExtensions) / sizeof(requiredDeviceExtensions[0]);
      i++)
  {
    for (u32 j = 0; j < nExtensions; j++)
    {
      if (strcmp(extensions[j].extensionName, requiredDeviceExtensions[i]) == 0)
      {
        goto found;
      }
    }
    FREE_ARR(ren->alloc, extensions, VkExtensionProperties, nExtensions, 
        MEMORY_TAG_ARRAY);
    return false;
found:
    continue;
  }

  vkGetPhysicalDeviceQueueFamilyProperties(dev, &nQueueFamilies, NULL);

  queueFamilies = NEW_ARR(ren->alloc, VkQueueFamilyProperties, nQueueFamilies, 
      MEMORY_TAG_ARRAY);

  vkGetPhysicalDeviceQueueFamilyProperties(dev, &nQueueFamilies, queueFamilies);

  for (u32 i = 0; i < nQueueFamilies; i++)
  {
    VkBool32 presentSupport = false;

    if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
    {
      info->graphicsFamily = i;
      hasGraphics = true;
    }

    vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, ren->surface, &presentSupport);
    if (presentSupport)
    {
      info->presentFamily = i;
      hasPresent = true;
    }
  }

  FREE_ARR(ren->alloc, queueFamilies, VkQueueFamilyProperties, nQueueFamilies, 
      MEMORY_TAG_ARRAY);
  FREE_ARR(ren->alloc, extensions, VkExtensionProperties, nExtensions, 
      MEMORY_TAG_ARRAY);
  return hasGraphics & hasPresent;
}

static Err_Code 
SelectPhysicalDevice(Renderer *ren)
{
  u32 nDevs;
  VkPhysicalDevice *devs;

  vkEnumeratePhysicalDevices(ren->vk, &nDevs, NULL);
  if (nDevs == 0)
  {
    return ERR_NO_SUITABLE_HARDWARE;
  }

  devs = NEW_ARR(ren->alloc, VkPhysicalDevice, nDevs, MEMORY_TAG_ARRAY);

  vkEnumeratePhysicalDevices(ren->vk, &nDevs, devs);

  for (u32 i = 0; i < nDevs; i++)
  {
    if (DeviceIsSuitable(ren, devs[i], &ren->queueInfo))
    {
      ren->pDev = devs[i];
      FREE_ARR(ren->alloc, devs, VkPhysicalDevice, nDevs, MEMORY_TAG_ARRAY);
      return ERR_OK;
    }
  }

  LOG_DEBUG("failed to find suitable VkPhysicalDevice");
  return ERR_NO_SUITABLE_HARDWARE;
}

static Err_Code
CreateLogicalDevice(Renderer *ren)
{
  VkResult vkErr;
  f32 queuePriority = 1.0f;
  u32 queueCreateInfoCount;

  VkDeviceQueueCreateInfo queueCreateInfos[] =
  {
    {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = ren->queueInfo.graphicsFamily,
      .queueCount = 1,
      .pQueuePriorities = &queuePriority,
    },
    {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = ren->queueInfo.presentFamily,
      .queueCount = 1,
      .pQueuePriorities = &queuePriority,
    },
  };

  VkPhysicalDeviceFeatures deviceFeatures = {0};

  queueCreateInfoCount = 
    ren->queueInfo.graphicsFamily == ren->queueInfo.presentFamily ? 1 : 2;

  VkDeviceCreateInfo createInfo =
  {
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .pQueueCreateInfos = queueCreateInfos,
    .queueCreateInfoCount = queueCreateInfoCount,
    .pEnabledFeatures = &deviceFeatures,
    .enabledExtensionCount = 
      sizeof(requiredDeviceExtensions) / sizeof(requiredDeviceExtensions[0]),
    .ppEnabledExtensionNames = requiredDeviceExtensions,
    .enabledLayerCount = sizeof(requiredLayers) / sizeof(requiredLayers[0]),
    .ppEnabledLayerNames = requiredLayers,
  };

  vkErr = vkCreateDevice(ren->pDev, &createInfo, ren->allocCbs, &ren->dev);
  if (vkErr)
  {
    return ERR_LIBRARY_FAILURE;
  }

  vkGetDeviceQueue(ren->dev, ren->queueInfo.graphicsFamily, 0, 
      &ren->graphicsQueue);
  vkGetDeviceQueue(ren->dev, ren->queueInfo.presentFamily, 0, 
      &ren->presentQueue);
  return ERR_OK;
}

static Err_Code 
CreateSwapchain(Renderer *ren,
                Swapchain *swapchain)
{
  VkResult vkErr;
  VkSurfaceCapabilitiesKHR capabilities;
  u32 nFormats, nPresentModes;
  VkSurfaceFormatKHR *formats;
  VkExtent2D actualExtent;
  VkPresentModeKHR *presentModes;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ren->pDev, ren->surface, 
      &capabilities);

  vkGetPhysicalDeviceSurfaceFormatsKHR(ren->pDev, ren->surface, &nFormats, 
      NULL);

  formats = NEW_ARR(ren->alloc, VkSurfaceFormatKHR, nFormats, MEMORY_TAG_ARRAY);

  vkGetPhysicalDeviceSurfaceFormatsKHR(ren->pDev, ren->surface, &nFormats, 
      formats);

  for (u32 i = 0; i < nFormats; i++)
  {
    if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && 
        formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
    {
      swapchain->format = formats[i];
      goto foundFormat;
    }
  }
  swapchain->format = formats[0];
foundFormat:

  vkGetPhysicalDeviceSurfacePresentModesKHR(ren->pDev, ren->surface, 
      &nPresentModes, NULL);

  presentModes = NEW_ARR(ren->alloc, VkPresentModeKHR, nPresentModes, 
      MEMORY_TAG_ARRAY);

  vkGetPhysicalDeviceSurfacePresentModesKHR(ren->pDev, ren->surface, 
      &nPresentModes, presentModes);

  for (u32 i = 0; i < nPresentModes; i++)
  {
    if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) 
    {
      swapchain->presentMode = presentModes[i];
      goto foundMailbox;
    }
  }
  swapchain->presentMode = VK_PRESENT_MODE_FIFO_KHR;
foundMailbox:

  if (capabilities.currentExtent.width != 0xFFFFFFFF)
  {
    swapchain->extent = capabilities.currentExtent;
  } else
  {
    u32 w, h;
    PlatWindowGetFramebufferSize(ren->win, &w, &h);

    actualExtent.width = w;
    actualExtent.height = h;

    actualExtent.width = CLAMP(actualExtent.width, 
        capabilities.minImageExtent.width,
        capabilities.maxImageExtent.width);
    actualExtent.height = CLAMP(actualExtent.height, 
        capabilities.minImageExtent.height,
        capabilities.maxImageExtent.height);

    swapchain->extent = actualExtent;
  }

  swapchain->nImages = capabilities.minImageCount + 1;
  if (capabilities.maxImageCount > 0 && 
      swapchain->nImages > capabilities.maxImageCount)
  {
    swapchain->nImages = capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR createInfo = 
  {
    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .surface = ren->surface,
    .minImageCount = swapchain->nImages,
    .imageFormat = swapchain->format.format,
    .imageColorSpace = swapchain->format.colorSpace,
    .imageExtent = swapchain->extent,
    .imageArrayLayers = 1,
    .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    .preTransform = capabilities.currentTransform,
    .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    .presentMode = swapchain->presentMode,
    .clipped = VK_TRUE,
    .oldSwapchain = VK_NULL_HANDLE,
  };

  if (ren->queueInfo.graphicsFamily != ren->queueInfo.presentFamily)
  {
    uint32_t queueFamilyIndices[] = {
      ren->queueInfo.graphicsFamily,
      ren->queueInfo.presentFamily,
    };

    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  } else
  {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  vkErr = vkCreateSwapchainKHR(ren->dev, &createInfo, ren->allocCbs, 
      &swapchain->swapchain);
  if (vkErr)
  {
    return ERR_LIBRARY_FAILURE;
  }


  vkGetSwapchainImagesKHR(ren->dev, swapchain->swapchain, &swapchain->nImages,
      NULL);
  swapchain->images = NEW_ARR(ren->alloc, VkImage, swapchain->nImages, 
      MEMORY_TAG_ARRAY);
  vkGetSwapchainImagesKHR(ren->dev, swapchain->swapchain, &swapchain->nImages,
      swapchain->images);

  swapchain->imageViews = NEW_ARR(ren->alloc, VkImageView, swapchain->nImages,
      MEMORY_TAG_ARRAY);

  for (u32 i = 0; i < swapchain->nImages; i++)
  {
    VkImageViewCreateInfo createInfo =
    {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = swapchain->images[i],
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = swapchain->format.format,
      .components = {
        .r = VK_COMPONENT_SWIZZLE_IDENTITY,
        .g = VK_COMPONENT_SWIZZLE_IDENTITY,
        .b = VK_COMPONENT_SWIZZLE_IDENTITY,
        .a = VK_COMPONENT_SWIZZLE_IDENTITY,
      },
      .subresourceRange = 
      {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
      },
    };

    vkErr = vkCreateImageView(ren->dev, &createInfo, ren->allocCbs, 
        &swapchain->imageViews[i]);
    if (vkErr)
    {
      return ERR_LIBRARY_FAILURE;
    }
  }
  FREE_ARR(ren->alloc, presentModes, VkPresentModeKHR, nPresentModes, 
      MEMORY_TAG_ARRAY);
  FREE_ARR(ren->alloc, formats, VkSurfaceFormatKHR, nFormats, 
      MEMORY_TAG_ARRAY);

  return ERR_OK;
}

static void
DestroySwapchain(Renderer *ren, Swapchain *swapchain)
{
  for (u32 i = 0; i < swapchain->nImages; i++)
  {
    vkDestroyImageView(ren->dev, swapchain->imageViews[i], ren->allocCbs);
  }

  FREE_ARR(ren->alloc, swapchain->imageViews, VkImageView, swapchain->nImages, 
      MEMORY_TAG_ARRAY);
  FREE_ARR(ren->alloc, swapchain->images, VkImage, swapchain->nImages, 
      MEMORY_TAG_ARRAY);
  vkDestroySwapchainKHR(ren->dev, swapchain->swapchain, ren->allocCbs);
}

static Err_Code 
CreateTechnique(Renderer *ren, 
                const u8 *path, 
                Technique **tech)
{
  Err_Code err;
  Membuf buf;
  Bson_Ast *ast;
  Parse_Result result;
  Bson_Value *globals, *iter;
  String key;

  err = MembufLoadFile(&buf, path, ren->alloc);
  if (err)
  {
    LOG_ERROR_FMT("Failed to load technique file: '%s'", path);
    return err;
  }

  err = BsonAstParse(&ast, ren->alloc, &result, buf);
  if (err)
  {
    return err;
  }

  return err;
}

static Err_Code 
ShaderManagerInit(Renderer *ren, 
                  Shader_Manager *shaders)
{
  shaders->dict = DICT_CREATE(ren->alloc, Shader, false);
  shaders->compiler = shaderc_compiler_initialize();
  shaders->fs = ren->fs;
  return ERR_OK;
}

static void 
ShaderManagerDeinit(Renderer *ren, 
                   Shader_Manager *shaders)
{
  DictDestroyWithDestructor(shaders->dict, ren, ShaderDestroy);
  shaderc_compiler_release(shaders->compiler);
}

static void
ShaderDestroy(void *ud, String name, void *item)
{
  Renderer *ren = (Renderer *) ud;
  Shader *shader = (Shader *) item;
  vkDestroyShaderModule(ren->dev, shader->mod, ren->allocCbs);
}

static Shader *
ShaderManagerOpen(Renderer *ren, 
                  Shader_Manager *shaders, 
                  String name, 
                  Shader_Type type)
{
  Err_Code err;
  String path;
  Shader *shader, *iter;
  Membuf buf;
  VkResult vkErr;

  shader = DictFind(shaders->dict, name);
  if (shader != NULL)
  {
    return shader;
  }

  shader = DictInsertWithoutInit(shaders->dict, name);

  path = StringConcat(ren->alloc, STRING_CSTR("shaders/"), name);

  err = FsFileLoad(shaders->fs, path, &buf);
  if (err)
  {
    goto fail;
  }

  shaderc_compilation_result_t result = 
    shaderc_compile_into_spv(shaders->compiler, buf.data, buf.size, 
        type == SHADER_VERT ? shaderc_glsl_vertex_shader 
                            : shaderc_glsl_fragment_shader, 
        name.buf, "main", NULL);

  if (shaderc_result_get_compilation_status(result))
  {
    LOG_DEBUG_FMT("failed to compile shader: \n\n%s", 
        shaderc_result_get_error_message(result));
    return NULL;
  }

  shader->name = name;

  VkShaderModuleCreateInfo createInfo =
  {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = shaderc_result_get_length(result),
    .pCode = (u32 *) shaderc_result_get_bytes(result),
  };

  vkErr = vkCreateShaderModule(ren->dev, &createInfo, ren->allocCbs, 
      &shader->mod);
  if (vkErr)
  {
    return NULL;
  }


  shaderc_result_release(result);
  FsFileDestroy(shaders->fs, &buf);

  StringDestroy(ren->alloc, path);
  return shader;

fail:
  StringDestroy(ren->alloc, path);
  return NULL;
}

static Err_Code 
TechniqueManagerInit(Renderer *ren, 
                     Technique_Manager *techs)
{
  techs->dict = DICT_CREATE(ren->alloc, Technique, false);
  return ERR_OK;
}

static void 
TechniqueManagerDeinit(Renderer *ren, 
                       Technique_Manager *techs)
{
  DictDestroyWithDestructor(techs->dict, ren, TechDestroy);
}

static void
TechDestroy(void *ud, 
            String name,
            void *ptr)
{
  Renderer *ren = (Renderer *) ud;
  Technique *tech = (Technique *) ptr;

  vkDestroyPipeline(ren->dev, tech->pipeline, ren->allocCbs);
  vkDestroyPipelineLayout(ren->dev, tech->layout, ren->allocCbs);
  vkDestroyRenderPass(ren->dev, tech->fakePass, ren->allocCbs);
}

static Err_Code
TechniqueManagerOpen(Renderer *ren, 
                     Technique_Manager *techs, 
                     String name)
{
  Err_Code err;
  Membuf buf;
  Parse_Result result;
  Bson_Ast *ast;
  Bson_Dict_Iterator iter;
  Bson_Value *globals, *techDict;
  String techName, path;

  path = StringConcat(ren->alloc, STRING_CSTR("assets/"), name);

  err = FsFileLoad(ren->fs, path, &buf);
  if (err)
  {
    goto fail;
  }

  err = BsonAstParse(&ast, ren->alloc, &result, buf);
  if (err)
  {
    goto fail;
  }

  globals = BsonAstGetValue(ast);

  BsonDictIteratorCreate(globals, &iter);
  while (BsonDictIteratorNext(&iter, &techName, &techDict))
  {
    Technique *tech;

    tech = DictInsertWithoutInit(techs->dict, techName);

    String vertStr, fragStr;
    Bson_Value *vertName, *fragName;
    vertName = BsonValueLookup(techDict, STRING_CSTR("vert"));
    if (vertName == NULL)
    {
      LOG_DEBUG_FMT("failed to find vert shader in '%.*s'", 
          (int) techName.len, techName.buf);
      err = ERR_FAILED_PARSE;
      goto fail;
    }
    fragName = BsonValueLookup(techDict, STRING_CSTR("frag"));
    if (fragName == NULL)
    {
      LOG_DEBUG_FMT("failed to find frag shader in '%.*s'", 
          (int) techName.len, techName.buf);
      err = ERR_FAILED_PARSE;
      goto fail;
    }

    vertStr = BsonValueGetString(vertName);
    fragStr = BsonValueGetString(fragName);

    Shader *vertShader, *fragShader;
    vertShader = ShaderManagerOpen(ren, &ren->shaders, vertStr, SHADER_VERT);
    fragShader = ShaderManagerOpen(ren, &ren->shaders, fragStr, SHADER_FRAG);

    tech->vert = vertShader;
    tech->frag = fragShader;

    err = InitTechnique(ren, tech);
    if (err)
    {
      return err;
    }

    LOG_DEBUG_FMT("loaded technique '%.*s'", (int) techName.len, techName.buf);
  }
  
  BsonAstDestroy(ast, ren->alloc);
  FsFileDestroy(ren->fs, &buf);
  StringDestroy(ren->alloc, path);
  return ERR_OK;

fail:
  StringDestroy(ren->alloc, path);
  return err;
}

static Err_Code 
InitTechnique(Renderer *ren, 
              Technique *tech)
{
  VkResult vkErr;

  VkDynamicState dynamicStates[] =
  {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,
  };

  VkPipelineDynamicStateCreateInfo dynamicState =
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .dynamicStateCount = 2,
    .pDynamicStates = dynamicStates,
  };

  VkPipelineVertexInputStateCreateInfo vertexInputInfo =
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = 1,
    .vertexAttributeDescriptionCount = 2,
    .pVertexBindingDescriptions = &vertexBindingDescription,
    .pVertexAttributeDescriptions = vertexAttributeDescription,
  };

  VkPipelineInputAssemblyStateCreateInfo inputAssembly =
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    .primitiveRestartEnable = VK_FALSE,
  };

  VkViewport viewport =
  {
    .x = 0.0f,
    .y = 0.0f,
    .width = (float) ren->swapchain.extent.width,
    .height = (float) ren->swapchain.extent.height,
    .minDepth = 0.0f,
    .maxDepth = 1.0f,
  };

  VkRect2D scissor =
  {
    .offset = {0, 0},
    .extent = ren->swapchain.extent,
  };

  VkPipelineViewportStateCreateInfo viewportState =
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .scissorCount = 1,
  };

  VkPipelineRasterizationStateCreateInfo rasterizer =
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .depthClampEnable = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .lineWidth = 1.0f,
    .cullMode = VK_CULL_MODE_BACK_BIT,
    .frontFace = VK_FRONT_FACE_CLOCKWISE,
    .depthBiasEnable = VK_FALSE,
  };

  VkPipelineMultisampleStateCreateInfo multisampler =
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .sampleShadingEnable = VK_FALSE,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
  };

  VkPipelineColorBlendAttachmentState colorBlendAttachment =
  {

    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT 
      | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    .blendEnable = VK_FALSE,
  };

  VkPipelineColorBlendStateCreateInfo colorBlend =
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .logicOpEnable = VK_FALSE,
    .attachmentCount = 1,
    .pAttachments = &colorBlendAttachment,
  };

  VkPipelineLayoutCreateInfo pipelineLayoutInfo = 
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
  };

  vkErr = vkCreatePipelineLayout(ren->dev, &pipelineLayoutInfo, ren->allocCbs, 
      &tech->layout);
  if (vkErr)
  {
    return ERR_LIBRARY_FAILURE;
  }

  VkAttachmentDescription colorAttachment =
  {
    .format = ren->swapchain.format.format,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
  };

  VkAttachmentReference colorAttachmentRef =
  {
    .attachment = 0,
    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };

  VkSubpassDescription subpass =
  {
    .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
    .colorAttachmentCount = 1,
    .pColorAttachments = &colorAttachmentRef,
  };

  VkSubpassDependency dependency =
  {
    .srcSubpass = VK_SUBPASS_EXTERNAL,
    .dstSubpass = 0,
    .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    .srcAccessMask = 0,
    .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
  };

  VkRenderPassCreateInfo renderPassInfo =
  {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .attachmentCount = 1,
    .pAttachments = &colorAttachment,
    .subpassCount = 1,
    .pSubpasses = &subpass,
    .dependencyCount = 1,
    .pDependencies = &dependency,
  };

  vkErr = vkCreateRenderPass(ren->dev, &renderPassInfo, ren->allocCbs, 
      &tech->fakePass);
  if (vkErr)
  {
    return ERR_OK;
  }

  VkPipelineShaderStageCreateInfo vertShaderStageInfo =
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = VK_SHADER_STAGE_VERTEX_BIT,
    .module = tech->vert->mod,
    .pName = "main",
  };

  VkPipelineShaderStageCreateInfo fragShaderStageInfo =
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
    .module = tech->frag->mod,
    .pName = "main",
  };

  VkPipelineShaderStageCreateInfo shaderStages[] = 
  {
    vertShaderStageInfo, fragShaderStageInfo
  };

  VkGraphicsPipelineCreateInfo pipelineInfo =
  {
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .stageCount = 2,
    .pStages = shaderStages,
    .pVertexInputState = &vertexInputInfo,
    .pInputAssemblyState = &inputAssembly,
    .pViewportState = &viewportState,
    .pRasterizationState = &rasterizer,
    .pMultisampleState = &multisampler,
    .pColorBlendState = &colorBlend,
    .pDynamicState = &dynamicState,
    .layout = tech->layout,
    .renderPass = tech->fakePass,
    .subpass = 0,
  };

  vkErr = vkCreateGraphicsPipelines(ren->dev, VK_NULL_HANDLE, 1, &pipelineInfo,
      ren->allocCbs, &tech->pipeline);
  if (vkErr)
  {
    return ERR_LIBRARY_FAILURE;
  }

  return ERR_OK;
}


static Technique *
TechniqueManagerLookup(Technique_Manager *techs, 
                       String name)
{
  return DictFind(techs->dict, name);
}

static Err_Code 
CreateFramebuffers(Renderer *ren, Render_Graph *graph)
{
  VkResult vkErr;
  Technique *tech;
  graph->swapFbs = NEW_ARR(ren->alloc, VkFramebuffer, ren->swapchain.nImages, 
      MEMORY_TAG_RENDERER);

  tech = TechniqueManagerLookup(&ren->techs, STRING_CSTR("tri"));
  for (usize i = 0; i < ren->swapchain.nImages; i++)
  {
    VkImageView attachments[] =
    {
      ren->swapchain.imageViews[i],
    };

    VkFramebufferCreateInfo framebufferInfo = 
    {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass = tech->fakePass,
      .attachmentCount = 1,
      .pAttachments = attachments,
      .width = ren->swapchain.extent.width,
      .height = ren->swapchain.extent.height,
      .layers = 1,
    };

    vkErr = vkCreateFramebuffer(ren->dev, &framebufferInfo, ren->allocCbs, 
        &graph->swapFbs[i]);
    if (vkErr)
    {
      return ERR_LIBRARY_FAILURE;
    }
  }

  return ERR_OK;
}

static Err_Code 
RenderGraphInit(Renderer *ren, 
                Render_Graph *graph)
{
  Err_Code err;
  VkResult vkErr;

  err = CreateFramebuffers(ren, graph);
  if (err)
  {
    return err;
  }

  VkCommandPoolCreateInfo poolInfo =
  {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = ren->queueInfo.graphicsFamily,
  };

  vkErr = vkCreateCommandPool(ren->dev, &poolInfo, ren->allocCbs, 
      &graph->commandPool);
  if (vkErr)
  {
    return ERR_LIBRARY_FAILURE;
  }

  VkCommandBufferAllocateInfo allocInfo =
  {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = graph->commandPool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
  };

  vkErr = vkAllocateCommandBuffers(ren->dev, &allocInfo, graph->commandBuffers);
  if (vkErr)
  {
    return ERR_LIBRARY_FAILURE;
  }

  VkSemaphoreCreateInfo semaphoreInfo = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };

  VkFenceCreateInfo fenceInfo =
  {
    .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
  };


  for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    vkErr = vkCreateSemaphore(ren->dev, &semaphoreInfo, ren->allocCbs, 
        &graph->imageAvailableSemaphores[i]);
    vkErr = vkCreateSemaphore(ren->dev, &semaphoreInfo, ren->allocCbs, 
        &graph->renderFinishedSemaphores[i]);
    vkErr = vkCreateFence(ren->dev, &fenceInfo, ren->allocCbs, 
        &graph->inFlightFences[i]);
    if (vkErr)
    {
      return ERR_LIBRARY_FAILURE;
    }
  }

  return ERR_OK;
}

static void 
RenderGraphDeinit(Renderer *ren, 
                  Render_Graph *graph)
{

  vkDestroyCommandPool(ren->dev, graph->commandPool, ren->allocCbs);

  for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    vkDestroySemaphore(ren->dev, graph->imageAvailableSemaphores[i], 
        ren->allocCbs);
    vkDestroySemaphore(ren->dev, graph->renderFinishedSemaphores[i], 
        ren->allocCbs);
    vkDestroyFence(ren->dev, graph->inFlightFences[i], ren->allocCbs);
  }

  for (usize i = 0; i < ren->swapchain.nImages; i++)
  {
    vkDestroyFramebuffer(ren->dev, ren->graph.swapFbs[i], ren->allocCbs);
  }

  FREE_ARR(ren->alloc, ren->graph.swapFbs, VkFramebuffer, ren->swapchain.nImages, 
      MEMORY_TAG_RENDERER);
}

static void 
RenderGraphRecord(Renderer *ren, 
                  Render_Graph *graph,
                  u32 imageIndex)
{
  Technique *tech = TechniqueManagerLookup(&ren->techs, STRING_CSTR("tri"));
  VkCommandBuffer buf = graph->commandBuffers[ren->currentFrame];

  VkResult vkErr;

  VkCommandBufferBeginInfo beginInfo =
  {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
  };

  vkErr = vkBeginCommandBuffer(buf, &beginInfo);
  if (vkErr)
  {
    LOG_ERROR("failed to begin command buffer");
    return;
  }

  VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};

  VkRenderPassBeginInfo renderPassInfo =
  {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .renderPass = tech->fakePass,
    .framebuffer = graph->swapFbs[imageIndex],
    .renderArea =
      {
        .offset = {0, 0},
        .extent = ren->swapchain.extent,
      },
    .clearValueCount = 1,
    .pClearValues = &clearColor,
  };

  vkCmdBeginRenderPass(buf, &renderPassInfo, 
      VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, 
      tech->pipeline);

  VkBuffer vertexBuffers[] = {ren->vertexBuffer};
  VkDeviceSize offsets[] = {0};

  vkCmdBindVertexBuffers(buf, 0, 1, vertexBuffers, offsets);
  vkCmdBindIndexBuffer(buf, ren->indexBuffer, 0, VK_INDEX_TYPE_UINT16);

  VkViewport viewport =
  {
    .x = 0.0f,
    .y = 0.0f,
    .width = (float) ren->swapchain.extent.width,
    .height = (float) ren->swapchain.extent.height,
    .minDepth = 0.0f,
    .maxDepth = 1.0f,
  };

  vkCmdSetViewport(buf, 0, 1, &viewport);

  VkRect2D scissor =
  {
    .offset = {0, 0},
    .extent = ren->swapchain.extent,
  };

  vkCmdSetScissor(buf, 0, 1, &scissor);

  vkCmdDrawIndexed(buf, 6, 1, 0, 0, 0);

  vkCmdEndRenderPass(buf);

  vkEndCommandBuffer(buf);
}

static Err_Code
RebuildRenderGraph(Renderer *ren)
{
  Err_Code err;

  for (usize i = 0; i < ren->swapchain.nImages; i++)
  {
    vkDestroyFramebuffer(ren->dev, ren->graph.swapFbs[i], ren->allocCbs);
  }

  FREE_ARR(ren->alloc, ren->graph.swapFbs, VkFramebuffer, ren->swapchain.nImages, 
      MEMORY_TAG_RENDERER);

  err = CreateFramebuffers(ren, &ren->graph);
  if (err)
  {
    return err;
  }
  return ERR_OK;
}

static Err_Code
RebuildResize(Renderer *ren)
{
  Err_Code err;
  err = RebuildSwapchain(ren);
  if (err)
  {
    return err;
  }

  err = RebuildRenderGraph(ren);
  if (err)
  {
    return err;
  }

  return ERR_OK;
}

static Err_Code 
RebuildSwapchain(Renderer *ren)
{
  Swapchain new;
  Err_Code err;

  vkDeviceWaitIdle(ren->dev);

  DestroySwapchain(ren, &ren->swapchain);

  err = CreateSwapchain(ren, &new);
  if (err)
  {
    return err;
  }

  ren->swapchain = new;

  
  return ERR_OK;
}

static Err_Code 
CreateBuffers(Renderer *ren)
{
  Err_Code err;
  void *data;
  VkResult vkErr;
  VkBuffer vStagingBuffer, iStagingBuffer;
  VkDeviceMemory vStagingBufferMemory, iStagingBufferMemory;

  VkDeviceSize vBufferSize = sizeof(vertices);
  err = CreateBuffer(ren, vBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      &vStagingBuffer, &vStagingBufferMemory);
  if (err)
  {
    return err;
  }

  vkMapMemory(ren->dev, vStagingBufferMemory, 0, vBufferSize, 0, &data);

  MemoryCopy(data, vertices, (usize) vBufferSize);
  vkUnmapMemory(ren->dev, vStagingBufferMemory);

  err = CreateBuffer(ren, vBufferSize, 
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &ren->vertexBuffer, &ren->vertexBufferMemory);
  if (err)
  {
    return err;
  }

  CopyBuffer(ren, vStagingBuffer, ren->vertexBuffer, vBufferSize);

  DestroyBuffer(ren, vStagingBuffer, vStagingBufferMemory);

  VkDeviceSize iBufferSize = sizeof(indices);
  err = CreateBuffer(ren, iBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      &iStagingBuffer, &iStagingBufferMemory);
  if (err)
  {
    return err;
  }

  vkMapMemory(ren->dev, iStagingBufferMemory, 0, iBufferSize, 0, &data);

  MemoryCopy(data, indices, (usize) iBufferSize);
  vkUnmapMemory(ren->dev, iStagingBufferMemory);

  err = CreateBuffer(ren, iBufferSize, 
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &ren->indexBuffer, 
      &ren->indexBufferMemory);
  if (err)
  {
    return err;
  }

  CopyBuffer(ren, iStagingBuffer, ren->indexBuffer, iBufferSize);

  DestroyBuffer(ren, iStagingBuffer, iStagingBufferMemory);

  return ERR_OK;
}

static void
DestroyBuffers(Renderer *ren)
{
  DestroyBuffer(ren, ren->vertexBuffer, ren->vertexBufferMemory);
  DestroyBuffer(ren, ren->indexBuffer, ren->indexBufferMemory);
}

static uint32_t 
FindMemoryType(Renderer *ren, uint32_t typeFilter, VkMemoryPropertyFlags props)
{
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(ren->pDev, &memProperties);

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
  {
    if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & props) == props)
    {
      return i;
    }
  }

  LOG_ERROR("failed to find suitable memory type");
  return 0;
}

static Err_Code
CreateBuffer(Renderer *ren, 
             VkDeviceSize size, 
             VkBufferUsageFlags usage, 
             VkMemoryPropertyFlags properties, 
             VkBuffer *buffer, 
             VkDeviceMemory *bufferMemory)
{
  VkResult vkErr;
  VkMemoryRequirements memRequirements;

  VkBufferCreateInfo bufferInfo =
  {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = size,
    .usage = usage,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };

  vkErr = vkCreateBuffer(ren->dev, &bufferInfo, ren->allocCbs, buffer);
  if (vkErr)
  {
    return ERR_LIBRARY_FAILURE;
  }

  vkGetBufferMemoryRequirements(ren->dev, *buffer, &memRequirements);

  VkMemoryAllocateInfo allocInfo =
  {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = memRequirements.size,
    .memoryTypeIndex = FindMemoryType(ren, memRequirements.memoryTypeBits, 
        properties),
  };

  vkErr = vkAllocateMemory(ren->dev, &allocInfo, ren->allocCbs, bufferMemory);
  if (vkErr)
  {
    return ERR_LIBRARY_FAILURE;
  }

  vkBindBufferMemory(ren->dev, *buffer, *bufferMemory, 0);

  return ERR_OK;
}

static void 
CopyBuffer(Renderer *ren,
           VkBuffer srcBuffer, 
           VkBuffer dstBuffer, 
           VkDeviceSize size)
{
  VkCommandBuffer commandBuffer;

  VkCommandBufferAllocateInfo allocInfo =
  {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandPool = ren->utilPool,
    .commandBufferCount = 1,
  };

  vkAllocateCommandBuffers(ren->dev, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo =
  {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  VkBufferCopy copyRegion = 
  {
    .srcOffset = 0,
    .dstOffset = 0,
    .size = size,
  };

  vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo = 
  {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .commandBufferCount = 1,
    .pCommandBuffers = &commandBuffer,
  };

  vkQueueSubmit(ren->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(ren->graphicsQueue);

  vkFreeCommandBuffers(ren->dev, ren->utilPool, 1, &commandBuffer);
}

static Err_Code 
CreateCommandPools(Renderer *ren)
{
  VkResult vkErr;
  VkCommandBuffer commandBuffer;

  VkCommandPoolCreateInfo poolInfo =
  {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = ren->queueInfo.graphicsFamily,
  };

  vkErr = vkCreateCommandPool(ren->dev, &poolInfo, ren->allocCbs, 
      &ren->utilPool);
  if (vkErr)
  {
    return ERR_LIBRARY_FAILURE;
  }

  return ERR_OK;
}

static void
DestroyCommandPools(Renderer *ren)
{
  vkDestroyCommandPool(ren->dev, ren->utilPool, ren->allocCbs);
}

static void 
DestroyBuffer(Renderer *ren, 
              VkBuffer buffer, 
              VkDeviceMemory memory)
{
  vkDestroyBuffer(ren->dev, buffer, ren->allocCbs);
  vkFreeMemory(ren->dev, memory, ren->allocCbs);
}
