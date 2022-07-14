/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * General purpose immutable memory buffer.
 */

#include <stdio.h>
#include <stdlib.h>

#include <notte/membuf.h>
#include <notte/memory.h>

void 
MembufDestroy(Membuf *membuf)
{
  if (membuf->data != NULL)
  {
    MEMORY_FREE_ARR((u8 *) membuf->data, u8, membuf->size, MEMORY_TAG_MEMBUF);
  }
}

Err_Code 
MembufLoadFile(Membuf *membuf, const char *file)
{
  membuf->data = NULL;

  FILE *f = fopen(file, "rb");
  if (f == NULL)
  {
    return ERR_NO_FILE;
  }

  fseek(f, 0, SEEK_END);
  membuf->size = ftell(f);
  fseek(f, 0, SEEK_SET);

  u8 *data = MEMORY_NEW_ARR(u8, membuf->size + 1, MEMORY_TAG_MEMBUF);
  if (data == NULL)
  {
    return ERR_NO_MEM;
  }

  fread(data, 1, membuf->size, f);
  data[membuf->size] = '\0';
  membuf->data = data;

  fclose(f);

  return ERR_OK;
}
