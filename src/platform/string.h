#pragma once

typedef struct string {
    // YOU CANNOT REARRANGE THESE FIELDS.
    // CODE DEPENDS ON IT BEING THIS BECAUSE OF INITIALIZER
    char* ptr;
    u64 len;
    u64 max;
} string;

typedef struct cstring {
    // YOU CANNOT REARRANGE THESE FIELDS.
    // CODE DEPENDS ON IT BEING THIS BECAUSE OF INITIALIZER
    const char* ptr;
    u64 len;
} cstring;

static inline cstring cstr(string s) { cstring c = { s.ptr, s.len }; return c; };
static inline cstring cstr_cptr(const char* s) { cstring c = { s, strlen(s) }; return c; };

bool string_equal_cstr(cstring str, char* ptr);

string string_clone_cstr(cstring text);
string string_clone_cptr(const char* text);
string string_clone(const char* text, int len);

#ifdef IMPL_PLATFORM

#include "platform/memory.h"

bool string_equal_cstr(cstring str, char* ptr) {
    int len = strlen(ptr);
    return str.len == len && !memcmp(str.ptr, ptr, len);
}


string string_clone_cstr(cstring text) {
    return string_clone(text.ptr, text.len);
}

string string_clone_cptr(const char* text) {
    return string_clone(text, strlen(text));
}

string string_clone(const char* text, int len) {
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