/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Vulkan renderer.
 */

#include <string.h>

#include <vulkan/vulkan.h>

#include <notte/renderer.h>

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

struct Renderer
{
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
};

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

/* === PROTOTYPES === */

static Err_Code CreateInstance(Renderer *ren);
static Err_Code SelectPhysicalDevice(Renderer *ren);
static bool DeviceIsSuitable(Renderer *ren, VkPhysicalDevice dev, 
    Queue_Family_Info *info);
static Err_Code CreateLogicalDevice(Renderer *ren);
static Err_Code CreateSwapchain(Renderer *ren, Swapchain *swapchain);
static void DestroySwapchain(Renderer *ren, Swapchain *swapchain);

/* === PUBLIC FUNCTIONS === */

Err_Code 
RendererCreate(Renderer_Create_Info *createInfo, 
               Renderer **renOut)
{
  Err_Code err;
  Renderer *ren = NEW(createInfo->alloc, Renderer, MEMORY_TAG_RENDERER);

  ren->win = createInfo->win;
  ren->allocCbs = NULL;
  ren->alloc = createInfo->alloc;

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

  *renOut = ren;
  return ERR_OK;
}

void 
RendererDestroy(Renderer *ren)
{
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

