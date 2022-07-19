/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * 3D Model representation for the CPU.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

#include <notte/model.h>
#include <notte/log.h>
#include <notte/memory.h>

void StaticModelDestroy(Static_Model *model,
                        Allocator alloc)
{
  for (usize i = 0; i < model->nShapes; i++)
  {
    Static_Shape *shape = &model->shapes[i];
    FREE_ARR(alloc, shape->indices, u32, shape->nIndices, MEMORY_TAG_ARRAY);
    FREE_ARR(alloc, shape->verts, Static_Vert, shape->nVerts, MEMORY_TAG_ARRAY);
  }

  FREE_ARR(alloc, model->shapes, Static_Shape, model->nShapes, MEMORY_TAG_ARRAY);
}
