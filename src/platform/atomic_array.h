#pragma once

#include "platform/memory.h"
#include "platform/thread.h"

#define DEF_ATOMIC_ARRAY(T)                     \
    typedef struct AtomicArray_##T {             \
        volatile int    len;                    \
        volatile int    cap;                    \
        int             elements_per_chunk;     \
        int             element_size;           \
        Mutex           mutex;                  \
        volatile T**    chunks;                 \
} AtomicArray_##T;

typedef struct AtomicArray {
    volatile int    len;
    volatile int    cap;
    int             elements_per_chunk;
    int             element_size;
    Mutex           mutex;
    volatile void** chunks;
} AtomicArray;


// NOT THREAD SAFE
void _atomic_array_init(AtomicArray* arr, int elements_per_chunk, int max_chunks, int element_size);
void _atomic_array_cleanup(AtomicArray* arr);
// THREAD SAFE
int _atomic_array_push(AtomicArray* arr);


// NOT THREAD SAFE
#define atomic_array_init(ARR, ELEMENTS_PER_CHUNK, MAX_CHUNKS) \
    _atomic_array_init((AtomicArray*)ARR, sizeof(**(ARR)->chunks), ELEMENTS_PER_CHUNK, MAX_CHUNKS)
#define atomic_array_cleanup(ARR) \
    _atomic_array_cleanup((AtomicArray*)ARR)

// THREAD SAFE
#define atomic_array_push(ARR) \
    _atomic_array_push((AtomicArray*)ARR)
#define atomic_array_get(ARR) \
    *(arr->chunks[index / arr->elements_per_chunk] + index % arr->elements_per_chunk)
#define atomic_array_getptr(ARR) \
    (arr->chunks[index / arr->elements_per_chunk] + index % arr->elements_per_chunk)

#ifdef IMPL_PLATFORM

// NOT THREAD SAFE
void _atomic_array_init(AtomicArray* arr, int elements_per_chunk, int max_chunks, int element_size) {
    memset(arr, 0, sizeof(*arr));
    create_mutex(&arr->mutex);
    arr->elements_per_chunk = elements_per_chunk;
    arr->element_size = element_size;
    arr->chunks = heap_alloc(sizeof(u8*) * max_chunks);
}
// NOT THREAD SAFE
void _atomic_array_cleanup(AtomicArray* arr) {
    cleanup_mutex(&arr->mutex);
    for (int i=0;i<arr->cap / arr->elements_per_chunk;i++) {
        heap_free(arr->chunks[i]);
    }
    heap_free(arr->chunks);
    arr->chunks = NULL;
}

// THREAD SAFE
int _atomic_array_push(AtomicArray* arr) {
    int index = atomic_add(&arr->len, 1);
    if (index >= arr->cap) {
        lock_mutex(&arr->mutex);
        if (index >= arr->cap) {
            arr->chunks[index / arr->elements_per_chunk] = heap_alloc(arr->element_size * arr->elements_per_chunk);
            arr->cap += arr->elements_per_chunk;
        }
        unlock_mutex(&arr->mutex);
    }
    return index;
}

#endif // IMPL_PLATFORM
