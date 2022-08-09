/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * 3D Model representation for the CPU.
 */

#ifndef NOTTE_MODEL_H
#define NOTTE_MODEL_H

#include <notte/error.h>
#include <notte/defs.h>
#include <notte/membuf.h>
#include <notte/math.h>
#include <notte/memory.h>
#include <notte/vector.h>
#include <notte/renderer.h>

/* Loads a wavefront .OBJ file as a static model. */
Err_Code StaticMeshLoadObj(Renderer *ren, Allocator alloc, 
    Static_Mesh **mesh, Parse_Result *result, Membuf buf);
Err_Code ConvertObjToUStatic(Allocator alloc, Membuf inBuf, Membuf *outBuf);
Err_Code StaticMeshLoadUStatic(Renderer *ren, Allocator alloc,
    Static_Mesh **mesh, Parse_Result *result, Membuf buf);

#endif /* NOTTE_MODEL_H */
