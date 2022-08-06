/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Vulkan renderer.
 */

#include <notte/renderer.h>
#include <notte/membuf.h>
#include <notte/bson.h>
#include <notte/dict.h>
#include <notte/renderer_priv.h>
#include <notte/material.h>

/* === MACROS === */

#define CLAMP(_val, _min, _max) ((_val) > (_max) ? (_max) : ((_val) < (_min) ? \
      (_min) : (_val)))

#define INIT_DRAW_CALLS_ALLOC 32

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

/* === PROTOTYPES === */

static Transform TransformInit(void);
static Err_Code CreateDescriptorPool(Renderer *ren);
static void DestroyDescriptorPool(Renderer *ren);
static Err_Code CreateBuffers(Renderer *ren);
static void DestroyBuffers(Renderer *ren);
static Err_Code CreateInstance(Renderer *ren);
static Err_Code SelectPhysicalDevice(Renderer *ren);
static bool DeviceIsSuitable(Renderer *ren, VkPhysicalDevice dev, 
    Queue_Family_Info *info);
static Err_Code CreateLogicalDevice(Renderer *ren);
static Err_Code CreateSwapchain(Renderer *ren, Swapchain *swapchain);
static void DestroySwapchain(Renderer *ren, Swapchain *swapchain);
static Err_Code RebuildSwapchain(Renderer *ren);
static void CopyBuffer(Renderer *ren, VkBuffer srcBuffer, VkBuffer dstBuffer, 
    VkDeviceSize size);
static Err_Code CreateCommandPools(Renderer *ren);
static void DestroyCommandPools(Renderer *ren);
static void CameraSetMatrices(Renderer *ren, Camera *cam);

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

  ren->drawCalls = VECTOR_CREATE(ren->alloc, Draw_Call);

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
  
  err = CreateDescriptorPool(ren);
  if (err)
  {
    return err;
  }
  LOG_DEBUG("created descriptor pools");

  err = CreateBuffers(ren);
  if (err)
  {
    return err;
  }

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


  *renOut = ren;
  return ERR_OK;
}

Err_Code 
RendererDraw(Renderer *ren)
{
  VkResult vkErr;
  uint32_t imageIndex;
  Err_Code err;

  err = ShaderManagerReload(ren, &ren->shaders);
  if (err)
  {
    return err;
  }

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
  VectorDestroy(&ren->drawCalls, ren->alloc);
  DestroyBuffers(ren);

  DestroyCommandPools(ren);
  DestroyDescriptorPool(ren);
  RenderGraphDeinit(ren, &ren->graph);
  ShaderManagerDeinit(ren, &ren->shaders);
  TechniqueManagerDeinit(ren, &ren->techs);
  DestroySwapchain(ren, &ren->swapchain);
  vkDestroySurfaceKHR(ren->vk, ren->surface, ren->allocCbs);
  vkDestroyDevice(ren->dev, ren->allocCbs);
  vkDestroyInstance(ren->vk, ren->allocCbs);
  FREE(ren->alloc, ren, Renderer, MEMORY_TAG_RENDERER);
}

Err_Code 
RendererCreateStaticMesh(Renderer *ren, 
                         Static_Mesh_Create_Info *createInfo, 
                         Static_Mesh **meshOut)
{
  Err_Code err;
  void *data;
  VkResult vkErr;
  VkBuffer vStagingBuffer, iStagingBuffer;
  VkDeviceMemory vStagingBufferMemory, iStagingBufferMemory;
  VkDeviceSize vBufferSize, iBufferSize;

  Static_Mesh *mesh = NEW(ren->alloc, Static_Mesh, MEMORY_TAG_RENDERER);

  mesh->nVerts = createInfo->nVerts;
  mesh->nIndices = createInfo->nIndices;
  Vertex *verts = NEW_ARR(ren->alloc, Vertex, mesh->nVerts, MEMORY_TAG_ARRAY);
  MemoryCopy(verts, createInfo->verts, sizeof(Vertex) * mesh->nVerts);
  u16 *indices = NEW_ARR(ren->alloc, u16, mesh->nIndices, MEMORY_TAG_ARRAY);
  MemoryCopy(indices, createInfo->indices, sizeof(u16) * mesh->nIndices);
  mesh->verts = verts;
  mesh->indices = indices;


  vBufferSize = sizeof(Vertex) * mesh->nVerts;
  err = CreateBuffer(ren, vBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      &vStagingBuffer, &vStagingBufferMemory);
  if (err)
  {
    return err;
  }

  vkMapMemory(ren->dev, vStagingBufferMemory, 0, vBufferSize, 0, &data);

  MemoryCopy(data, mesh->verts, (usize) vBufferSize);
  vkUnmapMemory(ren->dev, vStagingBufferMemory);

  err = CreateBuffer(ren, vBufferSize, 
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &mesh->vertexBuffer, 
      &mesh->vertexMemory);
  if (err)
  {
    return err;
  }

  CopyBuffer(ren, vStagingBuffer, mesh->vertexBuffer, vBufferSize);

  DestroyBuffer(ren, vStagingBuffer, vStagingBufferMemory);

  iBufferSize = sizeof(u16) * mesh->nIndices;
  err = CreateBuffer(ren, iBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      &iStagingBuffer, &iStagingBufferMemory);
  if (err)
  {
    return err;
  }

  vkMapMemory(ren->dev, iStagingBufferMemory, 0, iBufferSize, 0, &data);

  MemoryCopy(data, mesh->indices, (usize) iBufferSize);
  vkUnmapMemory(ren->dev, iStagingBufferMemory);

  err = CreateBuffer(ren, iBufferSize, 
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &mesh->indexBuffer, 
      &mesh->indexMemory);
  if (err)
  {
    return err;
  }

  CopyBuffer(ren, iStagingBuffer, mesh->indexBuffer, iBufferSize);

  DestroyBuffer(ren, iStagingBuffer, iStagingBufferMemory);

  ren->mesh = mesh;
  *meshOut = mesh;

  return ERR_OK;
}

void 
RendererDestroyStaticMesh(Renderer *ren, 
                          Static_Mesh *mesh)
{
  vkDeviceWaitIdle(ren->dev);
  FREE_ARR(ren->alloc, (void *) mesh->verts, Vertex, mesh->nVerts, MEMORY_TAG_ARRAY);
  FREE_ARR(ren->alloc, (void *) mesh->indices, u16, mesh->nIndices, MEMORY_TAG_ARRAY);
  DestroyBuffer(ren, mesh->vertexBuffer, mesh->vertexMemory);
  DestroyBuffer(ren, mesh->indexBuffer, mesh->indexMemory);

  FREE(ren->alloc, mesh, Static_Mesh, MEMORY_TAG_RENDERER);
}

void 
RendererDrawStaticMesh(Renderer *ren, 
                       Static_Mesh *mesh, 
                       Transform transform)
{
  Draw_Call drawCall = 
  {
    .t = DRAW_CALL_STATIC_MESH,
    .staticMesh = mesh,
    .transform = transform,
  };

  VectorPush(&ren->drawCalls, ren->alloc, &drawCall);
}


Err_Code 
RendererCreateCamera(Renderer *ren, 
                     Camera **cameraOut)
{
  Camera *cam = NEW(ren->alloc, Camera, MEMORY_TAG_RENDERER);
  cam->trans = TransformInit();
  cam->fov = 45.0f;
  CameraSetMatrices(ren, cam);

  *cameraOut = cam;
  return ERR_OK;
}

void 
RendererDestroyCamera(Renderer *ren, 
                      Camera *cam)
{
  FREE(ren->alloc, cam, Camera, MEMORY_TAG_RENDERER);
}

void 
RendererSetCameraActive(Renderer *ren, 
                        Camera *cam)
{
  ren->cam = cam;
}

void 
RendererSetCameraTransform(Renderer *ren, 
                           Camera *cam, 
                           Transform trans)
{
  cam->trans = trans;
  CameraSetMatrices(ren, cam);
}

void 
RendererSetCameraFov(Renderer *ren, 
                     Camera *cam, 
                     f32 fov)
{
  cam->fov = fov;
  CameraSetMatrices(ren, cam);
}

/* === PRIVATE FUNCTIONS === */

static Transform 
TransformInit(void)
{
  Transform trans = 
  {
    .pos = {0, 0, 0},
    .rot = {0, 0, 0},
  };
  return trans;
}

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
RebuildResize(Renderer *ren)
{
  Err_Code err;
  err = RebuildSwapchain(ren);
  if (err)
  {
    return err;
  }

  err = RenderGraphRebuild(ren);
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

static Err_Code 
CreateBuffers(Renderer *ren)
{
  VkDeviceSize bufferSize = sizeof(Camera_Uniform);
  VkResult err;

  for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    err = CreateBuffer(ren, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &ren->uniformBuffers[i], &ren->uniformMemory[i]);
    if (err)
    {
      return err;
    }
  }

  return ERR_OK;
}

static void
DestroyBuffers(Renderer *ren)
{
  for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
  {
    DestroyBuffer(ren, ren->uniformBuffers[i], ren->uniformMemory[i]);
  }
}

static Err_Code 
CreateDescriptorPool(Renderer *ren)
{
  VkResult vkErr;

  VkDescriptorPoolSize poolSize = 
  {
    .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .descriptorCount = MAX_FRAMES_IN_FLIGHT
  };

  VkDescriptorPoolCreateInfo createInfo = 
  {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .poolSizeCount = 1,
    .pPoolSizes = &poolSize,
    .maxSets = MAX_FRAMES_IN_FLIGHT,
  };

  vkErr = vkCreateDescriptorPool(ren->dev, &createInfo, ren->allocCbs, 
      &ren->descriptorPool);
  if (vkErr)
  {
    return ERR_LIBRARY_FAILURE;
  }

  return ERR_OK;
}

static void 
DestroyDescriptorPool(Renderer *ren)
{
  vkDestroyDescriptorPool(ren->dev, ren->descriptorPool, ren->allocCbs);
}

static void
CameraSetMatrices(Renderer *ren, 
                  Camera *cam)
{
  Vec3 center = {0.0f, 0.0f, 0.0f};
  Vec3 up = {0.0f, 0.0f, 1.0f};

  Mat4Lookat(cam->trans.pos, center, up, cam->view);

  Mat4Perspective(cam->fov,
      ren->swapchain.extent.width / (float) ren->swapchain.extent.height, 
      0.1f, 10.0f, cam->proj);
  cam->proj[1][1] *= 1.0f;
}
