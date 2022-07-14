/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Vulkan renderer.
 */

#ifndef NOTTE_VECTOR_H
#define NOTTE_VECTOR_H

#include <notte/defs.h>
#include <notte/memory.h>

/* === TYPES === */

typedef struct
{
  usize elemSize, elemsUsed, elemsAlloc;
  u8 *buf;
} Vector;

/* === MACROS === */

#define VECTOR_CREATE(_type) VectorCreate(sizeof(_type))
#define VECTOR_DEFAULT_ELEMS_ALLOC 8

/* === INLINE FUNCTIONS === */

NOTTE_INLINE Vector 
VectorCreate(usize elemSize)
{
  Vector vec = {
    .elemSize = elemSize,
    .elemsUsed = 0,
    .elemsAlloc = VECTOR_DEFAULT_ELEMS_ALLOC,
  };

  vec.buf = MEMORY_NEW_ARR(u8, elemSize * VECTOR_DEFAULT_ELEMS_ALLOC, 
      MEMORY_TAG_VECTOR);

  return vec;
}

NOTTE_INLINE void *
VectorPush(Vector *vec, void *item)
{
  if (vec->elemsUsed + 1 > vec->elemsAlloc)
  {
    vec->buf = MEMORY_RESIZE_ARR(vec->buf, u8, 
        vec->elemSize * vec->elemsAlloc, 
        vec->elemSize * vec->elemsAlloc * 2, MEMORY_TAG_VECTOR);
    vec->elemsAlloc *= 2;
  }

  void *ptr = vec->buf + (vec->elemsUsed++ * vec->elemSize);
  MemoryCopy(ptr, item, vec->elemSize);
  return ptr;
}

NOTTE_INLINE void
VectorDestroy(Vector *vec)
{
  MEMORY_FREE_ARR(vec, u8, vec->elemSize * vec->elemsAlloc, MEMORY_TAG_VECTOR);
  vec->buf = NULL;
  vec->elemSize = vec->elemsUsed = vec->elemsAlloc = 0;
}

#endif /* NOTTE_VECTOR_H */
