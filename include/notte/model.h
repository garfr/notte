/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * 3D Model representation for the CPU.
 */

#ifndef NOTTE_MODEL_H
#define NOTTE_MODEL_H

#include <notte/error.h>
#include <notte/defs.h>
#include <notte/membuf.h>
#include <notte/math.h>
#include <notte/memory.h>

typedef struct
{
  Vec3 pos, nor;
  Vec2 tex;
} Static_Vert;

typedef struct
{
  usize nVerts;
  usize nIndices; 
  Static_Vert *verts;
  u32 *indices;
  const char *name;
} Static_Shape;

typedef struct
{
  Static_Shape *shapes;
  usize nShapes;
} Static_Model;

/* Loads a wavefront .OBJ file as a static model. */
Err_Code StaticModelLoadObj(Allocator alloc, Static_Model *model, 
    Parse_Result *result, Membuf buf);

void StaticModelDestroy(Static_Model *model, Allocator alloc);

#endif /* NOTTE_MODEL_H */
