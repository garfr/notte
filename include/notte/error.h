/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Error handling and utilities. 
 */

#ifndef NOTTE_ERROR_H
#define NOTTE_ERROR_H

typedef enum
{
  ERR_OK,

  ERR_NO_MEM,
  ERR_NO_FILE,
  ERR_LIBRARY_FAILURE, /* Unexplainable library failure. */
  ERR_INVALID_USAGE, /* Invalid usage of a function. */
  ERR_FAILED_PARSE, /* Failed to parsea a file format. */
  ERR_INVALID_SHADER, /* When a shader fails to compile. */
  ERR_UNIMPLEMENTED_FUNCTIONALITY, /* When a feature is required but not 
                                      implemented. */
  ERR_CYCLICAL_RENDER_GRAPH,
  ERR_NO_SUITABLE_HARDWARE,
} Err_Code;


const char *errorToStr(Err_Code err);

#endif /* NOTTE_ERROR_H */
