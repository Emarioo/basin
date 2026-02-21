#pragma once

#include "util/string.h"

typedef u16 ImportID;

#define INVALID_IMPORT_ID 0xFFFFu


typedef struct TokenStream TokenStream;
typedef struct AST AST;

typedef struct {
    ImportID import_id;
    string path; // sometimes we don't have path, for small code created through metaprogramming for example.
    string text; // text may be empty, in this case the driver needs to look at path and read the file
    TokenStream* stream;
    AST* ast;
} Import;

typedef enum {
    SUCCESS,
    FAILURE,
} ResultKind;

typedef struct {
    ResultKind kind;
    string message;
} Result;

typedef struct {
    const char* path;
    int line;
} CLocation;

#define CLOCATION { __FILE__, __LINE__ }


