/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Vulkan memory allocator.
 */

#ifndef NOTTE_VK_MEM_H
#define NOTTE_VK_MEM_H

#include <notte/renderer_priv.h>

Err_Code CreateBuffer(Renderer *ren, VkDeviceSize size, 
    VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, 
    VkBuffer *buffer, VkDeviceMemory *bufferMemory);
void DestroyBuffer(Renderer *ren, VkBuffer buffer, 
    VkDeviceMemory memory);

#endif  /* NOTTE_VK_MEM_H */
