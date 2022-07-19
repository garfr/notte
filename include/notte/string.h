/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * String utilities.
 */

#ifndef NOTTE_STRING_H
#define NOTTE_STRING_H

#include <notte/defs.h>

typedef struct
{
  usize len;
  u8 *buf;
} String;

String StringClone(String str);
String StringCloneSlice(const u8 *buf, usize len);
String StringStealSlice(const u8 *buf, usize len);
bool StringEqual(String str1, String str2);

#endif /* NOTTE_STRING_H */
