#pragma once

typedef struct string {
    // YOU CANNOT REARRANGE THESE FIELDS.
    // CODE DEPENDS ON IT BEING THIS BECAUSE OF INITIALIZER
    char* ptr;
    u32 len;
    u32 max;
} string;

typedef struct cstring {
    // YOU CANNOT REARRANGE THESE FIELDS.
    // CODE DEPENDS ON IT BEING THIS BECAUSE OF INITIALIZER
    const char* ptr;
    u64 len;
} cstring;

static inline cstring cstr(string s) { cstring c = { s.ptr, s.len }; return c; };
static inline cstring cstr_cptr(const char* s) { cstring c = { s, strlen(s) }; return c; };

// text null terminated
int string_find(const char* text, int pos, const char* pattern);
int string_rfind(const char* text, int pos, const char* pattern);

bool string_equal_cstr(cstring str, char* ptr);
bool string_equal(cstring a, cstring b);

string string_create(int cap);
string string_clone_cstr(cstring text);
string string_clone_cptr(const char* text);
string string_clone(const char* text, int len);
void   string_cleanup(string* str);

void   string_append_cstr(string* str, cstring cstr);
void   string_append_cptr(string* str, const char* cptr);
void   string_append_char(string* str, char c);
void   string_append(string* str, const char* text, int len);


#ifdef IMPL_PLATFORM

#include "platform/memory.h"

bool string_equal_cstr(cstring str, char* ptr) {
    int len = strlen(ptr);
    return str.len == len && !memcmp(str.ptr, ptr, len);
}

bool string_equal(cstring a, cstring b) {
    return a.len == b.len && !memcmp(a.ptr, b.ptr, a.len);
}

int string_find(const char* text, int pos, const char* pattern) {
    char* ptr = strstr(text + pos, pattern);
    if (!ptr) {
        return -1;
    }
    return (int)(ptr - text);
}
int string_rfind(const char* text, int pos, const char* pattern) {
    int pattern_len = strlen(pattern);
    if (pattern_len == 1) {
        char c = pattern[0];
        while (pos >= 0 && text[pos] != c) pos--;
        return pos;
    }

    ASSERT(false);
}

string string_create(int cap) {
    string s = {};
    s.max = cap;
    s.ptr = heap_alloc(s.max + 1);
    ASSERT(s.ptr);
    return s;
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
    out.max = out.len;
    out.ptr = heap_alloc(out.max + 1);
    if(out.len);
        memcpy(out.ptr, text, out.len);
    out.ptr[out.len] = '\0';
    return out;
}
void string_cleanup(string* str) {
    heap_free(str->ptr);
    memset(str, 0, sizeof(*str));
}


void string_append_cstr(string* str, cstring cstr) {
    string_append(str, cstr.ptr, cstr.len);
}
void string_append_cptr(string* str, const char* cptr) {
    string_append(str, cptr, strlen(cptr));
}
void string_append_char(string* str, char c) {
    string_append(str, &c, 1);
}
void string_append(string* str, const char* text, int len) {
    if (str->len + len > str->max) {
        int new_max = str->max*2 + 10 + len;
        str->ptr = heap_realloc(str->ptr, new_max + 1);
        str->max = new_max;
    }
    memcpy(str->ptr + str->len, text, len);
    str->len += len;
    str->ptr[str->len] = '\0';
}


#endif // IMPL_PLATFORM
