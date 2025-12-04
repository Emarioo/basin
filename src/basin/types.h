#pragma once

#include "platform/string.h"

typedef u16 ImportID;

#define INVALID_IMPORT_ID 0xFFFFu


typedef struct {
    ImportID import_id;
    string path; // sometimes we don't have path, for small code created through metaprogramming for example.
    string text;
} Import;

typedef enum {
    SUCCESS,
    FAILURE,
} ResultKind;

typedef struct {
    ResultKind kind;
    string message;
} Result;

