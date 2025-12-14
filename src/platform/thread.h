#pragma once

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

void spawn_thread(Thread* thread, ThreadRoutine func, void* arg);
void join_thread(Thread* thread);
bool is_thread_joinable(Thread* thread);
u64 current_thread_id();

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

#ifdef OS_WINDOWS
    #include "windows.h"
#else
    #include <pthread.h>
    #include <semaphore.h>
    #include <errno.h>
#endif

void spawn_thread(Thread* thread, ThreadRoutine func, void* arg) {
    #ifdef OS_WINDOWS
        u32 thread_id;
        // Unsafe casting routine pointer?
        HANDLE handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, arg, 0, (DWORD*)&thread_id);

        ASSERT(handle != INVALID_HANDLE_VALUE);

        thread->handle = (u64)handle;
        thread->id = thread_id;
    #else
        int res;
        u64 thread_id;
        res = pthread_create((pthread_t*)&thread_id, NULL, (void *(*) (void *))func, arg);

        ASSERT(res == 0);
        
        thread->handle = thread_id;
        thread->id = thread_id;
        ASSERT(thread_id != 0); // 0 represents invalid thread in is_thread_joinable
    #endif
}
void join_thread(Thread* thread) {
    #ifdef OS_WINDOWS
        u32 res = WaitForSingleObject((HANDLE)thread->handle, INFINITE);
        ASSERT(res != WAIT_FAILED);
        
        u32 yes = CloseHandle((HANDLE)thread->handle);
        ASSERT(yes);
    #else
        int res;
        res = pthread_join(thread->handle, NULL);
        ASSERT(res == 0);
    #endif
}
bool is_thread_joinable(Thread* thread) {
    return thread->id != 0 && thread->id != current_thread_id();
}
u64 current_thread_id() {
    #ifdef OS_WINDOWS
        return (u64)GetCurrentThreadId();
    #else
        return (u64)pthread_self();
    #endif
}

#ifdef OS_LINUX
    #define MAX_MUTEX 200
    static bool g_mutex_array_used[MAX_MUTEX];
    static pthread_mutex_t g_mutex_array[MAX_MUTEX];
    static volatile int g_next_mutex = 0;
#endif

void create_mutex(Mutex* mutex) {
    #ifdef OS_WINDOWS
        HANDLE handle = CreateMutex(NULL, false, NULL);
        ASSERT(handle != INVALID_HANDLE_VALUE);
        mutex->handle = (u64)handle;
    #else
        int res;
        int index;
        int limit = MAX_MUTEX;
        while (true) {
            index = __atomic_fetch_add(&g_next_mutex, 1, __ATOMIC_SEQ_CST);
            if (!g_mutex_array_used[index % MAX_MUTEX]) {
                g_mutex_array_used[index % MAX_MUTEX] = true;
                break;
            }
            limit--;
            ASSERT(limit > 0); // increase max mutex count or allocate mutex on heap when array is no longer enough.
        }
        res = pthread_mutex_init(&g_mutex_array[index % MAX_MUTEX], NULL);
        ASSERT(res == 0);
        mutex->handle = (u64)&g_mutex_array[index % MAX_MUTEX];
    #endif
}
void lock_mutex(Mutex* mutex) {
    #ifdef OS_WINDOWS
        u32 res = WaitForSingleObject((HANDLE)mutex->handle, INFINITE);
        ASSERT(res != WAIT_FAILED);
    #else
        int res;
        res = pthread_mutex_lock((pthread_mutex_t*)mutex->handle);
        ASSERT(res == 0);
    #endif
}
bool trylock_mutex(Mutex* mutex) {
    #ifdef OS_WINDOWS
        u32 res = WaitForSingleObject((HANDLE)mutex->handle, 0);
        if (res == WAIT_TIMEOUT)
            return false;
        ASSERT(res != WAIT_FAILED);
        return true;
    #else
        int res;
        res = pthread_mutex_trylock((pthread_mutex_t*)mutex->handle);
        if (res == 0)
            return true;
        if (res == EBUSY)
            return false;
        ASSERT(res == 0 || res == EBUSY);
        return false;
    #endif
}
void unlock_mutex(Mutex* mutex) {
    #ifdef OS_WINDOWS
        bool yes = ReleaseMutex((HANDLE)mutex->handle);
        ASSERT(yes);
    #else
        int res;
        res = pthread_mutex_unlock((pthread_mutex_t*)mutex->handle);
        ASSERT(res == 0);
    #endif
}
void cleanup_mutex(Mutex* mutex) {
    #ifdef OS_WINDOWS
        bool yes = CloseHandle((HANDLE)mutex->handle);
        mutex->handle = 0;
    #else
        int res;
        res = pthread_mutex_destroy((pthread_mutex_t*)mutex->handle);
        ASSERT(res == 0);
        int index = (mutex->handle - (u64)g_mutex_array) / sizeof(*g_mutex_array);
        mutex->handle = 0;
        g_mutex_array_used[index] = 0;
    #endif
}


#ifdef OS_LINUX
    #define MAX_SEMAPHORE 200
    static bool g_semaphore_array_used[MAX_SEMAPHORE];
    static sem_t g_semaphore_array[MAX_SEMAPHORE];
    static volatile int g_next_semaphore = 0;
#endif


void create_semaphore(Semaphore* semaphore, u32 initial, u32 max_locks) {
    #ifdef OS_WINDOWS
        HANDLE handle = CreateSemaphore(NULL, initial, max_locks, NULL);
        ASSERT(handle != INVALID_HANDLE_VALUE);
        semaphore->handle = (u64)handle;
    #else
        int res;
        int index;
        int limit = MAX_SEMAPHORE;
        while (true) {
            index = __atomic_fetch_add(&g_next_semaphore, 1, __ATOMIC_SEQ_CST);
            if (!g_semaphore_array_used[index % MAX_SEMAPHORE]) {
                g_semaphore_array_used[index % MAX_SEMAPHORE] = true;
                break;
            }
            limit--;
            ASSERT(limit > 0); // increase max mutex count or allocate mutex on heap when array is no longer enough.
        }
        // @TODO max_locks is unused on Linux?
        res = sem_init(&g_semaphore_array[index % MAX_MUTEX], 0, initial);
        ASSERT(res == 0);
        semaphore->handle = (u64)&g_semaphore_array[index % MAX_MUTEX];
    #endif
}
void wait_semaphore(Semaphore* semaphore) {
    #ifdef OS_WINDOWS
        u32 res = WaitForSingleObject((HANDLE)semaphore->handle, INFINITE);
        ASSERT(res != WAIT_FAILED);
    #else
        int res;
        res = sem_wait((sem_t*)semaphore->handle);
        ASSERT(res == 0);
    #endif
}
bool signal_semaphore(Semaphore* semaphore, int count) {
    #ifdef OS_WINDOWS
        bool yes = ReleaseSemaphore((HANDLE)semaphore->handle, count, NULL);
        return yes;
    #else
        int res;
        res = sem_post((sem_t*)semaphore->handle);
        ASSERT(res == 0);
        return true;
    #endif
}
void cleanup_semaphore(Semaphore* semaphore) {
    #ifdef OS_WINDOWS
        bool yes = CloseHandle((HANDLE)semaphore->handle);
        semaphore->handle = 0;
    #else
        int res;
        res = sem_destroy((sem_t*)semaphore->handle);
        ASSERT(res == 0);
        int index = (semaphore->handle - (u64)g_semaphore_array) / sizeof(*g_semaphore_array);
        semaphore->handle = 0;
        g_semaphore_array_used[index] = 0;
    #endif
}

#endif // IMPL_PLATFORM