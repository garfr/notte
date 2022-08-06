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

const Vertex verts[] =
{
  {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
  {{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
  {{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
  {{-0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}},
};

const u16 indices[] = {
  0, 1, 2, 2, 3, 0,
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
  Static_Mesh *square;

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

  Static_Mesh_Create_Info squareCreateInfo = 
  {
    .verts = verts, 
    .indices = indices,
    .nVerts = ELEMOF(verts),
    .nIndices = ELEMOF(indices),
  };

  err = RendererCreateStaticMesh(ren, &squareCreateInfo, &square);
  if (err)
  {
    LOG_FATAL_CODE("failed to create square mesh", err);
    return EXIT_FAILURE;
  }

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
    .pos = {2.0, 2.0, 2.0},
  };
  RendererSetCameraTransform(ren, cam, camTrans);
  RendererSetCameraActive(ren, cam);

  f64 baseTime = PlatGetTime();

  Transform trans = 
  {
    .pos = {0.0f, 0.0f, 0.0f},
    .rot = {0.0f, 0.0f, 45.0f},
  };

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
    RendererDrawStaticMesh(ren, square, trans);
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
  RendererDestroyStaticMesh(ren, square);
  PlatWindowDestroy(win);
  RendererDestroy(ren);
  FsDriverDestroy(&fs);

  MemoryPrintUsage();
  MemoryDeinit();

  LOG_DEBUG("exiting main function successfully");
  return EXIT_SUCCESS;
}
