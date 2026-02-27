#pragma once

#include "platform/memory.h"
#include "platform/assert.h"

#define DEF_ARRAY(T)                                                                                    \
    typedef struct {                                                                                    \
        T* ptr;                                                                                         \
        int len;                                                                                        \
        int max;                                                                                        \
    } Array_##T;

#define array_init(ARR, MAX) _array_init((Array*)(ARR), sizeof(*(ARR)->ptr), MAX);
#define array_cleanup(ARR) _array_cleanup((Array*)(ARR), sizeof(*(ARR)->ptr))

#define array_push(ARR, ELEMENTP) ( _array_push((Array*)(ARR), sizeof(*(ARR)->ptr), (false && (((ARR)->ptr[0] = *(ELEMENTP)), false), ELEMENTP)))
#define array_pushv(ARR, ELEMENT) ( _array_push((Array*)(ARR), sizeof(*(ARR)->ptr), NULL), (ARR)->ptr[(ARR)->len-1] = (ELEMENT))
#define array_pop(ARR) (ASSERT_INDEX((ARR)->len > 0), (ARR)->len--)
#define array_last(ARR) (ASSERT_INDEX((ARR)->len > 0), (ARR)->ptr[(ARR)->len-1])
#define array_lastptr(ARR) (ASSERT_INDEX((ARR)->len > 0), &(ARR)->ptr[(ARR)->len-1])
#define array_get(ARR, INDEX) (ASSERT_INDEX((INDEX) < (ARR)->len), (ARR)->ptr[INDEX])
#define array_getptr(ARR, INDEX) (ASSERT_INDEX((INDEX) < (ARR)->len), &(ARR)->ptr[INDEX])
#define array_size(ARR) ((ARR)->len)

typedef struct {
    void* ptr;
    int len;
    int cap;
} Array;

void _array_init(Array* array, int element_size, int initial_cap);
void _array_cleanup(Array* array, int element_size);

void _array_push(Array* array, int element_size, void* element);
void _array_pop(Array* array, int element_size);

// common
DEF_ARRAY(int)

#ifdef IMPL_PLATFORM

void _array_init(Array* array, int element_size, int cap) {
    memset(array, 0, sizeof(*array));
    array->ptr = heap_alloc(cap * element_size);
    array->len = 0;
    array->cap = cap;
}
void _array_cleanup(Array* array, int element_size) {
    heap_free(array->ptr);
    array->len = 0;
    array->cap = 0;
}

void _array_push(Array* array, int element_size, void* element) {
    if (!array->ptr) {
        _array_init(array, element_size, 10);
    }
    if(array->len >= array->cap) {
        int new_max = array->cap * 2 + 10;
        void* new_ptr = heap_realloc(array->ptr, new_max * element_size);
        array->ptr = new_ptr;
        array->cap = new_max;
    }
    if (element) {
        void* spot = (char*)array->ptr + array->len * element_size;
        memcpy(spot, element, element_size);
    }
    array->len++;
}

#endif // IMPL_PLATFORM
