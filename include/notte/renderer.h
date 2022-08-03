/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Vulkan renderer.
 */

#ifndef NOTTE_RENDERER_H
#define NOTTE_RENDERER_H

#include <notte/error.h>
#include <notte/plat.h>
#include <notte/memory.h>
#include <notte/fs.h>

typedef struct
{
  Plat_Window *win;
  Allocator alloc;
  Fs_Driver *fs;
} Renderer_Create_Info;

typedef struct Renderer Renderer;

Err_Code RendererCreate(Renderer_Create_Info *create_info, Renderer **ren_out);
Err_Code RendererDraw(Renderer *ren);
void RendererDestroy(Renderer *ren);

#endif /* NOTTE_RENDERER_H */
