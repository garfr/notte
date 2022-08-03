/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * String utilities.
 */

#include <string.h>

#include <notte/string.h>

String 
StringClone(Allocator alloc,
            String str)
{
  String new;

  new.len = str.len;
  u8 *buf = NEW_ARR(alloc, u8, str.len, MEMORY_TAG_STRING);
  MemoryCopy(buf, str.buf, str.len);
  new.buf = buf;
  return new;
}

String 
StringCloneSlice(Allocator alloc,
                 const u8 *buf, 
                 usize len)
{
  String new;
  if (len == 0)
  {
    len = strlen(buf);
  }

  new.len = len;
  u8 *newBuf = NEW_ARR(alloc, u8, len, MEMORY_TAG_STRING);
  MemoryCopy(newBuf, buf, len);
  new.buf = newBuf;
  return new;
}

String 
StringStealSlice(const u8 *buf, 
                 usize len)
{
  String new;
  if (len == 0)
  {
    len = strlen(buf);
  }

  new.len = len;
  new.buf = buf;
  return new;
}

bool 
StringEqual(String str1, 
    String str2)
{
  return str1.len == str2.len && strncmp(str1.buf, str2.buf, str1.len) == 0;
}

u32 
StringHash(String str)
{
  return str.len;
}

char *
StringConcatIntoCString(Allocator alloc, 
                        String s1, 
                        String s2,
                        usize *sizeOut)
{
  usize size = s1.len + s2.len;

  char *str = NEW_ARR(alloc, u8, size + 1, MEMORY_TAG_STRING);

  MemoryCopy(str, s1.buf, s1.len);
  MemoryCopy(str + s1.len, s2.buf, s2.len);
  str[size] = '\0';
  *sizeOut = size + 1;
  return str;
}

String 
StringConcat(Allocator alloc, 
             String s1, 
             String s2)
{
  usize size = s1.len + s2.len;

  u8 *buf = NEW_ARR(alloc, u8, size, MEMORY_TAG_STRING);

  MemoryCopy(buf, s1.buf, s1.len);
  MemoryCopy(buf+ s1.len, s2.buf, s2.len);
  String str = {
    .len = size,
    .buf = buf,
  };

  return str;
}

void 
StringDestroy(Allocator alloc, 
              String str)
{
  FREE_ARR(alloc, (u8 *) str.buf, u8, str.len, MEMORY_TAG_STRING);
}

