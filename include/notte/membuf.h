/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * General purpose immutable memory buffer.
 */

#ifndef NOTTE_MEMBUF_H
#define NOTTE_MEMBUF_H

#include <notte/error.h>
#include <notte/defs.h>

/* Big slab of immutable data. */
typedef struct
{
  const u8 *data;
  usize size;
} Membuf;

void MembufDestroy(Membuf *membuf);

Err_Code MembufLoadFile(Membuf *membuf, const char *file);

#endif /* NOTTE_MEMBUF_H */
