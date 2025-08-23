#pragma once

#include "platform/assert.h"

void* heap_alloc(int size);
void* heap_realloc(void* ptr, const int size);
void heap_free(void* ptr);

#define HEAP_ALLOC_OBJECT(T) (T*)_heap_alloc_object(sizeof(T));
static inline void* _heap_alloc_object(const int size) {
    void* ptr = heap_alloc(size);
    memset(ptr, 0, size);
    return ptr;
}

#ifdef IMPL_PLATFORM

void* heap_alloc(int size) {
    void* ptr = malloc(size);
    ASSERT(ptr);
    return ptr;
}
void* heap_realloc(void* ptr, const int size) {
    void* new_ptr = realloc(ptr, size);
    ASSERT(new_ptr);
    return new_ptr;
}
void heap_free(void* ptr) {
    free(ptr);
}

#endif // IMPL_PLATFORM