#pragma once

#include "platform/platform.h"

#define DEF_ATOMIC_ARRAY(T)                     \
    typedef struct AtomicArray_##T {            \
        volatile int    len;                    \
        volatile int    cap;                    \
        volatile int    chunk_cap;              \
        int             elements_per_chunk;     \
        int             element_size;           \
        Mutex           mutex;                  \
        T* volatile* volatile chunks;           \
        T* volatile* volatile old_chunks;       \
} AtomicArray_##T;

typedef struct AtomicArray {
    volatile int    len;
    volatile int    cap;
    volatile int    chunk_cap;
    int             elements_per_chunk;
    int             element_size;
    Mutex           mutex;
    void* volatile* volatile chunks;
    void* volatile* volatile old_chunks;
} AtomicArray;


// NOT THREAD SAFE
void _atomic_array_init(AtomicArray* arr, int element_size, int initial_cap, int elements_per_chunk);
void _atomic_array_cleanup(AtomicArray* arr);
// THREAD SAFE
int _atomic_array_push(AtomicArray* arr, void* element);

// NOT THREAD SAFE
#define atomic_array_init(ARR, CAP, ELEMENTS_PER_CHUNK) \
    _atomic_array_init((AtomicArray*)ARR, sizeof(**(ARR)->chunks), CAP, ELEMENTS_PER_CHUNK)
#define atomic_array_cleanup(ARR) \
    _atomic_array_cleanup((AtomicArray*)ARR)

// THREAD SAFE
#define atomic_array_push(ARR, ELEMENTP) \
    _atomic_array_push((AtomicArray*)(ARR), (false && ( *(ARR)->chunks[0] = *(ELEMENTP), false), ELEMENTP))
#define atomic_array_get(ARR, INDEX) \
    (ARR)->chunks[(INDEX) / (ARR)->elements_per_chunk][(INDEX) % (ARR)->elements_per_chunk]
#define atomic_array_getptr(ARR, INDEX) \
    (&(ARR)->chunks[(INDEX) / (ARR)->elements_per_chunk][(INDEX) % (ARR)->elements_per_chunk])

#ifdef IMPL_PLATFORM

// NOT THREAD SAFE
void _atomic_array_init(AtomicArray* arr, int element_size, int initial_cap, int elements_per_chunk) {
    memset(arr, 0, sizeof(*arr));
    thread__create_mutex(&arr->mutex);
    arr->element_size = element_size;
    arr->elements_per_chunk = elements_per_chunk;
    arr->chunk_cap = (initial_cap + elements_per_chunk - 1) / elements_per_chunk;
    if (arr->chunk_cap != 0) {
        arr->chunks = mem__alloc(sizeof(void*) * arr->chunk_cap);
    }

    for (int i=0;i<arr->chunk_cap;i++) {
        arr->chunks[i] = mem__alloc(element_size * elements_per_chunk);
    }
}
// NOT THREAD SAFE
void _atomic_array_cleanup(AtomicArray* arr) {
    thread__cleanup_mutex(&arr->mutex);
    for (int i=0;i<arr->cap / arr->elements_per_chunk;i++) {
        mem__free((void*)arr->chunks[i]);
    }
    mem__free((void*)arr->chunks);
    arr->chunks = NULL;
}

// THREAD SAFE
int _atomic_array_push(AtomicArray* arr, void* element) {
    int index = atomic_add(&arr->len, 1);
    if (index >= arr->cap) {
        thread__lock_mutex(&arr->mutex);
        if (index >= arr->cap) {
            int chunk_index = index / arr->elements_per_chunk;
            if (chunk_index >= arr->chunk_cap) {
                // Resize chunks array.
                // Since another thread may be accessing the chunks array
                // at any moment we cannot free it right away.
                // We can however allocate a bigger new chunk array, copy
                // chunks to that new array, then finally replace the chunks pointer
                // in the atomic array struct.
                // Since we lock_mutex no other thread will be replacing or adding
                // new chunks so no race conditions.
                
                // We save chunks array to old chunks
                // and free it next time we need to resize array.
                // We hope that the chunks array isn't stuck in a register
                // in a thread that have been context switched out
                // for a long time. Is this preferably to leaking memory?
                if (arr->old_chunks)
                    mem__free((void*)arr->old_chunks);
                arr->old_chunks = arr->chunks;

                int old_chunk_cap = arr->chunk_cap;
                arr->chunk_cap = old_chunk_cap * 2;
                void** new_chunks = mem__alloc(sizeof(void*) * arr->chunk_cap);
                memcpy(new_chunks, (void*)arr->old_chunks, sizeof(void*) * old_chunk_cap);
                arr->chunks = new_chunks;
            }

            arr->chunks[chunk_index] = mem__alloc(arr->element_size * arr->elements_per_chunk);
            arr->cap += arr->elements_per_chunk;
        }
        thread__unlock_mutex(&arr->mutex);
    }
    if (element) {
        memcpy((char*)arr->chunks[index/arr->elements_per_chunk] + arr->element_size * (index % arr->elements_per_chunk), element, arr->element_size);
    }
    return index;
}

#endif // IMPL_PLATFORM
