#pragma once

#include "platform/memory.h"
#include "platform/assert.h"

#define DEF_ARRAY(T)                                                                                    \
    typedef struct {                                                                                    \
        T* ptr;                                                                                         \
        int len;                                                                                        \
        int max;                                                                                        \
    } Array_##T;                                                                                        \
    static inline void array_init_##T(T* array, int max) { array_init(array, sizeof(T), max); }         \
    static inline void array_push_##T(T* array, T* element) { array_push(array, sizeof(T), element); }  \
    static inline void array_pop_##T(T* array) { array_pop(array, sizeof(T)); }                         \
    static inline void array_cleanup_##T(T* array) { array_cleanup(array, sizeof(T)); }

typedef struct {
    void* ptr;
    int len;
    int max;
} Array;

void array_init(Array* array, int element_size, int initial_max);
void array_cleanup(Array* array, int element_size);

void array_push(Array* array, int element_size, void* element);
void array_pop(Array* array, int element_size);


#ifdef IMPL_PLATFORM

void array_init(Array* array, int element_size, int max) {
    array->ptr = heap_alloc(max * element_size);
    array->len = 0;
    array->max = max;
}
void array_cleanup(Array* array, int element_size) {
    heap_free(array->ptr);
    array->len = 0;
    array->max = 0;
}

void array_push(Array* array, int element_size, void* element) {
    if(array->len >= array->max) {
        int new_max = array->max * 2 + 10;
        void* new_ptr = heap_realloc(array->ptr, new_max * element_size);
        array->ptr = new_ptr;
        array->max = new_max;
    }

    memcpy((char*)array->ptr + array->len * element_size, element, element_size);
    array->len++;
}
void array_pop(Array* array, int element_size) {
    ASSERT(array->len > 0);
    array->len--;
}

#endif // IMPL_PLATFORM