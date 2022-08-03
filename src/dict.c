/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Runtime polymorphic dictionary implementation.
 */

#include <notte/dict.h>

/* === TYPES === */

typedef struct Dict_Entry
{
  String key;
  struct Dict_Entry *next;
  u8 val[];
} Dict_Entry;

struct Dict
{
  usize itemSize;
  Dict_Entry **buckets;
  usize nBuckets;
  bool stealStrings;
  Allocator alloc;
};

/* === MACROS === */

#define INIT_DICT_BUCKETS 8

/* === PUBLIC FUNCTIONS === */

Dict *
DictCreate(Allocator alloc, 
           usize itemSize, 
           bool stealStrings)
{
  Dict *dict = NEW(alloc, Dict, MEMORY_TAG_DICT);

  dict->itemSize = itemSize;
  dict->alloc = alloc;
  dict->stealStrings = stealStrings;

  dict->nBuckets = INIT_DICT_BUCKETS;
  dict->buckets = NEW_ARR(alloc, Dict_Entry *, INIT_DICT_BUCKETS, 
      MEMORY_TAG_DICT);

  for (usize i = 0; i < dict->nBuckets; i++)
  {
    dict->buckets[i] = NULL;
  }

  return dict;
}

void *
DictInsertWithoutInit(Dict *dict, 
                      String key)
{
  u32 idx = StringHash(key) % dict->nBuckets;

  Dict_Entry *iter = dict->buckets[idx];
  while (iter != NULL)
  {
    if (StringEqual(key, iter->key))
    {
      return NULL;
    }

    iter = iter->next;
  }

  Dict_Entry *new = (Dict_Entry *) NEW_ARR(dict->alloc, u8, 
      sizeof(Dict_Entry) + dict->itemSize, MEMORY_TAG_DICT);
  if (dict->stealStrings)
  {
    new->key = key;
  } else
  {
    new->key = StringClone(dict->alloc, key);
  }

  new->next = dict->buckets[idx];
  dict->buckets[idx] = new;
  return new->val;
}

void *
DictInsert(Dict *dict, 
           String key, 
           void *val)
{
  void *ptr = DictInsertWithoutInit(dict, key);
  if (ptr != NULL)
  {
    MemoryCopy(ptr, val, dict->itemSize);
  }
  return ptr;
}

void *
DictFind(Dict *dict, 
         String key)
{
  u32 idx = StringHash(key) % dict->nBuckets;

  Dict_Entry *iter = dict->buckets[idx];
  while (iter != NULL)
  {
    if (StringEqual(key, iter->key))
    {
      return iter->val;
    }

    iter = iter->next;
  }

  return NULL;
}

void 
DictDestroy(Dict *dict)
{
  DictDestroyWithDestructor(dict, NULL, NULL);
}

void 
DictDestroyWithDestructor(Dict *dict,   
                          void *ud, 
                          Dict_Destructor_Fn fn)
{
  for (usize i = 0; i < dict->nBuckets; i++)
  {
    Dict_Entry *iter = dict->buckets[i], *temp;
    while (iter != NULL)
    {
      temp = iter;
      iter = iter->next;
      if (fn != NULL)
      {
        fn(ud, temp->key, temp->val);
      }
      if (!dict->stealStrings)
      {
        StringDestroy(dict->alloc, temp->key);
      }
      FREE_ARR(dict->alloc, temp, u8, sizeof(Dict_Entry) + dict->itemSize, 
          MEMORY_TAG_DICT);
    }
  }

  FREE_ARR(dict->alloc, dict->buckets, Dict_Entry *, dict->nBuckets, 
      MEMORY_TAG_DICT);
  FREE(dict->alloc, dict, Dict, MEMORY_TAG_DICT);
}
