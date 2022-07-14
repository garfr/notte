/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Memory management.
 */

#ifndef NOTTE_MEMORY_H
#define NOTTE_MEMORY_H

#include <notte/defs.h>

typedef enum
{
  MEMORY_TAG_UNKNOWN,
  MEMORY_TAG_ARRAY,
  MEMORY_TAG_MEMBUF,
  MEMORY_TAG_VECTOR,
  MEMORY_TAG_RENDERER,
  MEMORY_TAG_PLATFORM,
  MEMORY_TAG_TAG_COUNT,
} Memory_Tag;

void MemoryInit(void);
void MemoryDeinit(void);
void MemoryPrintUsage(void);

void *MemoryAlloc(usize size, Memory_Tag tag);
void *MemoryResize(void *ptr, usize old_size, usize new_size, Memory_Tag tag);
void MemoryFree(void *ptr, usize size, Memory_Tag tag);
void *MemoryZero(void *ptr, usize size);
void *MemoryCopy(void *dest, const void *src, usize size);
void *MemorySet(void *dest, u8 val, usize size);

#define MEMORY_NEW(_type, _tag) ((_type *) MemoryAlloc(sizeof(_type), (_tag)))
#define MEMORY_NEW_ARR(_type, _size, _tag) ((_type *) MemoryAlloc(             \
      (_size) * sizeof(_type), (_tag)))

#define MEMORY_FREE(_ptr, _type, _tag) MemoryFree((_ptr), sizeof(_type), (_tag))
#define MEMORY_FREE_ARR(_ptr, _type, _sz, _tag) MemoryFree((_ptr),             \
    sizeof(_type) * (_sz), (_tag))

#define MEMORY_RESIZE_ARR(_ptr, _type, _osz, _nsz, _tag) MemoryResize(_ptr,    \
    sizeof(_type) * (_osz), sizeof(_type) * (_nsz), _tag)

#endif /* NOTTE_MEMORY_H */
