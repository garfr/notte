/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Vulkan memory allocator.
 */

#include <notte/vk_mem.h>

/* === PRIVATE FUNCTIONS === */

static uint32_t FindMemoryType(Renderer *ren, uint32_t typeFilter, 
    VkMemoryPropertyFlags props);

/* === PUBLIC FUNCTIONS === */

Err_Code
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


void 
DestroyBuffer(Renderer *ren, 
              VkBuffer buffer, 
              VkDeviceMemory memory)
{
  vkDestroyBuffer(ren->dev, buffer, ren->allocCbs);
  vkFreeMemory(ren->dev, memory, ren->allocCbs);
}

/* === PRIVATE FUNCTIONS === */

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

