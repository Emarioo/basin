
/*
    Properties of this bucket array implementation

        Stable pointers to elements (no reallocation of the element data)

        Cannot insert or remove element in the middle, only push and pop at the end
*/

#pragma once

#include "platform/platform.h"


#define DEF_BUCKET_ARRAY(T)    \
typedef struct {               \
    T* elements;               \
} Bucket_##T;                  \
typedef struct {               \
    Bucket_##T* buckets;       \
    int buckets_cap;           \
    int element_count;         \
    int items_per_bucket;      \
} BucketArray_##T;

typedef struct {
    void* elements;
} Bucket;
typedef struct {
    Bucket* buckets;
    int buckets_cap;
    int element_count;
    int items_per_bucket;
} BucketArray;

#define barray_init(ARR, INITIAL_CAP, ITEMS_PER_BUCKET) _barray_init((BucketArray*)(ARR), sizeof((ARR)->buckets[0].elements[0]), (INITIAL_CAP), (ITEMS_PER_BUCKET))
#define barray_cleanup(ARR) _barray_cleanup((BucketArray*)(ARR), sizeof((ARR)->buckets[0].elements[0]))
#define barray_push(ARR, ELEMENTP) _barray_push((BucketArray*)(ARR), sizeof((ARR)->buckets[0].elements[0]), (false && ((ARR)->buckets[0].elements[0] = *(ELEMENTP), false), (ELEMENTP)))
#define barray_pop(ARR, ELEMENTP) _barray_pop((BucketArray*)(ARR), sizeof((ARR)->buckets[0].elements[0]), (false && ((ARR)->buckets[0].elements[0] = *(ELEMENTP), false), (ELEMENTP)))
#define barray_get(ARR, INDEX) _barray_get((BucketArray*)(ARR), sizeof((ARR)->buckets[0].elements[0]), (INDEX))
#define barray_count(ARR) ((ARR)->element_count)

void _barray_init(BucketArray* array, int element_size, int initial_cap, int items_per_bucket);
void _barray_cleanup(BucketArray* array, int element_size);

void* _barray_push(BucketArray* array, int element_size, void* element);
void _barray_pop(BucketArray* array, int element_size, void* out_element);
void* _barray_get(BucketArray* array, int element_size, int index);

#ifdef IMPL_PLATFORM

void _barray_init(BucketArray* array, int element_size, int initial_cap, int items_per_bucket) {
    memset(array, 0, sizeof(*array));
    array->buckets = (Bucket*)mem__alloc(initial_cap * sizeof(Bucket));
    // array->buckets_len = 0;
    array->buckets_cap = (initial_cap + items_per_bucket-1) / items_per_bucket;

    // memset(array->buckets, 0, initial_max_buckets * sizeof(Bucket));
    
    for (int i=0; i < array->buckets_cap; i++) {
        Bucket* bucket = &array->buckets[i];
        bucket->elements = mem__alloc(items_per_bucket * element_size);
        memset(bucket->elements, 0, items_per_bucket * element_size);
    }

    array->element_count = 0;
    array->items_per_bucket = items_per_bucket;
}
void _barray_cleanup(BucketArray* array, int element_size) {
    for(int i=0;i<array->buckets_cap;i++) {
        Bucket* bucket = &array->buckets[i];
        mem__free(bucket->elements);
    }
    mem__free(array->buckets);

    array->buckets = NULL;
    // array->buckets_len = 0;
    array->buckets_cap = 0;
    array->element_count = 0;
    // keep items_per_bucket
}

void* _barray_push(BucketArray* array, int element_size, void* element) {
    int element_cap = array->items_per_bucket * array->buckets_cap;

    if(array->element_count >= element_cap) {
        int new_bucket_cap = array->buckets_cap * 2 + 2;
        Bucket* new_buckets = (Bucket*)mem__realloc(new_bucket_cap * sizeof(Bucket), array->buckets);
       
        // memset(new_buckets + array->buckets_max, 0, (new_max - array->buckets_max) * sizeof(Bucket));

        for (int i = array->buckets_cap; i < new_bucket_cap; i++) {
            Bucket* bucket = &array->buckets[i];
            bucket->elements = mem__alloc(array->items_per_bucket * element_size);
            memset(bucket->elements, 0, array->items_per_bucket * element_size);
        }

        array->buckets = new_buckets;
        array->buckets_cap = new_bucket_cap;
    }
    
    int bucket_index = array->element_count / array->items_per_bucket;
    int element_index = array->element_count % array->items_per_bucket;

    void* ptr = (char*)array->buckets[bucket_index].elements + element_index * element_size;

    memcpy(ptr, element, element_size);

    array->element_count++;
    return ptr;
}

void _barray_pop(BucketArray* array, int element_size, void* out_element) {
    array->element_count--;
    if(out_element) {
        int index = array->element_count;
        int bucket_index = index / array->items_per_bucket;
        int element_index = index % array->items_per_bucket;
        
        memcpy(out_element, (char*)array->buckets[bucket_index].elements + element_index * element_size, element_size);
    }
}

void* _barray_get(BucketArray* array, int element_size, int index) {
    ASSERT(index < array->element_count);

    int bucket_index = index / array->items_per_bucket;
    int element_index = index % array->items_per_bucket;

    return (char*)array->buckets[bucket_index].elements + element_index * element_size;
}



#endif // IMPL_PLATFORM