/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * String utilities.
 */

#ifndef NOTTE_STRING_H
#define NOTTE_STRING_H

#include <notte/defs.h>
#include <notte/memory.h>

#define STRING_CSTR(_cstr) StringStealSlice(_cstr, 0)

typedef struct
{
  usize len;
  const u8 *buf;
} String;

String StringClone(Allocator alloc, String str);
String StringCloneSlice(Allocator alloc, const u8 *buf, usize len);
String StringStealSlice(const u8 *buf, usize len);
bool StringEqual(String str1, String str2);
String StringConcat(Allocator alloc, String s1, String s2);
char *StringConcatIntoCString(Allocator alloc, String s1, String s2, 
    usize *sizeOut);
char *StringMakeCString(Allocator alloc, String str);
void StringDestroy(Allocator alloc, String str);

u32 StringHash(String str);

#endif /* NOTTE_STRING_H */
