/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * File system abstraction.
 */

#ifndef NOTTE_FS_H
#define NOTTE_FS_H

#include <notte/error.h>
#include <notte/memory.h>
#include <notte/string.h>
#include <notte/membuf.h>

typedef struct Fs_Driver Fs_Driver;

typedef void (*Fs_Driver_Destroy_Fn)(Fs_Driver *driver);
typedef Err_Code (*Fs_File_Create_Fn)(Fs_Driver *driver, String path, 
    Membuf *buf);
typedef void (*Fs_File_Destroy_Fn)(Fs_Driver *driver, Membuf *buf);

struct Fs_Driver
{
  void *ud;
  Allocator alloc;

  Fs_Driver_Destroy_Fn destroyFn;
  Fs_File_Create_Fn fileCreateFn;
  Fs_File_Destroy_Fn fileDestroyFn;
};

Err_Code FsDiskDriverCreate(Fs_Driver *driverOut, Allocator alloc,
    String rootPath);
Err_Code FsFileLoad(Fs_Driver *driver, String path, Membuf *buf);
void FsFileDestroy(Fs_Driver *driver, Membuf *buf);

void FsDriverDestroy(Fs_Driver *driver);

typedef struct Fs_Dir_Monitor Fs_Dir_Monitor;

typedef enum
{
  FS_DIR_MONITOR_EVENT_CREATE,
  FS_DIR_MONITOR_EVENT_DELETE,
  FS_DIR_MONITOR_EVENT_MOVE,
  FS_DIR_MONITOR_EVENT_MODIFY,
} Fs_Dir_Monitor_Event_Type;

typedef struct
{
  Fs_Dir_Monitor_Event_Type t;
  String path;
} Fs_Dir_Monitor_Event;

Err_Code FsDirMonitorCreate(Allocator alloc, String rootPath,
    Fs_Dir_Monitor **monOut);
void FsDirMonitorDestroy(Fs_Dir_Monitor *mon);
Fs_Dir_Monitor_Event *FsDirMonitorGetEvents(Fs_Dir_Monitor *mon, usize *size);

#endif /* NOTTE_FS_H */
