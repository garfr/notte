/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * File system abstraction.
 */

#include <stdio.h>

#include <notte/fs.h>
#include <notte/thread.h>
#include <notte/log.h>

#define MAX_DIR_MONITOR_EVENTS 128
#define MAX_FILEPATH 128

/* === TYPES === */

typedef struct
{
  String root;
} Fs_Disk_Driver;

#ifdef NOTTE_WINDOWS 

#include <windows.h>

typedef struct
{
  DWORD action;
  char filepath[MAX_FILEPATH];
  bool skip;
} Raw_Event;

struct Fs_Dir_Monitor
{
  String rootPath;
  Allocator alloc;
  Thread *thread;
  Mutex *mutex;
  Fs_Dir_Monitor_Event events[MAX_DIR_MONITOR_EVENTS];
  Raw_Event rawEvents[MAX_DIR_MONITOR_EVENTS];
  usize eventsUsed, rawEventsUsed;
  HANDLE dirHandle;
  OVERLAPPED overlapped;
  uint8_t buffer[64512];
  bool shouldQuit;
  DWORD notifyFilter;
};

#endif

/* === PROTOTYPES === */

static void FsDiskDriverDestroy(Fs_Driver *driver);
static Err_Code FsDiskFileCreate(Fs_Driver *driver, String path, 
    Membuf *buf);
static void FsDiskFileDestroy(Fs_Driver *driver, Membuf *buf);
static void FsDirMonitorThread(void *ud);
static void ProcessRawEvents(Fs_Dir_Monitor *mon);
static int RefreshWatches(Fs_Dir_Monitor *mon);

/* === PUBLIC FUNCTIONS === */

Err_Code 
FsDiskDriverCreate(Fs_Driver *driverOut, 
                   Allocator alloc,
                   String root)
{
  Err_Code err;
  Fs_Disk_Driver *disk = NEW(alloc, Fs_Disk_Driver, MEMORY_TAG_FS);

  disk->root = root;

  driverOut->ud = disk;
  driverOut->alloc = alloc;
  driverOut->destroyFn = FsDiskDriverDestroy;
  driverOut->fileCreateFn = FsDiskFileCreate;;
  driverOut->fileDestroyFn = FsDiskFileDestroy;

  return ERR_OK;
}

void 
FsDriverDestroy(Fs_Driver *driver)
{
  driver->destroyFn(driver);
}

Err_Code 
FsFileLoad(Fs_Driver *driver, 
             String path, 
             Membuf *buf)
{
  return driver->fileCreateFn(driver, path, buf);
}

void 
FsFileDestroy(Fs_Driver *driver, 
              Membuf *buf)
{
  driver->fileDestroyFn(driver, buf);
}


Err_Code 
FsDirMonitorCreate(Allocator alloc, 
                   String rootPath, 
                   Fs_Dir_Monitor **monOut)
{
  Err_Code err;
  Fs_Dir_Monitor *mon = NEW(alloc, Fs_Dir_Monitor, MEMORY_TAG_FS);
  mon->alloc = alloc;
  mon->rootPath = StringClone(alloc, rootPath);
  mon->eventsUsed = mon->rawEventsUsed = 0;
  mon->shouldQuit = false;
  char *cRootPath;

  err = MutexCreate(alloc, &mon->mutex);
  if (err)
  {
    return err;
  }

  MutexAcquire(mon->mutex);
  Sleep(1000);
  MutexRelease(mon->mutex);
  cRootPath = StringMakeCString(alloc, rootPath);
  wchar_t wcPath[1000];

  usize convertedChars;
  mbstowcs_s(&convertedChars, wcPath, rootPath.len + 1, cRootPath, _TRUNCATE);

  FREE_ARR(alloc, cRootPath, char, rootPath.len + 1, MEMORY_TAG_STRING);
  mon->dirHandle = CreateFile(wcPath, GENERIC_READ, FILE_SHARE_READ |
      FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
  if (mon->dirHandle == INVALID_HANDLE_VALUE)
  {
    return ERR_LIBRARY_FAILURE;
  }

  mon->notifyFilter = FILE_NOTIFY_CHANGE_CREATION | 
    FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | 
    FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE;
  mon->overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

  err = ThreadCreate(alloc, mon, FsDirMonitorThread, &mon->thread);
  if (err)
  {
    return err;
  }

  *monOut = mon;
  return ERR_OK;
}

void 
FsDirMonitorDestroy(Fs_Dir_Monitor *mon)
{
  mon->shouldQuit = true;
}

Fs_Dir_Monitor_Event *
FsDirMonitorGetEvents(Fs_Dir_Monitor *mon, 
                      usize *size)
{
  MutexAcquire(mon->mutex);
  *size = mon->eventsUsed;
  mon->eventsUsed = 0;
  Fs_Dir_Monitor_Event *events = mon->events;
  MutexRelease(mon->mutex);
  return events;
}

/* === PRIVATE FUNCTIONS === */

static void 
FsDiskDriverDestroy(Fs_Driver *driver)
{
  FREE(driver->alloc, driver->ud, Fs_Disk_Driver, MEMORY_TAG_FS);
}

static Err_Code 
FsDiskFileCreate(Fs_Driver *driver, 
                 String path, 
                 Membuf *buf)
{
  usize pathSize, size;
  Err_Code err;
  u8 *data;
  Fs_Disk_Driver *disk = driver->ud;

  const char *cPath = StringConcatIntoCString(driver->alloc, disk->root, 
      path, &pathSize);

  FILE *file = fopen(cPath, "rb");
  if (file == NULL)
  {
    err = ERR_NO_FILE;
    goto fail;
  }

  fseek(file, 0, SEEK_END);
  size = ftell(file);
  fseek(file, 0, SEEK_SET);

  data = NEW_ARR(driver->alloc, u8, size + 1, MEMORY_TAG_FS);
  fread(data, 1, size, file);

  data[size] = '\0';

  buf->data = data,
  buf->size = size,

  FREE_ARR(driver->alloc, (char *) cPath, u8, pathSize, MEMORY_TAG_STRING);
  fclose(file);
  return ERR_OK;

fail:
  
  FREE_ARR(driver->alloc, (char *) cPath, u8, pathSize, MEMORY_TAG_STRING);
  return err;
}

static void 
FsDiskFileDestroy(Fs_Driver *driver, 
                  Membuf *buf)
{
  FREE_ARR(driver->alloc, (u8 *) buf->data, u8, buf->size + 1, MEMORY_TAG_FS);
}

static void 
FsDirMonitorThread(void *ud)
{
  DWORD result;
  Fs_Dir_Monitor *mon = (Fs_Dir_Monitor *) ud;
  HANDLE waitHandle = mon->overlapped.hEvent;
  RefreshWatches(mon);

  while (!mon->shouldQuit)
  {
    if (!MutexTryAcquire(mon->mutex))
    {
      Sleep(10);
      continue;
    }

    result = WaitForSingleObject(mon->overlapped.hEvent, 10);
    if (result != WAIT_TIMEOUT)
    {
      DWORD bytes;

      if (GetOverlappedResult(mon->dirHandle, &mon->overlapped, &bytes, FALSE))
      {
        char filepath[MAX_FILEPATH];

        PFILE_NOTIFY_INFORMATION notify;
        size_t offset = 0;

        if (bytes == 0)
        {
          RefreshWatches(mon);
          MutexRelease(mon->mutex);
          continue;
        }

        do {
          notify = (PFILE_NOTIFY_INFORMATION) &mon->buffer[offset];
          int count = WideCharToMultiByte(CP_UTF8, 0, notify->FileName, 
              notify->FileNameLength / sizeof(WCHAR),
              filepath, MAX_PATH - 1, NULL, NULL);
          filepath[count] = TEXT('\0');

          Raw_Event *ev = &mon->rawEvents[mon->rawEventsUsed++];
          ev->action = notify->Action;
          ev->skip = false;
          memcpy(ev->filepath, filepath, sizeof(ev->filepath));

          offset = notify->NextEntryOffset;
        } while (notify->NextEntryOffset > 0);

      }

      RefreshWatches(mon);
    }
    if (mon->rawEventsUsed > 0)
    {
      ProcessRawEvents(mon);
    }

    MutexRelease(mon->mutex);
    Sleep(10);
  }

  StringDestroy(mon->alloc, mon->rootPath);
  MutexDestroy(mon->alloc, mon->mutex);
  ThreadDestroy(mon->alloc, mon->thread);
  FREE(mon->alloc, mon, Fs_Dir_Monitor, MEMORY_TAG_FS);
}

static void
ProcessRawEvents(Fs_Dir_Monitor *mon)
{
  for (usize i = 0; i < mon->rawEventsUsed; i++)
  {
    Raw_Event *raw = &mon->rawEvents[i];

    if (raw->skip)
    {
      continue;
    }

    if (raw->action == FILE_ACTION_MODIFIED || raw->action == FILE_ACTION_ADDED)
    {
      for (usize j = i; j < mon->rawEventsUsed; j++)
      {
        Raw_Event *tmpRaw = &mon->rawEvents[j];
        if (tmpRaw->action == FILE_ACTION_MODIFIED &&
            strcmp(raw->filepath, tmpRaw->filepath) == 0)
        {
          tmpRaw->skip = true;
        }
      }
    }

    if (raw->action == FILE_ACTION_MODIFIED)
    {
      Fs_Dir_Monitor_Event *ev = &mon->events[mon->eventsUsed++];
      ev->t = FS_DIR_MONITOR_EVENT_MODIFY;
      ev->path = StringClone(mon->alloc, STRING_CSTR(raw->filepath));
    }
  }

  mon->rawEventsUsed = 0;
}

static int
RefreshWatches(Fs_Dir_Monitor *mon)
{
  return ReadDirectoryChangesW(mon->dirHandle, mon->buffer, sizeof(mon->buffer),
      TRUE, mon->notifyFilter, NULL, &mon->overlapped, NULL) != 0;
}
