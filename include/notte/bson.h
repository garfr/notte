/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Parser for the BSON format.
 */

#ifndef NOTTE_BSON_H
#define NOTTE_BSON_H

#include <notte/membuf.h>
#include <notte/error.h>
#include <notte/string.h>

typedef enum
{
  BSON_VALUE_NUM,
  BSON_VALUE_STRING,
  BSON_VALUE_BOOL,
  BSON_VALUE_DICT,
  BSON_VALUE_ARRAY,
} Bson_Value_Type;

typedef struct Bson_Ast Bson_Ast;
typedef struct Bson_Value Bson_Value;

typedef struct
{
  void *iter;
} Bson_Dict_Iterator;

Err_Code BsonAstParse(Bson_Ast **ast, Parse_Result *result, Membuf buf);
void BsonAstDestroy(Bson_Ast *ast);
Bson_Value *BsonAstGetValue(Bson_Ast *ast);

Bson_Value_Type BsonValueGetType(Bson_Value *value);

/* Returns NULL if not found. */
Bson_Value *BsonValueLookupSlice(Bson_Value *value, const u8 *str, usize size);
Bson_Value *BsonValueLookup(Bson_Value *value, String str);

String BsonValueGetString(Bson_Value *value);
float BsonValueGetNum(Bson_Value *value);
bool BsonValueGetBool(Bson_Value *value);

/* Returns NULL if out of bounds. */
Bson_Value *BsonValueGetIndex(Bson_Value *value, u32 index);
u32 BsonValueGetLength(Bson_Value *value);

void BsonDictIteratorCreate(Bson_Value *value, Bson_Dict_Iterator *iter);
bool BsonDictIteratorNext(Bson_Dict_Iterator *iter, String *key, 
    Bson_Value **value);

#endif /* NOTTE_BSON_H */
