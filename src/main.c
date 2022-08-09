/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Main entry point to the game.
 */

#include <stdio.h>
#include <stdlib.h>

#include <notte/log.h>
#include <notte/model.h>
#include <notte/memory.h>
#include <notte/math.h>
#include <notte/plat.h>
#include <notte/renderer.h>
#include <notte/bson.h>
#include <notte/fs.h>

/* === GLOBALS === */

Static_Vert verts[] = {
  {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
  {{0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
  {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
  {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
};

u32 indices[] = 
{
  0, 1, 2, 2, 3, 0
};

/* === PUBLIC FUNCTIONS === */

int
main()
{
  Err_Code err;
  Plat_Window *win;
  Plat_Event ev;
  Renderer *ren;
  Bson_Ast *ast;
  Membuf bsonBuf;
  Parse_Result result;
  Fs_Driver fs;
  Static_Mesh *bunny;

  LogSetLevel(LOG_LEVEL_DEBUG);

  LOG_DEBUG("entering main function");

  MemoryInit();

  Allocator libcAlloc = MemoryLoadLibcAllocator();

  err = PlatInit();
  if (err)
  {
    LOG_FATAL_CODE("failed to init platform window", err);
    return EXIT_FAILURE;
  }
  
  err = FsDiskDriverCreate(&fs, libcAlloc, STRING_CSTR("../"));
  if (err)
  {
    LOG_FATAL_CODE("failed to init Fs_Driver", err);
    return EXIT_FAILURE;
  }

  Plat_Window_Create_Info createInfo =
  {
    .w = 100,
    .h = 100,
    .alloc = libcAlloc,
  };

  err = PlatWindowCreate(&createInfo, &win);
  if (err)
  {
    LOG_FATAL_CODE("failed to create platform window", err);
    return EXIT_FAILURE;
  }

  Renderer_Create_Info rendererCreateInfo =
  {
    .win = win,
    .alloc = libcAlloc,
    .fs = &fs,
  };

  err = RendererCreate(&rendererCreateInfo, &ren);
  if (err)
  {
    LOG_FATAL_CODE("failed to create renderer", err);
    return EXIT_FAILURE;
  }

  f64 startTime = PlatGetTime();
  Membuf modelBuf;
  err = FsFileLoad(&fs, STRING_CSTR("assets/bunny.ustatic"), &modelBuf);
  if (err)
  {
    LOG_FATAL_CODE("failed to load model buffer", err);
    return EXIT_FAILURE;
  }

  Parse_Result meshResult;
  err = StaticMeshLoadUStatic(ren, libcAlloc, &bunny, &meshResult, modelBuf);
  if (err)
  {
    LOG_FATAL_CODE("failed to load bunny model", err);
    return EXIT_FAILURE;
  }
  f64 endTime = PlatGetTime();
  LOG_DEBUG_FMT("Loaded model in %f", endTime - startTime);

  FsFileDestroy(&fs, &modelBuf);

  Camera *cam;
  err = RendererCreateCamera(ren, &cam);
  if (err)
  {
    LOG_FATAL_CODE("failed to create camera", err);
    return EXIT_FAILURE;
  }

  LOG_DEBUG("created camera");

  Transform camTrans = 
  {
    .pos = {2.0, 0.0, 2.0},
  };
  RendererSetCameraTransform(ren, cam, camTrans);
  RendererSetCameraActive(ren, cam);

  f64 baseTime = PlatGetTime();

  Transform trans = 
  {
    .pos = {0.0f, 0.0f, 0.0f},
    .rot = {0.0f, 0.0f, 0.0f},
  };

  Material *mat = RendererLookupMaterial(ren, STRING_CSTR("tri"));

  while (1)
  {
    f64 nowTime = PlatGetTime();
    f64 delta = nowTime - baseTime;
    baseTime = nowTime;
    PlatWindowPumpEvents(win);
    while (PlatWindowGetEvent(win, &ev))
    {
      switch (ev.t)
      {
        case PLAT_EVENT_CLOSE:
          goto close;
      }
    }
    trans.rot[0] = 45 * sin(nowTime);
    trans.rot[1] = 30 + 90 * sin(nowTime / 2.0f);
    trans.rot[2] = 45 + 30 * sin(nowTime * 2.0f);
    RendererDrawStaticMesh(ren, bunny, trans, mat);
    err = RendererDraw(ren);
    if (err)
    {
      LOG_FATAL_CODE("failed to draw", err);
      return EXIT_FAILURE;
    }
    f64 afterTime = PlatGetTime();
    LOG_DEBUG_FMT("Drew frame in %f", afterTime - nowTime);

  }

close:

  RendererDestroyCamera(ren, cam);
  RendererDestroyStaticMesh(ren, bunny);
  PlatWindowDestroy(win);
  RendererDestroy(ren);
  FsDriverDestroy(&fs);

  MemoryPrintUsage();
  MemoryDeinit();

  LOG_DEBUG("exiting main function successfully");
  return EXIT_SUCCESS;
}
