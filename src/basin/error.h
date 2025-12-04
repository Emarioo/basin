/*
    Error handling

*/


#pragma once

#include "platform/string.h"

#define FORMAT_ERROR_MAX_CHARS 512

#define FORMAT_ERROR(RESULT, ERROR, FORMAT, ...) do {                                   \
    RESULT.error_type = ERROR;                                                          \
    RESULT.error_message = heap_alloc(FORMAT_ERROR_MAX_CHARS);                          \
    snprintf(RESULT.error_message, FORMAT_ERROR_MAX_CHARS, FORMAT, __VA_ARGS__);        \
    } while (0)
