/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Math support library.
 */

#ifndef NOTTE_MATH_H
#define NOTTE_MATH_H

#include <math.h>

#include <notte/defs.h>

#define MATH_PI 3.1415926535 

/* === TYPES === */

typedef struct
{
  f32 x, y;
} Vec2;

typedef struct
{
  f32 x, y, z;
} Vec3;

typedef struct
{
  f32 x, y, z, w;
} Vec4;

/* === INLINE FUNCTIONS === */

NOTTE_INLINE Vec2
Vec2Create(f32 x, 
           f32 y)
{
  Vec2 vec;
  vec.x = x;
  vec.y = y;
  return vec;
}

NOTTE_INLINE Vec2 
Vec2Zero(void)
{
  Vec2 vec;
  vec.x = vec.y = 0.0f;
  return vec;
}

NOTTE_INLINE Vec2 
Vec2Add(Vec2 a, Vec2 b)
{
  Vec2 vec;
  vec.x = a.x + b.x;
  vec.y = a.y + b.y;
  return vec;
}

NOTTE_INLINE Vec2 
Vec2Sub(Vec2 a, Vec2 b)
{
  Vec2 vec;
  vec.x = a.x - b.x;
  vec.y = a.y - b.y;
  return vec;
}

NOTTE_INLINE Vec2 
Vec2Mul(Vec2 a, Vec2 b)
{
  Vec2 vec;
  vec.x = a.x * b.x;
  vec.y = a.y * b.y;
  return vec;
}

NOTTE_INLINE Vec2 
Vec2Div(Vec2 a, Vec2 b)
{
  Vec2 vec;
  vec.x = a.x / b.x;
  vec.y = a.y / b.y;
  return vec;
}

NOTTE_INLINE f32
Vec2Length(Vec2 vec)
{
  return sqrtf(vec.x * vec.x + vec.y * vec.y);
}

NOTTE_INLINE bool
Vec2Equal(Vec2 v1, Vec2 v2)
{
  return v1.x == v2.x && v1.y == v2.y;
}

NOTTE_INLINE Vec3
Vec3Create(f32 x, 
           f32 y, 
           f32 z)
{
  Vec3 vec;
  vec.x = x;
  vec.y = y;
  vec.z = z;
  return vec;
}

NOTTE_INLINE Vec3
Vec3Zero(void)
{
  Vec3 vec;
  vec.x = vec.y = vec.z = 0.0f;
  return vec;
}

NOTTE_INLINE Vec3
Vec3Add(Vec3 a, Vec3 b)
{
  Vec3 vec;
  vec.x = a.x + b.x;
  vec.y = a.y + b.y;
  vec.z = a.z + b.z;
  return vec;
}

NOTTE_INLINE Vec3
Vec3Sub(Vec3 a, Vec3 b)
{
  Vec3 vec;
  vec.x = a.x - b.x;
  vec.y = a.y - b.y;
  vec.z = a.z - b.z;
  return vec;
}

NOTTE_INLINE Vec3
Vec3Mul(Vec3 a, Vec3 b)
{
  Vec3 vec;
  vec.x = a.x * b.x;
  vec.y = a.y * b.y;
  vec.z = a.z * b.z;
  return vec;
}

NOTTE_INLINE Vec3
Vec3Div(Vec3 a, Vec3 b)
{
  Vec3 vec;
  vec.x = a.x / b.x;
  vec.y = a.y / b.y;
  vec.z = a.z / b.z;
  return vec;
}

NOTTE_INLINE f32
Vec3Length(Vec3 vec)
{
  return sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
}

NOTTE_INLINE bool
Vec3Equal(Vec3 v1, Vec3 v2)
{
  return v1.x == v2.x && v1.y == v2.y && v1.z == v2.z;
}

NOTTE_INLINE Vec4
Vec4Create(f32 x, 
           f32 y,
           f32 z,
           f32 w)
{
  Vec4 vec;
  vec.x = x;
  vec.y = y;
  vec.z = z;
  vec.w = w;
  return vec;
}

NOTTE_INLINE Vec4
Vec4Zero(void)
{
  Vec4 vec;
  vec.x = vec.y = vec.z = vec.w = 0.0f;
  return vec;
}

NOTTE_INLINE Vec4
Vec4Add(Vec4 a, Vec4 b)
{
  Vec4 vec;
  vec.x = a.x + b.x;
  vec.y = a.y + b.y;
  vec.z = a.z + b.z;
  vec.w = a.w + b.w;
  return vec;
}

NOTTE_INLINE Vec4
Vec4Sub(Vec4 a, Vec4 b)
{
  Vec4 vec;
  vec.x = a.x - b.x;
  vec.y = a.y - b.y;
  vec.z = a.z - b.z;
  vec.w = a.w - b.w;
  return vec;
}

NOTTE_INLINE Vec4
Vec4Mul(Vec4 a, Vec4 b)
{
  Vec4 vec;
  vec.x = a.x * b.x;
  vec.y = a.y * b.y;
  vec.z = a.z * b.z;
  vec.w = a.w * b.w;
  return vec;
}

NOTTE_INLINE Vec4
Vec4Div(Vec4 a, Vec4 b)
{
  Vec4 vec;
  vec.x = a.x / b.x;
  vec.y = a.y / b.y;
  vec.z = a.z / b.z;
  vec.w = a.w / b.w;
  return vec;
}

NOTTE_INLINE f32
Vec4Length(Vec4 vec)
{
  return sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z + vec.w * vec.w);
}

#endif /* NOTTE_MATH_H */
