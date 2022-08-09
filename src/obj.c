/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Obj file loading.
 */

#include <notte/model.h>
#include <notte/error.h>
#include <notte/vector.h>

#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include <tinyobj_loader_c.h>

/* === TYPES === */

typedef struct
{
  Vector verts;
  Vector indices;
} Mesh_Data;

/* === PROTOTYPES === */

static Err_Code ObjLoadMeshData(Allocator alloc, Parse_Result *result, 
    Membuf inBuf, Mesh_Data *data);
static void GetFileData(void *ctx, const char *filename, const int is_mtl, 
    const char *obj_filename, char **data, size_t *len);
static Err_Code CreateStaticMeshFromData(Static_Mesh **mesh, Mesh_Data *data, 
    Renderer *ren, Allocator alloc);

/* === PUBLIC FUNCTION === */

Err_Code 
StaticMeshLoadObj(Renderer *ren, 
                  Allocator alloc, 
                  Static_Mesh **mesh, 
                  Parse_Result *result, 
                  Membuf buf)
{
  Err_Code err;
  Mesh_Data data;
  err = ObjLoadMeshData(alloc, result, buf, &data);
  if (err)
  {
    return err;
  }

  err = CreateStaticMeshFromData(mesh, &data, ren, alloc);
  if (err)
  {
    return err;
  }
  
  return ERR_OK;
}

static Err_Code
CreateStaticMeshFromData(Static_Mesh **mesh, 
                         Mesh_Data *data, 
                         Renderer *ren, 
                         Allocator alloc)
{
  Err_Code err;

  Static_Mesh_Create_Info createInfo = 
  {
    .verts = (Static_Vert *) data->verts.buf,
    .nVerts = data->verts.elemsUsed,
    .indices = (u32 *) data->indices.buf,
    .nIndices = data->indices.elemsUsed,
  };

  err = RendererCreateStaticMesh(ren, &createInfo, mesh);
  if (err)
  { 
    return err;
  }

  VectorDestroy(&data->verts, alloc);
  VectorDestroy(&data->indices, alloc);

  return ERR_OK;
}

Err_Code 
ConvertObjToUStatic(Allocator alloc, 
                    Membuf inBuf, 
                    Membuf *outBuf)
{
  Mesh_Data data;
  Parse_Result result;
  Err_Code err;

  err = ObjLoadMeshData(alloc, &result, inBuf, &data);
  if (err)
  {
    return err;
  }

  usize outSize = 2 * sizeof(u64) + data.verts.elemsUsed 
    * sizeof(Static_Vert) + data.indices.elemsUsed* sizeof(u32);
  u8 *buf = NEW_ARR(alloc, u8, outSize + 1, MEMORY_TAG_MEMBUF);
  usize idx = 0;

  MemoryCopy(idx + buf, &data.verts.elemsUsed, sizeof(u64));
  idx += sizeof(u64);
  MemoryCopy(idx + buf, &data.indices.elemsUsed, sizeof(u64));
  idx += sizeof(u64);

  MemoryCopy(idx + buf, data.verts.buf, data.verts.elemsUsed * sizeof(Static_Vert));
  idx += data.verts.elemsUsed * sizeof(Static_Vert);
  MemoryCopy(idx + buf, data.indices.buf, data.indices.elemsUsed * sizeof(u32));
  idx+= data.indices.elemsUsed * sizeof(u32);

  outBuf->data = buf;
  outBuf->size = outSize;

  VectorDestroy(&data.verts, alloc);
  VectorDestroy(&data.indices, alloc);

  return ERR_OK;
}

Err_Code 
StaticMeshLoadUStatic(Renderer *ren, 
                      Allocator alloc,
                      Static_Mesh **mesh, 
                      Parse_Result *result, 
                      Membuf buf)
{
  Err_Code err;

  const u8 *ptr = buf.data;

  u64 vertCount = *((u64*) ptr);
  ptr += sizeof(u64);
  u64 indexCount = *((u64*) ptr);
  ptr += sizeof(u64);

  Static_Vert *verts = NEW_ARR(alloc, Static_Vert, vertCount, MEMORY_TAG_ARRAY);
  u32 *indices = NEW_ARR(alloc, u32, indexCount, MEMORY_TAG_ARRAY);

  MemoryCopy(verts, ptr, sizeof(Static_Vert) * vertCount);
  ptr += sizeof(Static_Vert) * vertCount;
  MemoryCopy(indices, ptr, sizeof(u32) * indexCount);

  Static_Mesh_Create_Info createInfo = 
  {
    .verts = verts,
    .nVerts = vertCount,
    .indices = indices,
    .nIndices = indexCount, 
  };

  err = RendererCreateStaticMesh(ren, &createInfo, mesh);
  if (err)
  { 
    return err;
  }

  FREE_ARR(alloc, verts, Static_Vert, vertCount, MEMORY_TAG_ARRAY);
  FREE_ARR(alloc, indices, u32, indexCount, MEMORY_TAG_ARRAY);

  return ERR_OK;
}

/* === PRIVATE FUNCTION === */

static void 
GetFileData(void *ctx, 
            const char *filename,
            const int is_mtl, 
            const char *obj_filename, 
            char **data, 
            size_t *len)
{
  Membuf *buf = ctx;

  *data = (char *) buf->data;
  *len = buf->size;
}

static Err_Code 
ObjLoadMeshData(Allocator alloc, 
                Parse_Result *result, 
                Membuf inBuf, 
                Mesh_Data *data)
{
  usize numShapes, numMaterials;
  tinyobj_attrib_t attrib;
  tinyobj_shape_t *shapes = NULL;
  tinyobj_material_t *materials = NULL;
  Err_Code err;

  int ret = tinyobj_parse_obj(&attrib, &shapes, &numShapes, &materials,
      &numMaterials, "bunny2.obj", GetFileData, &inBuf, TINYOBJ_FLAG_TRIANGULATE);
  if (ret != TINYOBJ_SUCCESS)
  {
    return ERR_LIBRARY_FAILURE;
  }

  data->verts = VECTOR_CREATE(alloc, Static_Vert);
  data->indices = VECTOR_CREATE(alloc, u32);

  for (usize i = 0; i < attrib.num_faces; i++)
  {
    int vIdx = 3 * attrib.faces[i].v_idx;
    int nIdx = 3 * attrib.faces[i].vn_idx;
    Static_Vert vert = {
      {
        attrib.vertices[vIdx + 0],
        attrib.vertices[vIdx + 1],
        attrib.vertices[vIdx + 2],
      },
      {
        attrib.normals[nIdx + 0],
        attrib.normals[nIdx + 1],
        attrib.normals[nIdx + 2],
      },
      {
        0.0f, 0.0f,
      },
    };

    for (u32 j = 0; j < data->verts.elemsUsed; j++)
    {
      Static_Vert *tVert = VectorIdx(&data->verts, j);
      if (Vec3Equal(vert.pos, tVert->pos) &&
          Vec3Equal(vert.nor, tVert->nor) &&
          Vec2Equal(vert.tex, tVert->tex))
      {
        VectorPush(&data->indices, alloc, &j);
        goto skipCreate;
      }
    }
    u32 newIdx = data->verts.elemsUsed;
    VectorPush(&data->verts, alloc, &vert);
    VectorPush(&data->indices, alloc, &newIdx);
  skipCreate:
    ;
  }

  return ERR_OK;
}
