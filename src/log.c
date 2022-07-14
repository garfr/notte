/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Logging.
 */

#include <stdio.h>
#include <stdarg.h>

#include <notte/log.h>

static Log_Level minLevel = LOG_LEVEL_WARN;

/* Lookup table for string name of error level. */
const char *lvl_to_str[] = {
  [LOG_LEVEL_DEBUG] = "DEBUG",
  [LOG_LEVEL_WARN] = "WARN ",
  [LOG_LEVEL_ERROR] = "ERROR",
  [LOG_LEVEL_FATAL] = "FATAL",
};

void 
LogSetLevel(Log_Level lvl)
{
  minLevel = lvl;
}

void 
_Log(Log_Level lvl, 
     const char *file, 
     int line, 
     const char *msg, 
     ...)
{
  va_list args;

  va_start(args, msg);

  if (lvl < minLevel)
  {
    return;
  }

  fprintf(stderr, "%s %s:%d: ", lvl_to_str[lvl], file, line);
  vfprintf(stderr, msg, args);

  va_end(args);

  fprintf(stderr, "\n");
}

void 
_LogCode(Log_Level lvl, 
          const char *file, 
          int line,               
          const char *msg, 
          Err_Code err, ...)
{
  va_list args;

  va_start(args, msg);

  if (lvl < minLevel)
  {
    return;
  }

  fprintf(stderr, "%s %s:%d: ", lvl_to_str[lvl], file, line);
  vfprintf(stderr, msg, args);

  va_end(args);

  fprintf(stderr, ": %s\n", errorToStr(err));
}

