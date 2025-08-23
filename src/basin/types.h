#pragma once

#include "platform/string.h"

typedef u16 ImportID;

#define INVALID_IMPORT_ID 0xFFFFu

typedef enum {
    SUCCESS,
    FAILURE,
} ResultKind;

typedef struct {
    ResultKind kind;
    string message;
} Result;