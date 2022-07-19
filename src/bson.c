/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Parser for the BSON format.
 */

#include <notte/bson.h>
#include <notte/memory.h>
#include <notte/vector.h>
#include <notte/linear_allocator.h>

/* === TYPES === */

typedef struct Bson_KV Bson_KV;

typedef struct 
{
  Bson_KV *kv;
} Bson_Dict;

struct Bson_Value
{
  Bson_Value_Type t;
  union
  {
    f32 num;
    bool b;
    String str;
    struct
    {
      struct Bson_Value *arr;
      usize arrSz;
    };
    Bson_Dict dict;
  };
};

struct Bson_KV
{
  String key;
  Bson_Value val;
  struct Bson_KV *next;
};

struct Bson_Ast
{
  Bson_Value value;
  Linear_Allocator alloc;
};

typedef struct
{
  usize len;
  const u8 *src;
  usize idx;
  Bson_Ast *ast;
} Parser;

/* === MACROS === */

#define IS_EOF(_parser) ((_parser)->idx > (_parser)->len)
#define SKIPC(_parser) ((_parser)->idx++)
#define NEXTC(_parser) ((_parser)->src[(_parser)->idx++])
#define PEEKC(_parser) ((_parser)->src[(_parser)->idx])

#define INIT_ARR_CAP 8

/* === PROTOTYPES === */

static void SkipWhitespace(Parser *parser);
static Err_Code ParseValue(Parser *parser, Bson_Value *valueOut);
static Bson_KV *AddEntry(Parser *parser, Bson_Dict *dict, u8 *key, usize keySz,
    Bson_Value val);

/* === PUBLIC FUNCTIONS === */

Err_Code 
BsonAstParse(Bson_Ast **astOut, 
             Parse_Result *result, 
             Membuf buf)
{
  Bson_Ast *ast = MEMORY_NEW(Bson_Ast, MEMORY_TAG_BSON);
  ast->value.t = BSON_VALUE_DICT;
  LinearAllocatorInit(&ast->alloc);

  Parser parser =
  {
    .len = buf.size,
    .src = buf.data,
    .ast = ast,
  };

  while (!IS_EOF(&parser))
  {
    int c;
    SkipWhitespace(&parser);
    usize start = parser.idx;
    while (isalpha((c = PEEKC(&parser))) || c == '_')
    {
      SKIPC(&parser);
    }
    usize strLen = parser.idx - start;
    u8 *str = LinearAllocatorAlloc(&ast->alloc, strLen + 1);
    memcpy(str, parser.src + start, strLen);
    str[strLen] = '\0';
    while (PEEKC(&parser) != ':')
    {
      SKIPC(&parser);
    }
    SKIPC(&parser);
    Bson_Value value;
    ParseValue(&parser, &value);
    AddEntry(&parser, &parser.ast->value.dict, str, strLen, value);
  }

  *astOut = ast;

  return ERR_OK;
}

void BsonAstDestroy(Bson_Ast *ast)
{
  LinearAllocatorDeinit(&ast->alloc);
  MEMORY_FREE(ast, Bson_Ast, MEMORY_TAG_BSON);
}

Bson_Value *
BsonAstGetValue(Bson_Ast *ast)
{
  return &ast->value;
}

Bson_Value_Type 
BsonValueGetType(Bson_Value *value)
{
  return value->t;
}

String
BsonValueGetString(Bson_Value *value)
{
  return value->str;
}

float 
BsonValueGetNum(Bson_Value *value)
{
  return value->num;
}

bool 
BsonValueGetBool(Bson_Value *value)
{
  return value->b;
}

Bson_Value *
BsonValueLookupSlice(Bson_Value *value, const u8 *buf, usize len)
{
  String str = StringStealSlice(buf, len);
  return BsonValueLookup(value, str);
}

Bson_Value *
BsonValueLookup(Bson_Value *value, String str)
{
  Bson_KV *kv = value->dict.kv;
  while (kv != NULL)
  {
    if (StringEqual(str, kv->key))
    {
      return &kv->val;
    }
    kv = kv->next;
  }
  return NULL;
}

Bson_Value *
BsonValueGetIndex(Bson_Value *value, 
                  u32 index)
{
  if (index >= value->arrSz)
  {
    return NULL;
  }

  return &value->arr[index];
}

u32 
BsonValueGetLength(Bson_Value *value)
{
  return value->arrSz;
}

void 
BsonDictIteratorCreate(Bson_Value *value, 
                       Bson_Dict_Iterator *iter)
{
  iter->iter = value->dict.kv;
}

bool 
BsonDictIteratorNext(Bson_Dict_Iterator *iter, 
                     String *key,
                     Bson_Value **value)
{
  Bson_KV *kv = iter->iter;
  if (kv == NULL)
  {
    return false;
  }

  iter->iter = kv->next;
  *key = kv->key; 
  *value = &kv->val;
  return true;
}

/* === PRIVATE FUNCTIONS === */

static void 
SkipWhitespace(Parser *parser)
{
  while (isspace(PEEKC(parser)))
  {
    SKIPC(parser);
  }
}

static Err_Code 
ParseValue(Parser *parser, 
           Bson_Value *valueOut)
{
  int c;
  SkipWhitespace(parser);

  c = PEEKC(parser);
  if (c == '-' || isdigit(c))
  {
    bool negative = false;
    if (c == '-')
    {
      negative = true;
      SKIPC(parser);
    }

    float total = 0.0f;
    while (isdigit((c = PEEKC(parser))))
    {
      total *= 10.0f;
      total += (float) (c - '0');
      SKIPC(parser);
    }
    if (c == '.')
    {
      SKIPC(parser);
      float div = 0.1f;
      while (isdigit((c = PEEKC(parser))))
      {
        total += (div * ((float) (c - '0')));
        div /= 10.0f;
        SKIPC(parser);
      }
    }
    if (negative)
    {
      total *= -1.0f;
    }
    valueOut->t = BSON_VALUE_NUM;
    valueOut->num = total;
  } else if (c == '"')
  {
    SKIPC(parser);
    usize start = parser->idx;
    while (NEXTC(parser) != '\"')
      ;
    usize len = (parser->idx - start) - 1;
    u8 *buf = LinearAllocatorAlloc(&parser->ast->alloc, len + 1);
    memcpy(buf, parser->src + start, len);
    buf[len] = '\0';
    valueOut->t = BSON_VALUE_STRING;
    valueOut->str = buf;
    valueOut->strSz = len;
  } else if (c == 't')
  {
    SKIPC(parser);
    if (NEXTC(parser) == 'r'
     && NEXTC(parser) == 'u'
     && NEXTC(parser) == 'e')
    {
      valueOut->t = BSON_VALUE_BOOL;
      valueOut->b = true;
    }
  } else if (c == 'f')
  {
    SKIPC(parser);
    if (NEXTC(parser) == 'a'
     && NEXTC(parser) == 'l'
     && NEXTC(parser) == 's'
     && NEXTC(parser) == 'e')
    {
      valueOut->t = BSON_VALUE_BOOL;
      valueOut->b = false;
    }
  } else if (c == '[')
  {
    SKIPC(parser);
    usize cap = INIT_ARR_CAP;
    usize used = 0;
    Vector vec = VECTOR_CREATE(Bson_Value);

    while (1)
    {
      SkipWhitespace(parser);
      if (PEEKC(parser) == ']')
      {
        SKIPC(parser);
        Bson_Value *values = LinearAllocatorAlloc(&parser->ast->alloc,  
            sizeof(Bson_Value) * vec.elemsUsed);
        memcpy(values, vec.buf, sizeof(Bson_Value) * vec.elemsUsed);
        valueOut->t = BSON_VALUE_ARRAY;
        valueOut->arr = values;
        valueOut->arrSz = used;
        break;
      }
      Bson_Value val;
      Err_Code err = ParseValue(parser, &val);
      if (err)
      {
        return err;
      }
      VectorPush(&vec, &val);
      SkipWhitespace(parser);
      if (PEEKC(parser) == ',')
      {
        SKIPC(parser);
      }
    }
  } else if (c == '{')
  {
    SKIPC(parser);
    Bson_KV *chain = NULL;

    while (1)
    {
      SkipWhitespace(parser);
      if (PEEKC(parser) == '}')
      {
        valueOut->t = BSON_VALUE_DICT;
        valueOut->dict.kv = chain;
        break;
      }
      Bson_KV *kv = LinearAllocatorAlloc(&parser->ast->alloc, sizeof(Bson_KV));

      usize start = parser->idx;
      while (isalpha((c = PEEKC(parser))) || c == '_')
      {
        NEXTC(parser);
      }

      usize len = parser->idx - start;
      u8 *buf = LinearAllocatorAlloc(&parser->ast->alloc, len + 1);
      memcpy(buf, parser->src + start, len);
      buf[len] = '\0';
      kv->key = buf;
      kv->keySz = len;

      SkipWhitespace(parser);
      SKIPC(parser);
      SkipWhitespace(parser);

      Err_Code err = ParseValue(parser, &kv->val);
      if (err)
      {
        return err;
      }
      
      SkipWhitespace(parser);
      if (PEEKC(parser) == ',')
      {
        SKIPC(parser);
        SkipWhitespace(parser);
      }

      kv->next = chain;
      chain = kv;
    }

  }


  return ERR_OK;
}

static Bson_KV *
AddEntry(Parser *parser, 
         Bson_Dict *dict, 
         u8 *key, 
         usize keySz,
         Bson_Value val)
{
  Bson_KV *kv = LinearAllocatorAlloc(&parser->ast->alloc, sizeof(Bson_KV));

  kv->key = key;
  kv->keySz = keySz;
  kv->val = val;
  kv->next = dict->kv;
  dict->kv = kv;
  return kv;
}
