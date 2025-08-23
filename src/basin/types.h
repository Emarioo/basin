#pragma once

#include "platform/string.h"

typedef u16 ImportID;

typedef enum {
    SUCCESS,
    FAILURE,
} ResultKind;

typedef struct {
    ResultKind kind;
    string message;
} Result;