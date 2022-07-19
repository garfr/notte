/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Abstractions over platform specifics.
 */

#ifndef NOTTE_PLAT_H
#define NOTTE_PLAT_H

#include <vulkan/vulkan.h>

#include <notte/defs.h>
#include <notte/error.h>
#include <notte/memory.h>

typedef struct Plat_Window Plat_Window;

typedef struct
{
  u32 w, h;
  Allocator alloc;
} Plat_Window_Create_Info;

typedef enum
{
  PLAT_EVENT_CLOSE,
} Plat_Event_Type;

typedef struct
{
  Plat_Event_Type t;
  union
  {
    Plat_Window *win;
  };
} Plat_Event;

Err_Code PlatInit(void);

Err_Code PlatWindowCreate(Plat_Window_Create_Info *info, 
    Plat_Window **winOut);
void PlatWindowDestroy(Plat_Window *win);
void PlatWindowPumpEvents(Plat_Window *win);
bool PlatWindowGetEvent(Plat_Window *win, Plat_Event *event);
bool PlatWindowShouldClose(Plat_Window *win);
Err_Code PlatWindowGetInstanceExtensions(Plat_Window *win, u32 *nNames, 
    const char **names);
Err_Code PlatWindowCreateVulkanSurface(Plat_Window *win, VkInstance vk,
    VkAllocationCallbacks *alloc, VkSurfaceKHR *surfaceOut);
void PlatWindowGetFramebufferSize(Plat_Window *win, u32 *w, u32 *h);

#endif /* NOTTE_PLAT_H */
