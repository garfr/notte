/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Memory management.
 */

#include <stdlib.h>
#include <stdio.h>

#include <notte/memory.h>
#include <notte/log.h>

/* === GLOBALS === */

struct
{
  usize totalAllocations;
  usize totalAllocated;
  usize taggedAllocations[MEMORY_TAG_TAG_COUNT];
  usize taggedAllocated[MEMORY_TAG_TAG_COUNT];
} memoryState;

static const char *
memoryTagToStr[] = {
  [MEMORY_TAG_UNKNOWN]  = "UNKNOWN ",
  [MEMORY_TAG_ARRAY]    = "ARRAY   ",
  [MEMORY_TAG_MEMBUF]   = "MEMBUF  ",
  [MEMORY_TAG_VECTOR]   = "VECTOR  ",
  [MEMORY_TAG_PLATFORM] = "PLATFORM",
  [MEMORY_TAG_RENDERER] = "RENDERER",
};

/* === PUBLIC FUNCTIONS === */

void 
MemoryInit(void)
{
  memoryState.totalAllocated = 0;
  memoryState.totalAllocations = 0;
  for (Memory_Tag tag = 0; tag < MEMORY_TAG_TAG_COUNT; tag++)
  {
    memoryState.taggedAllocations[tag] = 0;
    memoryState.taggedAllocated[tag] = 0;
  }
}

void 
MemoryDeinit(void)
{
  return;
}

void 
MemoryPrintUsage(void)
{
  printf("ALL      %zu unfreed bytes, %zu unfreed allocations\n", 
      memoryState.totalAllocated, memoryState.totalAllocations);
  for (Memory_Tag tag = 0; tag < MEMORY_TAG_TAG_COUNT; tag++)
  {
    printf("%s %zu unfreed bytes, %zu unfreed allocations\n", memoryTagToStr[tag],
        memoryState.taggedAllocated[tag], memoryState.taggedAllocations[tag]);
  }
}

void *
MemoryAlloc(usize size, 
            Memory_Tag tag)
{
  if (tag == MEMORY_TAG_UNKNOWN)
  {
    LOG_WARN("memory allocated with MEMORY_TAG_UNKNOWN");
  }

  memoryState.totalAllocations++;
  memoryState.taggedAllocations[tag]++;
  memoryState.totalAllocated += size;
  memoryState.taggedAllocated[tag] += size;

  void *block = calloc(1, size);
  if (block == NULL)
  {
    LOG_FATAL("out of memory");
    exit(EXIT_FAILURE);
  }
  return block;
}

void *
MemoryResize(void *ptr, 
             usize oldSize, 
             usize newSize, 
             Memory_Tag tag)
{
  if (tag == MEMORY_TAG_UNKNOWN)
  {
    LOG_WARN("memory allocated with MEMORY_TAG_UNKNOWN");
  }

  if (oldSize > newSize)
  {
    memoryState.totalAllocated -= (oldSize - newSize);
    memoryState.taggedAllocated[tag] -= (oldSize - newSize);
  } else
  {
    memoryState.totalAllocated += (newSize - oldSize);
    memoryState.taggedAllocated[tag] += (newSize - oldSize);
  }

  void *block = realloc(ptr, newSize);
  if (block == NULL)
  {
    LOG_FATAL("out of memory");
    exit(EXIT_FAILURE);
  }

  return block;
}

void 
MemoryFree(void *ptr, 
           usize size, 
           Memory_Tag tag)
{
  if (tag == MEMORY_TAG_UNKNOWN)
  {
    LOG_WARN("memory allocated with MEMORY_TAG_UNKNOWN");
  }

  memoryState.totalAllocations--;
  memoryState.taggedAllocations[tag]--;
  memoryState.totalAllocated -= size;
  memoryState.taggedAllocated[tag] -= size;;

  free(ptr);
}

void *
MemoryZero(void *ptr, 
           usize size)
{
  memset(ptr, 0, size);
  return ptr;
}

void *
MemoryCopy(void *dest, 
           const void *src, 
           usize size)
{
  memcpy(dest, src, size);
  return dest;
}

void *
MemorySet(void *dest, 
          u8 val, 
          usize size)
{
  memset(dest, val, size);
  return dest;
}
