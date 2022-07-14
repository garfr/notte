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

void StaticModelDestroy(Static_Model *model)
{
  for (usize i = 0; i < model->nShapes; i++)
  {
    Static_Shape *shape = &model->shapes[i];
    MEMORY_FREE_ARR(shape->indices, u32, shape->nIndices, MEMORY_TAG_ARRAY);
    MEMORY_FREE_ARR(shape->verts, Static_Vert, shape->nVerts, MEMORY_TAG_ARRAY);
  }

  MEMORY_FREE_ARR(model->shapes, Static_Shape, model->nShapes, MEMORY_TAG_ARRAY);
}
