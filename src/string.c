/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * String utilities.
 */

#include <string.h>

#include <notte/string.h>

String 
StringClone(String str)
{
  String new;

  new.len = str.len;
  new.buf = MEMORY_NEW_ARR(u8, str.len, MEMORY_TAG_STRING);
  MemoryCopy(new.buf, str.buf, str.len);
  return new;
}

String 
StringCloneSlice(const u8 *buf, 
                 usize len)
{
  String new;
  if (len == 0)
  {
    len = strlen(buf);
  }

  new.len = len;
  new.buf = MEMORY_NEW_ARR(u8, len, MEMORY_TAG_STRING);
  MemoryCopy(new.buf, buf, len);
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
