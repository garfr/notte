/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Linear chunk-based memory allocator.
 */

#include <notte/linear_allocator.h>
#include <notte/memory.h>

/* === MACROS === */

#define DEFAULT_CHUNK_SIZE 4096

/* === PROTOTYPES === */

static Linear_Allocator_Chunk *CreateChunk(Linear_Allocator *lin, 
    usize minSize);
static void *LinearAllocatorAlloc(void *ud, usize data,
    Memory_Tag tag);
static void LinearAllocatorFree(void *ud, void *ptr, usize data,
    Memory_Tag tag);
static void *LinearAllocatorResize(void *ud, void *ptr, usize osz, 
    usize nsz, Memory_Tag tag);

/* === GLOBALS === */

Allocator_Logic linearAllocatorLogic =
{
  .new = LinearAllocatorAlloc,
  .free = LinearAllocatorFree,
  .resize = LinearAllocatorResize,
};

/* === PUBLIC FUNCTIONS === */

Err_Code 
LinearAllocatorInit(Linear_Allocator *out,
                    Allocator alloc)
{
  out->alloc = alloc;
  out->start = CreateChunk(out, DEFAULT_CHUNK_SIZE);

  out->cur = out->start;

  return ERR_OK;
}

void 
LinearAllocatorDeinit(Linear_Allocator *lin)
{
  Linear_Allocator_Chunk *iter = lin->start, *temp;

  while (iter != NULL)
  {
    temp = iter;
    iter = iter->next;

    FREE_ARR(lin->alloc, temp->data, u8, temp->alloc, MEMORY_TAG_ARRAY);
    FREE(lin->alloc, temp, Linear_Allocator_Chunk, MEMORY_TAG_ALLOC);
  }
}


Allocator 
LinearAllocatorWrap(Linear_Allocator *lin)
{
  Allocator alloc = {
    .logic = &linearAllocatorLogic,
    .ud = lin,
  };
  return alloc;
}

/* === PRIVATE FUNCTIONS === */

static Linear_Allocator_Chunk *
CreateChunk(Linear_Allocator *lin,
            usize minSize)
{
  Linear_Allocator_Chunk *chunk = NEW(lin->alloc, Linear_Allocator_Chunk, 
      MEMORY_TAG_ALLOC);

  size_t realSize = 1;
  while (realSize < minSize)
  {
    realSize *= 2;
  }

  chunk->alloc = realSize;
  chunk->used = 0;
  chunk->data = NEW_ARR(lin->alloc, u8, chunk->alloc, MEMORY_TAG_ARRAY);
  chunk->next = NULL;

  return chunk;
}

static void *
LinearAllocatorAlloc(void *ud,
                     usize data,
                     Memory_Tag tag)
{
  (void) tag;
  Linear_Allocator *lin = (Linear_Allocator *) ud;
  if (lin->cur->alloc - lin->cur->used < data)
  {
    Linear_Allocator_Chunk *new = CreateChunk(lin, data);
    lin->cur->next = new;
    lin->cur = new;
    u8 *buf = new->data;
    new->used = data;
    new->used += NOTTE_MAX_ALIGN - (new->used % NOTTE_MAX_ALIGN);
    return buf;
  }

  u8 *buf = lin->cur->data + lin->cur->used;
  lin->cur->used += data;
  lin->cur->used += NOTTE_MAX_ALIGN - (lin->cur->used % NOTTE_MAX_ALIGN);
  return buf;
}

static void 
LinearAllocatorFree(void *ud, 
                    void *ptr, 
                    usize data,
                    Memory_Tag tag)
{
  (void) ud;
  (void) ptr;
  (void) data;
  (void) tag;
  return;
}

static void *
LinearAllocatorResize(void *ud, 
                      void *ptr, 
                      usize osz, 
                      usize nsz, 
                      Memory_Tag tag)
{
  (void) tag;
  (void) osz;
  return LinearAllocatorAlloc(ud, nsz, tag);
}
