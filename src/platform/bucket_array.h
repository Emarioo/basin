
/*
    Properties of this bucket array implementation

        Stable pointers to elements (no reallocation of the element data)

        Cannot insert or remove element in the middle, only push and pop at the end
*/

#pragma once

#include "platform/memory.h"


#define DEF_BUCKET_ARRAY(T)                                                                                 \
typedef struct {                                                                                            \
    BucketArray _internal;                                                                                  \
} BucketArray_##T;                                                                                          \
static inline void barray_init_##T(BucketArray_##T* array, int items_per_bucket, int initial_max_buckets)   \
    { barray_init((BucketArray*)array, sizeof(T), items_per_bucket, initial_max_buckets); }                 \
static inline void barray_cleanup_##T(BucketArray_##T* array)                                               \
    { barray_cleanup((BucketArray*)array, sizeof(T)); }                                                     \
static inline T* barray_push_##T(BucketArray_##T* array, void* element)                                     \
    { return barray_push((BucketArray*)array, sizeof(T), element); }                                        \
static inline void barray_pop_##T(BucketArray_##T* array, void* out_element)                                \
    { barray_pop((BucketArray*)array, sizeof(T), out_element); }                                            \
static inline T* barray_get_##T(BucketArray_##T* array, int index)                                          \
    { return barray_get((BucketArray*)array, sizeof(T), index); }                                           \
static inline int barray_count_##T(BucketArray_##T* array)                                                  \
    { barray_count((BucketArray*)array, sizeof(T)); }

typedef struct {
    void* elements;
    // bucket array knows the capacity of the buckets
} Bucket;

typedef struct {
    Bucket* buckets;
    int buckets_len;
    int buckets_max;

    int element_count;
    int items_per_bucket;
} BucketArray;

void barray_init(BucketArray* array, int element_size, int items_per_bucket, int initial_max_buckets);
void barray_cleanup(BucketArray* array, int element_size);

void* barray_push(BucketArray* array, int element_size, void* element);
void barray_pop(BucketArray* array, int element_size, void* out_element);
void* barray_get(BucketArray* array, int element_size, int index);
int barray_count(BucketArray* array, int element_size);

#ifdef IMPL_PLATFORM

void barray_init(BucketArray* array, int element_size, int items_per_bucket, int initial_max_buckets) {
    array->buckets = (Bucket*)heap_alloc(initial_max_buckets * sizeof(Bucket));
    array->buckets_len = 0;
    array->buckets_max = initial_max_buckets;

    // memset(array->buckets, 0, initial_max_buckets * sizeof(Bucket));
    
    for (int i=0; i < array->buckets_max; i++) {
        Bucket* bucket = &array->buckets[i];
        bucket->elements = heap_alloc(items_per_bucket * element_size);
        memset(bucket->elements, 0, items_per_bucket * element_size);
    }

    array->element_count = 0;
    array->items_per_bucket = items_per_bucket;
}
void barray_cleanup(BucketArray* array, int element_size) {
    for(int i=0;i<array->buckets_len;i++) {
        Bucket* bucket = &array->buckets[i];
        heap_free(bucket->elements);
    }
    heap_free(array->buckets);

    array->buckets = NULL;
    array->buckets_len = 0;
    array->buckets_max = 0;
    array->element_count = 0;
    // keep items_per_bucket
}

void* barray_push(BucketArray* array, int element_size, void* element) {
    int element_max = array->items_per_bucket * array->buckets_max;

    if(array->element_count >= element_max) {
        int new_max = array->buckets_max * 2 + 2;
        Bucket* new_buckets = (Bucket*)heap_realloc(array->buckets, new_max * sizeof(Bucket));
       
        // memset(new_buckets + array->buckets_max, 0, (new_max - array->buckets_max) * sizeof(Bucket));

        for (int i = array->buckets_max; i < new_max; i++) {
            Bucket* bucket = &array->buckets[i];
            bucket->elements = heap_alloc(array->items_per_bucket * element_size);
            memset(bucket->elements, 0, array->items_per_bucket * element_size);
        }

        array->buckets = new_buckets;
        array->buckets_max = new_max;
    }
    
    int bucket_index = array->element_count / array->items_per_bucket;
    int element_index = array->element_count % array->items_per_bucket;

    void* ptr = (char*)array->buckets[bucket_index].elements + element_index * element_size;

    memcpy(ptr, element, element_size);

    array->element_count++;
    return ptr;
}

void barray_pop(BucketArray* array, int element_size, void* out_element) {
    array->element_count--;
    if(out_element) {
        int index = array->element_count;
        int bucket_index = index / array->items_per_bucket;
        int element_index = index % array->items_per_bucket;
        
        memcpy(out_element, (char*)array->buckets[bucket_index].elements + element_index * element_size, element_size);
    }
}

void* barray_get(BucketArray* array, int element_size, int index) {
    ASSERT(index < array->element_count);

    int bucket_index = index / array->items_per_bucket;
    int element_index = index % array->items_per_bucket;

    return (char*)array->buckets[bucket_index].elements + element_index * element_size;
}

int barray_count(BucketArray* array, int element_size) {
    return array->element_count;
}


#endif // IMPL_PLATFORM