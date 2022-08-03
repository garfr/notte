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
  MEMORY_TAG_STRING,
  MEMORY_TAG_MEMBUF,
  MEMORY_TAG_VECTOR,
  MEMORY_TAG_RENDERER,
  MEMORY_TAG_PLATFORM,
  MEMORY_TAG_DICT,
  MEMORY_TAG_BSON,
  MEMORY_TAG_FS,
  MEMORY_TAG_ALLOC, /* For allocators. */
  MEMORY_TAG_TAG_COUNT,
} Memory_Tag;

typedef void *(*Allocator_Alloc_Fn)(void *ud, usize sz, Memory_Tag tag);
typedef void *(*Allocator_Resize_Fn)(void *ud, void *ptr, usize osz, 
    usize nsz, Memory_Tag tag);
typedef void (*Allocator_Free_Fn)(void *ud, void *ptr, usize sz, Memory_Tag tag);

typedef struct
{
  bool avoidOOM;
  Allocator_Alloc_Fn new;
  Allocator_Resize_Fn resize;
  Allocator_Free_Fn free;
} Allocator_Logic;

typedef struct
{
  Allocator_Logic *logic;
  void *ud;
} Allocator;

void MemoryInit(void);
void MemoryDeinit(void);
Allocator MemoryLoadLibcAllocator(void);
void MemoryPrintUsage(void);

void *MemoryZero(void *ptr, usize size);
void *MemoryCopy(void *dest, const void *src, usize size);
void *MemorySet(void *dest, u8 val, usize size);

#define NEW(_alloc, _type, _tag) ((_type *)                                   \
    (_alloc).logic->new((_alloc).ud, sizeof(_type), _tag))
#define NEW_ARR(_alloc, _type, _sz, _tag) ((_type *)                          \
    (_alloc).logic->new((_alloc).ud, (_sz) * sizeof(_type), _tag))

#define FREE(_alloc, _ptr, _type, _tag) (_alloc).logic->free((_alloc).ud,     \
    (_ptr), sizeof(_type), (_tag))
#define FREE_ARR(_alloc, _ptr, _type, _len, _tag) (_alloc).logic->free(       \
    (_alloc).ud, (_ptr), (_len) * sizeof(_type), (_tag))

#define RESIZE_ARR(_alloc, _ptr, _type, _osz, _nsz, _tag)                     \
  ((_type *) (_alloc).logic->resize((_alloc).ud, (_ptr),                      \
    sizeof(_type) * (_osz), sizeof(_type) * (_nsz), _tag))                     

#endif /* NOTTE_MEMORY_H */
