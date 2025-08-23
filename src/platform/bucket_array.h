
/*
    Properties of this bucket array implementation

        Stable pointers to elements (no reallocation of the element data)

        Cannot insert or remove element in the middle, only push and pop at the end
*/

#pragma once

#include "platform/memory.h"


#define DEF_BUCKET_ARRAY(T)                                                                             \
typedef struct {                                                                                        \
    BucketArray _internal;                                                                              \
} BucketArray_##T;                                                                                      \
static inline void barray_init(BucketArray_##T* array, int items_per_bucket, int initial_max_buckets);  \
static inline void barray_cleanup(BucketArray_##T* array);                                              \
static inline void barray_push(BucketArray_##T* array, void* element);                                  \
static inline void barray_pop(BucketArray_##T* array);                                                  \
static inline T* barray_get(BucketArray_##T* array, int index);

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

void barray_push(BucketArray* array, int element_size, void* element);
void barray_pop(BucketArray* array, int element_size);
void* barray_get(BucketArray* array, int element_size, int index);

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

void barray_push(BucketArray* array, int element_size, void* element) {
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

    memcpy((char*)array->buckets[bucket_index].elements + element_index * element_size, element, element_size);

    array->element_count++;
}

void barray_pop(BucketArray* array, int element_size) {
    array->element_count--;
}

void* barray_get(BucketArray* array, int element_size, int index) {
    ASSERT(index < array->element_count);

    int bucket_index = index / array->items_per_bucket;
    int element_index = index % array->items_per_bucket;

    return (char*)array->buckets[bucket_index].elements + element_index * element_size;
}


#endif // IMPL_PLATFORM