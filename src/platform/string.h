#pragma once

typedef struct {
    char* ptr;
    int len;
    int max;
} string;

bool string_equal_cstr(string str, char* ptr);

#ifdef IMPL_PLATFORM

bool string_equal_cstr(string str, char* ptr) {
    return !strcmp(str.ptr, ptr);
}

#endif // IMPL_PLATFORM