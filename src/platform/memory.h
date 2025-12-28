#pragma once

#include "platform/assert.h"

void* heap_alloc(int size);
void* heap_realloc(void* ptr, const int size);
void heap_free(void* ptr);


typedef struct {
    void* ptr;
    int len;
    int max;
} Memory;

void memory_init(Memory* memory, int size);
void memory_cleanup(Memory* memory);
void memory_resize(Memory* array, int new_size);



#define HEAP_ALLOC_OBJECT(T) (T*)_heap_alloc_object(sizeof(T));
static inline void* _heap_alloc_object(const int size) {
    void* ptr = heap_alloc(size);
    memset(ptr, 0, size);
    return ptr;
}

#define HEAP_ALLOC_ARRAY(T,N) (T*)heap_alloc(sizeof(T) * (N))

#ifdef IMPL_PLATFORM

void* heap_alloc(int size) {
    void* ptr = malloc(size);
    ASSERT(ptr);
    return ptr;
}
void* heap_realloc(void* ptr, const int size) {
    if (!ptr)
        return heap_alloc(size);

    void* new_ptr = realloc(ptr, size);
    ASSERT(new_ptr);
    return new_ptr;
}
void heap_free(void* ptr) {
    free(ptr);
}


void memory_init(Memory* memory, int size) {
    memory->ptr = heap_alloc(size);
    memory->len = 0;
    memory->max = size;
}
void memory_cleanup(Memory* memory) {
    heap_free(memory->ptr);
    memory->len = 0;
    memory->max = 0;
}

void memory_resize(Memory* memory, int new_size) {
    void* new_ptr = heap_realloc(memory->ptr, new_size);
    ASSERT(new_ptr);
    memory->ptr = new_ptr;
    memory->max = new_size;
    if (memory->len > new_size)
        memory->len = new_size;
}

#endif // IMPL_PLATFORM