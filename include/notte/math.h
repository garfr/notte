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

typedef f32 Vec2[2];
typedef f32 Vec3[3];
typedef f32 Vec4[4];

typedef Vec4 Mat4[4];

/* == CONSTANTS === */

static Mat4 MAT4_EMPTY = {
  {0.0f, 0.0f, 0.0f, 0.0f},
  {0.0f, 0.0f, 0.0f, 0.0f},
  {0.0f, 0.0f, 0.0f, 0.0f},
  {0.0f, 0.0f, 0.0f, 0.0f}
};

static Mat4 MAT4_IDENTITY = {
  {1.0f, 0.0f, 0.0f, 0.0f},
  {0.0f, 1.0f, 0.0f, 0.0f},
  {0.0f, 0.0f, 1.0f, 0.0f},
  {0.0f, 0.0f, 0.0f, 1.0f}
};

/* === INLINE FUNCTIONS === */

NOTTE_INLINE f32
DegToRad(f32 deg)
{
  return (deg / 180.0f) * MATH_PI;
}

NOTTE_INLINE f32
RadToDeg(f32 deg)
{
  return (deg / MATH_PI) * 180.0f;
}

NOTTE_INLINE void
Vec2Create(f32 x, 
           f32 y, 
           Vec2 out)
{
  out[0] = x;
  out[1] = y;
}

NOTTE_INLINE void
Vec2Copy(Vec2 src, 
         Vec2 dst)
{
  dst[0] = src[0];
  dst[1] = src[1];
}

NOTTE_INLINE bool
Vec2Equal(Vec2 v1, 
          Vec2 v2)
{
  return v1[0] == v2[0] && 
         v1[1] == v2[1];
}

NOTTE_INLINE void
Vec3Create(f32 x, 
           f32 y, 
           f32 z, 
           Vec3 out)
{
  out[0] = x;
  out[1] = y;
  out[2] = y;
}

NOTTE_INLINE void
Vec3Copy(Vec3 src, 
         Vec3 dst)
{
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
}

NOTTE_INLINE bool
Vec3Equal(Vec3 v1, 
          Vec3 v2)
{
  return v1[0] == v2[0] && 
         v1[1] == v2[1] && 
         v1[2] == v2[2];
}

NOTTE_INLINE void
Vec3Scale(Vec3 in, 
          f32 val, 
          Vec3 out)
{
  out[0] = in[0] * val;
  out[1] = in[1] * val;
  out[2] = in[2] * val;
}

NOTTE_INLINE f32
Vec3Length(Vec3 v)
{
  return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

NOTTE_INLINE f32
Vec3Dot(Vec3 a, 
        Vec3 b)
{
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

NOTTE_INLINE f32
Vec3Norm2(Vec3 v)
{
  return Vec3Dot(v, v);
}

NOTTE_INLINE f32
Vec3Norm(Vec3 v)
{
  return sqrtf(Vec3Norm2(v));
}

NOTTE_INLINE void
Vec3Normalize(Vec3 in, 
              Vec3 out)
{
  float norm = Vec3Norm(in);

  if (norm == 0.0f)
  {
    out[0] = out[1] = out[2] = 0.0f;
    return;
  }

  Vec3Scale(in, 1.0f / norm, out);
}

NOTTE_INLINE void
Vec3Sub(Vec3 a, 
        Vec3 b, 
        Vec3 out)
{
  out[0] = a[0] - b[0];
  out[1] = a[1] - b[1];
  out[2] = a[2] - b[2];
}

NOTTE_INLINE void
Vec3Cross(Vec3 a, 
          Vec3 b, 
          Vec3 out)
{
  Vec3 c;
  c[0] = a[1] * b[2] - a[2] * b[1];
  c[1] = a[2] * b[0] - a[0] * b[2];
  c[2] = a[0] * b[1] - a[1] * b[0];
  Vec3Copy(c, out);
}

NOTTE_INLINE void
Vec3Crossn(Vec3 a, 
           Vec3 b, 
           Vec3 out)
{
  Vec3Cross(a, b, out);
  Vec3Normalize(out, out);
}

NOTTE_INLINE void
Vec4MulAdds(Vec4 in, 
            f32 val, 
            Vec4 out)
{
  out[0] += in[0] * val;
  out[1] += in[1] * val;
  out[2] += in[2] * val;
  out[3] += in[3] * val;
}

NOTTE_INLINE void
Mat4Translate(Mat4 in, 
              Vec3 v, 
              Mat4 out)
{
  Vec4MulAdds(in[0], v[0], out[3]);
  Vec4MulAdds(in[1], v[1], out[3]);
  Vec4MulAdds(in[2], v[2], out[3]);
}

NOTTE_INLINE void
Mat4Copy(Mat4 src, 
         Mat4 dst)
{
  MemoryCopy(dst, src, sizeof(Mat4));
}

NOTTE_INLINE void
Mat4Identity(Mat4 out)
{
  Mat4Copy(MAT4_IDENTITY, out);
}

NOTTE_INLINE void
Mat4Empty(Mat4 out)
{
  Mat4Copy(MAT4_EMPTY, out);
}

NOTTE_INLINE void
Mat4RotateMake(f32 rad, 
               Vec3 axis, 
               Mat4 out)
{
  Vec3 axisn, v, vs;
  float c;
  Mat4Empty(out);

  c = cosf(rad);

  Vec3Normalize(axis, axisn);
  Vec3Scale(axisn, 1.0f - c, v);
  Vec3Scale(axisn, sinf(rad), vs);

  Vec3Scale(axisn, v[0], out[0]);
  Vec3Scale(axisn, v[1], out[1]);
  Vec3Scale(axisn, v[2], out[2]);

  out[0][0] += c;     out[1][0] -= vs[2]; out[2][0] += vs[1];
  out[0][1] += vs[2]; out[1][1] += c;     out[2][1] -= vs[0];
  out[0][2] -= vs[1]; out[1][2] += vs[0]; out[2][2] += c;

  out[0][3] = out[1][3] = out[2][3] = out[3][0] = out[3][1] = out[3][2] = 0.0f;
  out[3][3] = 1.0f;
}

NOTTE_INLINE void
Mat4MulRot(Mat4 in, 
           Mat4 rot, 
           Mat4 out)
{
  float a00 = in[0][0], a01 = in[0][1], a02 = in[0][2], a03 = in[0][3],
        a10 = in[1][0], a11 = in[1][1], a12 = in[1][2], a13 = in[1][3],
        a20 = in[2][0], a21 = in[2][1], a22 = in[2][2], a23 = in[2][3],
        a30 = in[3][0], a31 = in[3][1], a32 = in[3][2], a33 = in[3][3],

        b00 = rot[0][0], b01 = rot[0][1], b02 = rot[0][2],
        b10 = rot[1][0], b11 = rot[1][1], b12 = rot[1][2],
        b20 = rot[2][0], b21 = rot[2][1], b22 = rot[2][2];

  out[0][0] = a00 * b00 + a10 * b01 + a20 * b02;
  out[0][1] = a01 * b00 + a11 * b01 + a21 * b02;
  out[0][2] = a02 * b00 + a12 * b01 + a22 * b02;
  out[0][3] = a03 * b00 + a13 * b01 + a23 * b02;

  out[1][0] = a00 * b10 + a10 * b11 + a20 * b12;
  out[1][1] = a01 * b10 + a11 * b11 + a21 * b12;
  out[1][2] = a02 * b10 + a12 * b11 + a22 * b12;
  out[1][3] = a03 * b10 + a13 * b11 + a23 * b12;

  out[2][0] = a00 * b20 + a10 * b21 + a20 * b22;
  out[2][1] = a01 * b20 + a11 * b21 + a21 * b22;
  out[2][2] = a02 * b20 + a12 * b21 + a22 * b22;
  out[2][3] = a03 * b20 + a13 * b21 + a23 * b22;

  out[3][0] = a30;
  out[3][1] = a31;
  out[3][2] = a32;
  out[3][3] = a33;
}

NOTTE_INLINE void
Mat4Rotate(Mat4 in, 
           f32 rad, 
           Vec3 axis, 
           Mat4 out)
{
  Mat4 rot;
  Mat4RotateMake(rad, axis, rot);
  Mat4MulRot(in, rot, out);
}

NOTTE_INLINE void
Mat4Lookat(Vec3 eye, 
           Vec3 center, 
           Vec3 up, 
           Mat4 out)
{
  Vec3 f, u, s;

  Vec3Sub(center, eye, f);
  Vec3Normalize(f, f);

  Vec3Crossn(f, up, s);
  Vec3Cross(s, f, u);

  out[0][0] = s[0];
  out[0][1] = u[0];
  out[0][2] = -f[0];
  out[1][0] = s[1];
  out[1][1] = u[1];
  out[1][2] = -f[1];
  out[2][0] = s[2];
  out[2][1] = u[2];
  out[2][2] = -f[2];
  out[3][0] = -Vec3Dot(s, eye);
  out[3][1] = -Vec3Dot(u, eye);
  out[3][2] =  Vec3Dot(f, eye);
  out[0][3] = out[1][3] = out[2][3] = 0.0f;
  out[3][3] = 1.0f;
}

NOTTE_INLINE void
Mat4Perspective(float fovy, 
                float aspect, 
                float nearZ, 
                float farZ, 
                Mat4 out)
{
  float f, fn;

  Mat4Empty(out);

  f = 1.0f / tanf(fovy * 0.5f);
  fn = 1.0f / (nearZ - farZ);

  out[0][0] = f / aspect;
  out[1][1] = f;
  out[2][2] = farZ * fn;
  out[2][3] = -1.0f;
  out[3][2] = nearZ * farZ * fn;
}

#endif /* NOTTE_MATH_H */
