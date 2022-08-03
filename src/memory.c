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
  [MEMORY_TAG_STRING]   = "STRING  ",
  [MEMORY_TAG_MEMBUF]   = "MEMBUF  ",
  [MEMORY_TAG_STRING]   = "STRING  ",
  [MEMORY_TAG_VECTOR]   = "VECTOR  ",
  [MEMORY_TAG_PLATFORM] = "PLATFORM",
  [MEMORY_TAG_DICT]     = "DICT    ",
  [MEMORY_TAG_RENDERER] = "RENDERER",
  [MEMORY_TAG_ALLOC]    = "ALLOC   ",
  [MEMORY_TAG_BSON]     = "BSON    ",
  [MEMORY_TAG_FS]       = "FS      ",
};

Allocator libcAllocator;
Allocator_Logic libcAllocatorLogic;

/* === PROTOTYPES === */

static void *LibcNew(void *ud, usize sz, Memory_Tag tag);
static void *LibcResize(void *ud, void *ptr, usize osz, usize nsz, 
    Memory_Tag tag);
static void LibcFree(void *ud, void *ptr, usize sz, Memory_Tag tag);

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

  libcAllocatorLogic.new = LibcNew;
  libcAllocatorLogic.resize = LibcResize;
  libcAllocatorLogic.free = LibcFree;
  libcAllocator.logic = &libcAllocatorLogic;
  libcAllocator.ud = NULL;
}

void 
MemoryDeinit(void)
{
  return;
}

Allocator 
MemoryLoadLibcAllocator(void)
{
  return libcAllocator;
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

static void *
LibcNew(void *ud, usize sz, Memory_Tag tag)
{
  (void) ud;
  if (tag == MEMORY_TAG_UNKNOWN)
  {
    LOG_WARN("memory allocated with MEMORY_TAG_UNKNOWN");
  }

  memoryState.totalAllocations++;
  memoryState.taggedAllocations[tag]++;
  memoryState.totalAllocated += sz;
  memoryState.taggedAllocated[tag] += sz;

  void *block = calloc(1, sz);
  if (block == NULL)
  {
    LOG_FATAL("out of memory");
    exit(EXIT_FAILURE);
  }
  return block;
}

static void *
LibcResize(void *ud, 
           void *ptr, 
           usize osz, 
           usize nsz, 
           Memory_Tag tag)
{
  (void) ud;
  if (tag == MEMORY_TAG_UNKNOWN)
  {
    LOG_WARN("memory allocated with MEMORY_TAG_UNKNOWN");
  }

  if (osz > nsz)
  {
    memoryState.totalAllocated -= (osz - nsz);
    memoryState.taggedAllocated[tag] -= (osz - nsz);
  } else
  {
    memoryState.totalAllocated += (nsz - osz);
    memoryState.taggedAllocated[tag] += (nsz - osz);
  }

  void *block = realloc(ptr, nsz);
  if (block == NULL)
  {
    LOG_FATAL("out of memory");
    exit(EXIT_FAILURE);
  }

  return block;
}

volatile int thing = 0;
static void 
LibcFree(void *ud, 
         void *ptr, 
         usize sz, Memory_Tag tag)
{
  (void) ud;
  if (tag == MEMORY_TAG_UNKNOWN)
  {
    LOG_WARN("memory allocated with MEMORY_TAG_UNKNOWN");
  }

  if (memoryState.totalAllocations == 0 
   || memoryState.totalAllocated < sz
   || memoryState.taggedAllocations[tag]== 0 
   || memoryState.taggedAllocated[tag] < sz)
  {
    LOG_WARN("use after free");
  } else
  {
    memoryState.totalAllocations--;
    memoryState.taggedAllocations[tag]--;
    memoryState.totalAllocated -= sz;
    memoryState.taggedAllocated[tag] -= sz;
  }

  free(ptr);
}
