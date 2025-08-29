#pragma once

// #include <stdatomic.h>

typedef struct {
    u64 handle;
    u32 id;
} Thread;

typedef struct {
    u64 handle;
} Mutex;

typedef struct {
    u64 handle;
} Semaphore;

typedef  u32(*ThreadRoutine)(void*);

void spawn_thread(Thread* thread, ThreadRoutine func, void* arg);
void join_thread(Thread* thread);
bool is_thread_joinable(Thread* thread);
u32 current_thread_id();

void create_mutex(Mutex* mutex);
void lock_mutex(Mutex* mutex);
void unlock_mutex(Mutex* mutex);
void cleanup_mutex(Mutex* mutex);

void create_semaphore(Semaphore* semaphore, u32 initial, u32 max_locks);
void wait_semaphore(Semaphore* semaphore);
bool signal_semaphore(Semaphore* semaphore, int count);
void cleanup_semaphore(Semaphore* semaphore);

// returns previous value
#define atomic_add(PTR, VAL) __atomic_fetch_add(PTR, VAL, __ATOMIC_SEQ_CST)
// returns previous value
#define atomic_add64(PTR, VAL) __atomic_fetch_add(PTR, VAL, __ATOMIC_SEQ_CST)

#ifdef IMPL_PLATFORM

#include "windows.h"

void spawn_thread(Thread* thread, ThreadRoutine func, void* arg) {
    // #if OS_WINDOWS
        u32 thread_id;
        // Unsafe casting routine pointer?
        HANDLE handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, arg, 0, (DWORD*)&thread_id);

        ASSERT(handle != INVALID_HANDLE_VALUE);

        thread->handle = (u64)handle;
        thread->id = thread_id;
    // #else
    // #endif
}
void join_thread(Thread* thread) {
    // #ifdef OS_WINDOWS
    u32 res = WaitForSingleObject((HANDLE)thread->handle, INFINITE);
    ASSERT(res != WAIT_FAILED);
    
    u32 yes = CloseHandle((HANDLE)thread->handle);
    ASSERT(yes);
    // #else
    // #endif
}
bool is_thread_joinable(Thread* thread) {
    return thread->id != 0 && thread->id != current_thread_id();
}
u32 current_thread_id() {
    // #ifdef OS_WINDOWS
        return GetCurrentThreadId();
    // #else
    // #endif
}

void create_mutex(Mutex* mutex) {
    HANDLE handle = CreateMutex(NULL, false, NULL);
    ASSERT(handle != INVALID_HANDLE_VALUE);
    mutex->handle = (u64)handle;
}
void lock_mutex(Mutex* mutex) {
    u32 res = WaitForSingleObject((HANDLE)mutex->handle, INFINITE);
    ASSERT(res != WAIT_FAILED);
}
void unlock_mutex(Mutex* mutex) {
    bool yes = ReleaseMutex((HANDLE)mutex->handle);
    ASSERT(yes);
}
void cleanup_mutex(Mutex* mutex) {
    bool yes = CloseHandle((HANDLE)mutex->handle);
    mutex->handle = 0;
}

void create_semaphore(Semaphore* semaphore, u32 initial, u32 max_locks) {
    HANDLE handle = CreateSemaphore(NULL, initial, max_locks, NULL);
    ASSERT(handle != INVALID_HANDLE_VALUE);
    semaphore->handle = (u64)handle;
}
void wait_semaphore(Semaphore* semaphore) {
    u32 res = WaitForSingleObject((HANDLE)semaphore->handle, INFINITE);
    ASSERT(res != WAIT_FAILED);
}
bool signal_semaphore(Semaphore* semaphore, int count) {
    bool yes = ReleaseSemaphore((HANDLE)semaphore->handle, count, NULL);
    return yes;
}
void cleanup_semaphore(Semaphore* semaphore) {
    bool yes = CloseHandle((HANDLE)semaphore->handle);
    semaphore->handle = 0;
}

#endif // IMPL_PLATFORM