/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * General purpose macro and type definitions.
 */

#ifndef NOTTE_DEFS_H
#define NOTTE_DEFS_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <notte/log.h>

#define NOTTE_UNREACHABLE() LOG_FATAL("unreachable location reached");         \
  exit(EXIT_FAILURE)

#define NOTTE_INLINE inline

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

typedef size_t usize;

#endif /* NOTTE_DEFS_H */
