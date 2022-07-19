/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Win32 backend for plat.h.
 */

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif /* WIN32_LEAN_AND_MEAN */
#include <windows.h>

#include <notte/plat.h>
#include <notte/memory.h>
#include <notte/log.h>

#define PLAT_WINDOW_MAX_EVENTS 128

/* === TYPES === */

struct Plat_Window
{
  HWND hwnd;
  bool shouldClose;
  Plat_Event events[PLAT_WINDOW_MAX_EVENTS];
  usize eventTop, eventBottom;
  Allocator alloc;
};

/* === CONSTANTS === */

const wchar_t CLASS_NAME[] = L"notte";

const char *instanceExtensions[] = {
  "VK_KHR_win32_surface",
};

/* === PROTOTYPES === */

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, 
    LPARAM lParam);
static Plat_Event *AllocEvent(Plat_Window *win, Plat_Event_Type t);

/* === PUBLIC FUNCTIONS === */

Err_Code 
PlatInit(void)
{
  WNDCLASS wc = {
    .lpfnWndProc = WindowProc,
    .hInstance = GetModuleHandle(NULL),
    .lpszClassName = CLASS_NAME,
  };

  if (RegisterClass(&wc) == 0)
  {
    return ERR_LIBRARY_FAILURE;
  }

  return ERR_OK;
}

Err_Code 
PlatWindowCreate(Plat_Window_Create_Info *info, 
                 Plat_Window **winOut)
{
  Plat_Window *win = NEW(info->alloc, Plat_Window, MEMORY_TAG_PLATFORM);

  win->alloc = info->alloc;
  win->shouldClose = false;
  win->eventTop = win->eventBottom = 0;

  win->hwnd = CreateWindowEx(
      0,
      CLASS_NAME,
      L"notte",
      WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      NULL,
      NULL,
      GetModuleHandle(NULL),
      NULL
  );

  SetWindowLongPtr(win->hwnd, GWLP_USERDATA, (LONG_PTR) win);

  ShowWindow(win->hwnd, SW_NORMAL);

  if (win->hwnd == NULL)
  {
    return ERR_LIBRARY_FAILURE;
  }

  *winOut = win;
  return ERR_OK;
}

void 
PlatWindowDestroy(Plat_Window *win)
{
  FREE(win->alloc, win, Plat_Window, MEMORY_TAG_PLATFORM);
}

bool 
PlatWindowShouldClose(Plat_Window *win)
{
  return win->shouldClose;
}

void
PlatWindowPumpEvents(Plat_Window *win)
{
  MSG msg;
  while (PeekMessage(&msg, win->hwnd, 0, 0, PM_REMOVE) > 0)
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}

bool
PlatWindowGetEvent(Plat_Window *win, 
                   Plat_Event *event)
{
  if (win->eventBottom == win->eventTop)
  {
    win->eventBottom = win->eventTop = 0;
    return false;
  }

  *event = win->events[win->eventBottom++];

  return true;
}

Err_Code
PlatWindowGetInstanceExtensions(Plat_Window *win, 
                                u32 *nNames, 
                                const char **names)
{
  if (names == NULL)
  {
    *nNames = sizeof(instanceExtensions) / sizeof(instanceExtensions[0]);
  }
  else
  {
    for (u32 i = 0; i < *nNames; i++)
    {
      names[i] = instanceExtensions[i];
    }
  }

  return true;
}


Err_Code 
PlatWindowCreateVulkanSurface(Plat_Window *win, 
                              VkInstance vk,
                              VkAllocationCallbacks *alloc,
                              VkSurfaceKHR *surfaceOut)
{
  VkResult vkErr;

  VkWin32SurfaceCreateInfoKHR createInfo =
  {
    .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
    .hwnd = win->hwnd,
    .hinstance = GetModuleHandle(NULL),
  };

  vkErr = vkCreateWin32SurfaceKHR(vk, &createInfo, alloc, surfaceOut);
  if (vkErr)
  {
    return ERR_LIBRARY_FAILURE;
  }
  
  return ERR_OK;
}


void 
PlatWindowGetFramebufferSize(Plat_Window *win, 
                             u32 *w, 
                             u32 *h)
{
  RECT area;
  GetClientRect(win->hwnd, &area);
  if (w != NULL)
  {
    *w = area.right;
  }
  if (h != NULL)
  {
    *h = area.bottom;
  }
}

/* === PRIVATE FUNCTIONS === */

static Plat_Event *
AllocEvent(Plat_Window *win,
           Plat_Event_Type t)
{
  Plat_Event *ev = &win->events[win->eventTop++];
  ev->t = t;
  return ev;
}

static LRESULT CALLBACK 
WindowProc(HWND hwnd, 
           UINT uMsg, 
           WPARAM wParam, 
           LPARAM lParam)
{
  Plat_Event *ev;
  Plat_Window *win = (Plat_Window *) GetWindowLongPtr(hwnd, GWLP_USERDATA);

  switch (uMsg)
  {
    case WM_QUIT:
    case WM_CLOSE:
      LOG_DEBUG("quitting");
      ev = AllocEvent(win, PLAT_EVENT_CLOSE);
      break;
    default:
      return DefWindowProc(hwnd, uMsg, wParam, lParam);
  }
  return 0;
}
