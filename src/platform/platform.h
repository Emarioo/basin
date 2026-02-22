#pragma once

#include <stdint.h>
#include <stdbool.h>



// ##########################
//      File System
// ##########################

typedef uint32_t FSHandle;

#define FS_READ  0x1
#define FS_WRITE 0x2
#define FS_INVALID_HANDLE 0xFFFFFFFF
typedef struct {
    uint64_t file_size;
    bool is_directory;
} FSInfo;

FSHandle fs__open(const char* path, uint32_t flags);
void fs__close(FSHandle handle);

void fs__info(FSHandle handle, FSInfo* info);

uint64_t fs__read(FSHandle handle, uint64_t offset, void* buffer, uint64_t size);
uint64_t fs__write(FSHandle handle, uint64_t offset, void* buffer, uint64_t size);

void fs__abspath(const char* path, int out_path_cap, char* out_path);
void fs__exepath(int out_path_cap, char* out_path);
bool fs__exists(const char* path);

// @TODO Iterate directory, recursively

// ##########################
//      Memory
// ##########################

// allocate:    ptr = mem_alloc(4096, NULL)
// reallocate:  ptr = mem_alloc(4096, ptr)
// free:        mem_alloc(0, ptr)
void* mem__allocate(uint64_t size, void* old_ptr);
#define mem__alloc(SIZE) mem__allocate(SIZE, NULL)
#define mem__realloc(SIZE, PTR) mem__allocate(SIZE, PTR)
#define mem__free(PTR) mem__allocate(0, PTR)

#define MEM_READ  0x1
#define MEM_WRITE 0x2
#define MEM_EXEC  0x4

void* mem__map(void* address, uint64_t size, int flags);
void  mem__mapflag(void* address, uint64_t size, int flags);
void  mem__unmap(void* address, uint64_t size);


#define HEAP_ALLOC_OBJECT(T) (T*)_heap_alloc_object(sizeof(T));
static inline void* _heap_alloc_object(const int size) {
    void* ptr = mem__allocate(size, NULL);
    memset(ptr, 0, size);
    return ptr;
}

#define HEAP_ALLOC_ARRAY(T,N) (T*)mem__allocate(sizeof(T) * (N), NULL)



// ##########################
//      Debug/logging
// ##########################

void log__printf(const char* format, ...);



// #############################
//       Threads
// #############################


// #include <stdatomic.h>

typedef struct {
    u64 handle;
    u64 id; // id on windows is 32-bit, on linux id == handle
} Thread;

typedef struct {
    u64 handle; // pthread_mutex_t* on Linux
} Mutex;

typedef struct {
    u64 handle;
} Semaphore;

typedef  u32(*ThreadRoutine)(void*);

void thread__spawn(Thread* thread, ThreadRoutine func, void* arg);
void thread__join(Thread* thread);
bool thread__joinable(Thread* thread);
u64 thread__current_id();
void thread__sleep_ns(u64 ns);

void thread__create_mutex(Mutex* mutex);
void thread__lock_mutex(Mutex* mutex);
void thread__unlock_mutex(Mutex* mutex);
void thread__cleanup_mutex(Mutex* mutex);

void thread__create_semaphore(Semaphore* semaphore, u32 initial, u32 max_locks);
void thread__wait_semaphore(Semaphore* semaphore);
bool thread__signal_semaphore(Semaphore* semaphore, int count);
void thread__cleanup_semaphore(Semaphore* semaphore);

// returns previous value
#define atomic_add(PTR, VAL) __atomic_fetch_add(PTR, VAL, __ATOMIC_SEQ_CST)
// returns previous value
#define atomic_add64(PTR, VAL) __atomic_fetch_add(PTR, VAL, __ATOMIC_SEQ_CST)

