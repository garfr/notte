/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Error handling and utilities. 
 */

#include <notte/defs.h>
#include <notte/error.h>

/* 
 * If we used a manual lookup table here, forgetting to create an entry for
 * a new Err_Code variant would cause undefined behaviour.  So we use a 
 * function here, which gets comopiled to a lookup table anyway.
 */
const char *
errorToStr(Err_Code err)
{
  switch (err)
  {
    case ERR_OK:
      return "OK";
    case ERR_NO_MEM:
      return "NO_MEM";
    case ERR_NO_FILE:
      return "NO_FILE";
    case ERR_LIBRARY_FAILURE:
      return "LIBRARY_FAILURE";
    case ERR_INVALID_USAGE:
      return "INVALID_USAGE";
    case ERR_FAILED_PARSE:
      return "FAILED_PARSE";
    case ERR_INVALID_SHADER:
      return "INVALID_SHADER";
    case ERR_NO_SUITABLE_HARDWARE:
      return "NO_SUITABLE_HARDWARE";
    case ERR_UNIMPLEMENTED_FUNCTIONALITY:
      return "UNIMPLEMENTED_FUNCTIONALITY";
    case ERR_CYCLICAL_RENDER_GRAPH:
      return "CYCLICAL_RENDER_GRAPH";
  }

  NOTTE_UNREACHABLE();
}
