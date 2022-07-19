/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Linear chunk-based memory allocator.
 */

#ifndef NOTTE_LINEAR_ALLOCATOR_H
#define NOTTE_LINEAR_ALLOCATOR_H

#include <notte/defs.h>
#include <notte/error.h>
#include <notte/memory.h>

typedef struct Linear_Allocator_Chunk
{
  usize alloc, used;
  u8 *data;
  struct Linear_Allocator_Chunk *next;
} Linear_Allocator_Chunk;

typedef struct
{
  Linear_Allocator_Chunk *start;
  Linear_Allocator_Chunk *cur;
  Allocator alloc;
} Linear_Allocator;

Err_Code LinearAllocatorInit(Linear_Allocator *out, Allocator alloc);
void LinearAllocatorDeinit(Linear_Allocator *lin);

void *LinearAllocatorAlloc(Linear_Allocator *lin, usize data);

#endif /* NOTTE_LINEAR_ALLOCATOR_H */
