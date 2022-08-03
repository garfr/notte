/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Runtime polymorphic dictionary implementation.
 */

#ifndef NOTTE_DICT_H
#define NOTTE_DICT_H

#include <notte/memory.h>
#include <notte/defs.h>
#include <notte/string.h>

#define DICT_CREATE(_alloc, _type, _steal) DictCreate(_alloc, sizeof(_type),   \
    _steal)

typedef struct Dict Dict;
typedef void (*Dict_Destructor_Fn)(void *ud, String name, void *item);

Dict *DictCreate(Allocator alloc, usize itemSize, bool stealStings);

void *DictInsert(Dict *dict, String key, void *val);
void *DictInsertWithoutInit(Dict *dict, String key);
void *DictFind(Dict *dict, String key);

void DictDestroy(Dict *dict);
void DictDestroyWithDestructor(Dict *dict, void *ud, Dict_Destructor_Fn fn);

#endif /* NOTTE_DICT_H */
