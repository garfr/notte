/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Vulkan renderer.
 */

#ifndef NOTTE_RENDERER_H
#define NOTTE_RENDERER_H

#include <notte/error.h>
#include <notte/plat.h>

typedef struct
{
  Plat_Window *win;
} Renderer_Create_Info;

typedef struct Renderer Renderer;

Err_Code RendererCreate(Renderer_Create_Info *create_info, Renderer **ren_out);
void RendererDestroy(Renderer *ren);

#endif /* NOTTE_RENDERER_H */
