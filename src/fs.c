/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * File system abstraction.
 */

#include <stdio.h>

#include <notte/fs.h>

/* === TYPES === */

typedef struct
{
  String root;
} Fs_Disk_Driver;

/* === PROTOTYPES === */

static void FsDiskDriverDestroy(Fs_Driver *driver);
static Err_Code FsDiskFileCreate(Fs_Driver *driver, String path, 
    Membuf *buf);
static void FsDiskFileDestroy(Fs_Driver *driver, Membuf *buf);


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
