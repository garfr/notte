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
#include <notte/math.h>

typedef struct
{
  Plat_Window *win;
  Allocator alloc;
  Fs_Driver *fs;
} Renderer_Create_Info;

typedef struct Static_Mesh Static_Mesh;

typedef struct
{
  Vec3 pos;
  Vec3 nor;
  Vec2 tex;
} Static_Vert;

typedef struct
{
  Vec3 pos;
  Vec3 rot;
} Transform;

/* The ownership of both verts and indices are taken. */
typedef struct
{
  const Static_Vert *verts;
  const u32 *indices;
  usize nVerts, nIndices;
} Static_Mesh_Create_Info;

typedef struct Renderer Renderer;

typedef struct Camera Camera;
typedef struct Material Material;

Err_Code RendererCreate(Renderer_Create_Info *create_info, Renderer **ren_out);
Err_Code RendererDraw(Renderer *ren);
void RendererDestroy(Renderer *ren);

Err_Code RendererCreateStaticMesh(Renderer *ren, 
    Static_Mesh_Create_Info *createInfo, Static_Mesh **mesh);
void RendererDestroyStaticMesh(Renderer *ren, Static_Mesh *mesh);

void RendererDrawStaticMesh(Renderer *ren, Static_Mesh *mesh, 
    Transform transform, Material *mat);

Err_Code RendererCreateCamera(Renderer *ren, Camera **cameraOut);
void RendererDestroyCamera(Renderer *ren, Camera *cam);
void RendererSetCameraActive(Renderer *ren, Camera *cam);
void RendererSetCameraTransform(Renderer *ren, Camera *cam, Transform trans);
void RendererSetCameraFov(Renderer *ren, Camera *cam, f32 fov);

Material *RendererLookupMaterial(Renderer *ren, String name);

#endif /* NOTTE_RENDERER_H */
