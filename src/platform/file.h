#pragma once

#include "platform/string.h"
#include "platform/memory.h"
#include "platform/assert.h"

string read_file(char* path);

#ifdef IMPL_PLATFORM

string read_file(char* path) {
    string str = {0};
    FILE* file = fopen(path, "rb");
    if(!file) {
        return str;
    }
    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    fseek(file, 0, SEEK_SET);

    void* ptr = heap_alloc(size + 1);
    ASSERT(ptr);

    int read = fread(ptr, 1, size, file);
    fclose(file);
    ASSERT(read == size);

    str.ptr = ptr;
    str.max = size;
    str.len = size;
    return str;
}

#endif // IMPL_PLATFORM