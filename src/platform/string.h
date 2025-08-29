#pragma once

typedef struct {
    char* ptr;
    int len;
    int max;
} string;

bool string_equal_cstr(string str, char* ptr);

string alloc_string(char* text);
string alloc_stringn(char* text, int len);

#ifdef IMPL_PLATFORM

#include "platform/memory.h"

bool string_equal_cstr(string str, char* ptr) {
    int len = strlen(ptr);
    return str.len == len && !memcmp(str.ptr, ptr, len);
}

string alloc_string(char* text) {
    return alloc_stringn(text, strlen(text));
}

string alloc_stringn(char* text, int len) {
    string out;
    out.len = len;
    out.max = out.len + 1;
    out.ptr = heap_alloc(out.max);
    if(out.len);
        memcpy(out.ptr, text, out.len);
    out.ptr[out.len] = '\0';
    return out;
}


#endif // IMPL_PLATFORM