/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Logging.
 */

#ifndef NOTTE_LOG_H
#define NOTTE_LOG_H

#include <notte/error.h>

typedef enum
{
  LOG_LEVEL_DEBUG,
  LOG_LEVEL_WARN,
  LOG_LEVEL_ERROR,
  LOG_LEVEL_FATAL,
} Log_Level;

#define LOG_DEBUG(msg) _Log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, msg)
#define LOG_WARN(msg) _Log(LOG_LEVEL_WARN, __FILE__, __LINE__, msg)
#define LOG_ERROR(msg) _Log(LOG_LEVEL_ERROR, __FILE__, __LINE__, msg)
#define LOG_FATAL(msg) _Log(LOG_LEVEL_FATAL, __FILE__, __LINE__, msg)

#define LOG_DEBUG_FMT(msg, ...) _Log(LOG_LEVEL_DEBUG, __FILE__, __LINE__,      \
    msg, __VA_ARGS__)
#define LOG_WARN_FMT(msg, ...) _Log(LOG_LEVEL_WARN, __FILE__, __LINE__, msg,   \
    __VA_ARGS__)
#define LOG_ERROR_FMT(msg, ...) _Log(LOG_LEVEL_ERROR, __FILE__, __LINE__, msg, \
    __VA_ARGS__)
#define LOG_FATAL_FMT(msg, ...) _Log(LOG_LEVEL_FATAL, __FILE__, __LINE__,      \
    msg, __VA_ARGS__)

#define LOG_ERROR_CODE(msg, code) _LogCode(LOG_LEVEL_ERROR, __FILE__,          \
    __LINE__, msg, code)
#define LOG_FATAL_CODE(msg, code) _LogCode(LOG_LEVEL_FATAL, __FILE__,          \
    __LINE__, msg, code)

void LogSetLevel(Log_Level lvl);

/* Should not be called manually. */
void _Log(Log_Level lvl, const char *file, int line, const char *msg, ...);
void _LogCode(Log_Level lvl, const char *file, int line, const char *msg, 
    Err_Code err, ...);

#endif /* NOTTE_LOG_H */
