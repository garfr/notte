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

int
main()
{
  Err_Code err;
  Plat_Window *win;
  Plat_Event ev;
  Renderer *ren;

  LogSetLevel(LOG_LEVEL_DEBUG);

  LOG_DEBUG("entering main function");

  MemoryInit();

  err = PlatInit();
  if (err)
  {
    LOG_FATAL_CODE("failed to init platform window", err);
    return EXIT_FAILURE;
  }
  
  Plat_Window_Create_Info createInfo =
  {
    .w = 100,
    .h = 100,
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
  };

  err = RendererCreate(&rendererCreateInfo, &ren);
  if (err)
  {
    LOG_FATAL_CODE("failed to create renderer", err);
    return EXIT_FAILURE;
  }

  while (1)
  {
    PlatWindowPumpEvents(win);
    while (PlatWindowGetEvent(win, &ev))
    {
      switch (ev.t)
      {
        case PLAT_EVENT_CLOSE:
          goto close;
      }
    }
  }

close:

  PlatWindowDestroy(win);
  RendererDestroy(ren);
  /*
  err = GraphicsInit();
  if (err)
  {
    LOG_FATAL_CODE("failed to initialize graphics", err);
    return EXIT_FAILURE;
  }

  while (!GraphicsShouldQuit())
  {
    GraphicsProcessEvents();
    GraphicsDrawFrame();
  }

  GraphicsDeinit();
  */

  MemoryPrintUsage();
  MemoryDeinit();

  LOG_DEBUG("exiting main function successfully");
  return EXIT_SUCCESS;
}
