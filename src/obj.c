/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Loads Wavefront .OBJ files. 
 */

#include <stdarg.h>
#include <stdio.h>

#include <notte/model.h>
#include <notte/log.h>

/* === CONSTANTS === */

#define MAX_VERTS_PER_FACE 3

/* === TYPES === */

typedef struct
{
  int nothing;
} Parser;

typedef struct
{
  usize nPos, nNor, nTex, nVerts;
} Size_Data;

typedef struct 
{
  enum
  {
    OBJ_CMD_V,
    OBJ_CMD_VT,
    OBJ_CMD_VN,
    OBJ_CMD_F,
  } t;

  union
  {
    f32 v[3];
    struct
    {
      u32 pos[MAX_VERTS_PER_FACE];
      u32 nor[MAX_VERTS_PER_FACE];
      u32 tex[MAX_VERTS_PER_FACE];
      u32 verts;
    } indices;
  };
} Obj_Cmd;

/* === PROTOTYPES === */

static usize CountCmds(Membuf buf);
static Err_Code ParseCmds(Membuf buf, Parse_Result *result, Obj_Cmd *cmds, 
    Size_Data *size);
static f32 ReadFloat(usize *pos_out, Membuf buf);
static u32 ReadInt(usize *pos_out, Membuf buf);
static void FormatErr(Parse_Result *result, const char *fmt, ...);

/* === PUBLIC FUNCTIONS === */

static bool
StaticVertEqual(Static_Vert v1, Static_Vert v2)
{
  return Vec3Equal(v1.pos, v2.pos) && Vec3Equal(v1.nor, v2.nor) 
    && Vec2Equal(v1.tex, v2.tex);
}

Err_Code 
StaticModelLoadObj(Allocator alloc,
                   Static_Model *model, 
                   Parse_Result *result, 
                   Membuf buf)
{
  Err_Code err;
  Size_Data size;
  usize nCmds = CountCmds(buf);

  Obj_Cmd *cmds = NEW_ARR(alloc, Obj_Cmd, nCmds, MEMORY_TAG_ARRAY);

  err = ParseCmds(buf, result, cmds, &size);
  if (err)
  {
    FREE_ARR(alloc, cmds, Obj_Cmd, nCmds, MEMORY_TAG_ARRAY);
    return err;
  }

  Vec3 *pos = NEW_ARR(alloc, Vec3, size.nPos, MEMORY_TAG_ARRAY);
  Vec3 *nor = NEW_ARR(alloc, Vec3, size.nNor, MEMORY_TAG_ARRAY);
  Vec2 *tex = NEW_ARR(alloc, Vec2, size.nTex, MEMORY_TAG_ARRAY);
  Static_Vert *verts = NEW_ARR(alloc, Static_Vert, size.nVerts, 
      MEMORY_TAG_ARRAY);
  u32 *indices = NEW_ARR(alloc, u32, size.nVerts, MEMORY_TAG_ARRAY);
  usize posIdx = 0, norIdx = 0, texIdx = 0, vertIdx = 0, indicesIdx = 0;

  for (usize i = 0; i < nCmds; i++)
  {
    Obj_Cmd *cmd = &cmds[i];
    switch (cmd->t)
    {
      case OBJ_CMD_V:
        Vec3Create(cmd->v[0], cmd->v[1], cmd->v[2], pos[posIdx++]);
        break;
      case OBJ_CMD_VN:
        Vec3Create(cmd->v[0], cmd->v[1], cmd->v[2], nor[norIdx++]);
        break;
      case OBJ_CMD_VT:
        Vec2Create(cmd->v[0], cmd->v[1], tex[texIdx++]);
        break;
      case OBJ_CMD_F:
      {
        for (u32 i = 0; i < cmd->indices.verts; i++)
        {
          Static_Vert vert;
          Vec3Copy(pos[cmd->indices.pos[i] - 1], vert.pos);
          Vec3Copy(nor[cmd->indices.nor[i] - 1], vert.nor);
          Vec2Copy(tex[cmd->indices.tex[i] - 1], vert.tex);
          for (usize j = 0; j < vertIdx; j++)
          {
            if (StaticVertEqual(vert, verts[j]))
            {
              indices[indicesIdx++] = j;
              goto skip;
            }
          }
          verts[vertIdx] = vert;
          indices[indicesIdx++] = vertIdx++;
skip:
          continue;
        }
        break;
      }
    }
  }

  FREE_ARR(alloc, cmds, Obj_Cmd, nCmds, MEMORY_TAG_ARRAY);
  FREE_ARR(alloc, pos, Vec3, size.nPos, MEMORY_TAG_ARRAY);
  FREE_ARR(alloc, nor, Vec3, size.nNor, MEMORY_TAG_ARRAY);
  FREE_ARR(alloc, tex, Vec2, size.nTex, MEMORY_TAG_ARRAY);

  Static_Shape *shape = NEW_ARR(alloc, Static_Shape, 1, MEMORY_TAG_ARRAY);
  verts = RESIZE_ARR(alloc, verts, Static_Vert, size.nVerts, vertIdx, 
      MEMORY_TAG_ARRAY);
  shape->name = "obj";
  shape->indices = indices;
  shape->nIndices = size.nVerts;
  shape->verts = verts;
  shape->nVerts = vertIdx;

  model->nShapes = 1;
  model->shapes = shape;

  return ERR_OK;
}

/* === PRIVATE FUNCTIONS === */

static usize 
CountCmds(Membuf buf)
{
  usize count = 0;

  usize pos = 0;
  while (pos < buf.size)
  {
    if (buf.data[pos] != '#')
    {
      count++;
    }
    while (buf.data[pos] != '\n')
    {
      pos++;
    }
    pos++;
  }
  return count;
}

static u32 
ReadInt(usize *pos_out, 
        Membuf buf)
{
  usize pos = *pos_out;
  u32 total = 0;
  bool neg = false;

  while (isspace(buf.data[pos]))
  {
    pos++;
  }

  while (isdigit(buf.data[pos]))
  {
    total *= 10;
    total += buf.data[pos] - '0';
    pos++;
  }

  *pos_out = pos;
  return total;
}

static f32
ReadFloat(usize *pos_out, 
          Membuf buf)
{
  usize pos = *pos_out;
  float total = 0.0f;
  bool neg = false;

  while (isspace(buf.data[pos]))
  {
    pos++;
  }

  if (buf.data[pos] == '-')
  {
    neg = true;
    pos++;
  }

  while (isdigit(buf.data[pos]))
  {
    total *= 10.0f;
    total += (float) (buf.data[pos] - '0');
    pos++;
  }

  if (buf.data[pos] == '.')
  {
    pos++;
    float mult = 0.1f;
    while (isdigit(buf.data[pos]))
    {
      total += ((float) (buf.data[pos] - '0')) * mult;
      mult /= 10.0;
      pos++;
    }
  }

  if (neg)
  {
    total *= -1.0f;
  }

  *pos_out = pos;
  return total;
}

static Err_Code 
ParseCmds(Membuf buf, 
          Parse_Result *result, 
          Obj_Cmd *cmds,
          Size_Data *size)
{
  usize cmdPos = 0;
  usize pos = 0;
  size->nPos = size->nNor = size->nTex = size->nVerts = 0;

  while (pos < buf.size)
  {
    if (buf.data[pos] == '#')
    {
      while (buf.data[pos] != '\n')
      {
        pos++;
      }
      pos++;
      continue;
    }
    switch (buf.data[pos])
    {
      case 'v':
      {
        pos++;
        switch (buf.data[pos])
        {
          case ' ':
          {
            Obj_Cmd *cmd = &cmds[cmdPos++];
            cmd->t = OBJ_CMD_V;
            cmd->v[0] = ReadFloat(&pos, buf);
            cmd->v[1] = ReadFloat(&pos, buf);
            cmd->v[2] = ReadFloat(&pos, buf);
            size->nPos++;
            break;
          }
          case 'n':
          {
            pos++;
            Obj_Cmd *cmd = &cmds[cmdPos++];
            cmd->t = OBJ_CMD_VN;
            cmd->v[0] = ReadFloat(&pos, buf);
            cmd->v[1] = ReadFloat(&pos, buf);
            cmd->v[2] = ReadFloat(&pos, buf);
            size->nNor++;
            break;
          }
          case 't':
          {
            pos++;
            Obj_Cmd *cmd = &cmds[cmdPos++];
            cmd->t = OBJ_CMD_VT;
            cmd->v[0] = ReadFloat(&pos, buf);
            cmd->v[1] = ReadFloat(&pos, buf);
            size->nTex++;
            break;
          }
        }
        break;
      }
      case 'f':
      {
        Obj_Cmd *cmd = &cmds[cmdPos++];
        cmd->t = OBJ_CMD_F;
        usize vert = 0;
        pos++;
        pos++; /* Skip the space. */
        cmd->indices.verts = 0;

        while (buf.data[pos] != '\n')
        {
          cmd->indices.verts++;
          size->nVerts++;
          cmd->indices.pos[vert] = ReadInt(&pos, buf);
          pos++;
          cmd->indices.tex[vert] = ReadInt(&pos, buf);
          pos++;
          cmd->indices.nor[vert] = ReadInt(&pos, buf);
          vert++;
          pos++; /* Skip the space. */
        }

        break;
      }

      default:
        FormatErr(result, "unknown .obj command '%c'", buf.data[pos]);
        return ERR_FAILED_PARSE;
    }
    while (buf.data[pos] != '\n')
    {
      pos++;
    }
    pos++;
  }

  return ERR_OK;
}

static void 
FormatErr(Parse_Result *result, 
          const char *fmt, 
          ...)
{
  va_list args;
  va_start(args, fmt);

  vsnprintf(result->msg, PARSE_RESULT_MAX_MSG, fmt, args);
}
