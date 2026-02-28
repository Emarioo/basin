#include "platform/platform.h"

#ifdef OS_WINDOWS
    #define WIN32_MEAN_AND_LEAN
    #include "Windows.h"
    #include <stdarg.h>
    #include <stdio.h>
    #include <string.h>
    #include <stdlib.h>
#endif
#ifdef OS_LINUX
    #include <unistd.h>
    #include "sys/mman.h"
    #include "linux/limits.h"
    #include <stdarg.h>
    #include <stdio.h>
    #include <string.h>
    #include <stdlib.h>
    #include <time.h>
    #include <errno.h>
#endif


// #define platform_log(...) log__printf(__VA_ARGS__)
#define platform_log(...)

#define ASSERT(E) ( (E) ? 0 : (log__printf("[ASSERT] %s:%d %s\n", __FILE__, __LINE__, #E), *((int*)0) = 5) )

// ##########################
//      File System
// ##########################

#if defined(OS_WINDOWS) || defined(OS_LINUX)
    #define MAX_FILE_HANDLES 100
    static FILE* handles[MAX_FILE_HANDLES];
    static int handles_len;
#endif

FSHandle fs__open(const char* path, uint32_t flags) {
    #if defined(OS_WINDOWS) || defined(OS_LINUX)
        FILE* file;
        if (flags & FS_WRITE) {
            file = fopen(path, "wb");
        } else if (flags & FS_READ) {
            file = fopen(path, "rb");
        }
        if (!file)
            return FS_INVALID_HANDLE;


        // @TODO Atomic operation
        if (handles_len >= MAX_FILE_HANDLES)
            return FS_INVALID_HANDLE;
        FSHandle handle = handles_len;
        handles[handles_len] = file;
        handles_len++;

        platform_log("Open [%d] = %s, %u\n", (int)handle, path, flags);
        return handle;
    #endif
}
void fs__close(FSHandle handle) {
    #if defined(OS_WINDOWS) || defined(OS_LINUX)
        FILE* file = handles[handle];

        int res = fclose(file);

        // @TODO Atomic operation
        handles[handle] = NULL;
        while (handles_len > 0 && handles[handles_len-1] == NULL) {
            handles_len--;
        }
        platform_log("Close [%d]\n", (int)handle);
    #endif
}

void fs__info(FSHandle handle, FSInfo* info) {
    #if defined(OS_WINDOWS) || defined(OS_LINUX)
        FILE* file = handles[handle];
        
        size_t cur_pos = ftell(file);
        fseek(file, 0, SEEK_END);
        size_t file_size = ftell(file);
        fseek(file, cur_pos, SEEK_SET);

        info->file_size = file_size;
        info->is_directory = false;

        platform_log("FSInfo [%d] = %u\n", (int)handle, (unsigned)file_size);
    #endif
}
uint64_t fs__read(FSHandle handle, uint64_t offset, void* buffer, uint64_t size) {
    #if defined(OS_WINDOWS) || defined(OS_LINUX)
        FILE* file = handles[handle];
        fseek(file, offset, SEEK_SET);
        size_t res = fread(buffer, 1, size, file);
        
        platform_log("Read [%d] = %u\n", (int)handle, (unsigned)res);
        return res;
    #endif
}
uint64_t fs__write(FSHandle handle, uint64_t offset, const void* buffer, uint64_t size) {
    #if defined(OS_WINDOWS) || defined(OS_LINUX)
        FILE* file = handles[handle];
        fseek(file, offset, SEEK_SET);
        size_t res = fwrite(buffer, 1, size, file);
        platform_log("Write [%d] %d, %d = written %u\n", (int)handle, (int)offset, (int)size, (unsigned)res);
        return res;
    #endif
}

void fs__abspath(const char* path, int out_path_cap, char* out_path) {
    #if defined(OS_WINDOWS)
        DWORD length = GetFullPathNameA(path, out_path_cap, out_path, NULL);
        if (length == 0) {
            out_path[0] = '\0';
            return;
        }
        for (int i=0;i<length;i++) {
            if (out_path[i] == '\\') {
                out_path[i] = '/';
            }
        }
    #elif defined(OS_LINUX)
        if (out_path_cap < PATH_MAX + 1) {
            char temp_path[PATH_MAX+1];
            char* res = realpath(path, temp_path);
            if (!res)
                out_path[0] = '\0';
            else
                memcpy(out_path, temp_path, strlen(temp_path) + 1);
        } else {
            char* res = realpath(path, out_path);
            if (!res)
                out_path[0] = '\0';
        }
    #endif
}

void fs__exepath(int out_path_cap, char* out_path) {
    #if defined(OS_WINDOWS)
        DWORD length = GetModuleFileNameA(NULL, out_path, out_path_cap);
        if (length == 0) {
            out_path[0] = '\0';
            return;
        }
        for (int i=0;i<length;i++) {
            if (out_path[i] == '\\') {
                out_path[i] = '/';
            }
        }
    #elif defined(OS_LINUX)
		int len = readlink("/proc/self/exe", out_path, out_path_cap);
        if (len < 0) {
            out_path[0] = '\0';
            return;
        }
    #endif
}

bool fs__exists(const char* path) {
    #if defined(OS_WINDOWS)
        DWORD attr = GetFileAttributesA(path);
        return attr != 0 && attr != INVALID_FILE_ATTRIBUTES;
    #elif defined(OS_LINUX)
        return access(path, F_OK) == 0;
    #endif
}

// ##########################
//      Memory
// ##########################

void* mem__allocate(uint64_t size, void* old_ptr) {
    if (size == 0) {
        free(old_ptr);
        return NULL;
    } else if (!old_ptr) {
        return malloc(size);
    }
    return realloc(old_ptr, size);
}

void* mem__map(void* address, uint64_t size, int flags) {
    #ifdef OS_WINDOWS
        return VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_READWRITE);
    #endif
    #ifdef OS_LINUX
        int page_size = getpagesize();
        uint64_t aligned_size = size % page_size == 0 ? size : size + (page_size - size) % page_size;
        void* ptr = mmap(NULL, aligned_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
        if (ptr == (void*)-1) {
            log__printf("barf: mmap failed, %s\n", strerror(errno));
            return NULL;
        }
        return ptr;
    #endif
}
void mem__mapflag(void* address, uint64_t size, int flags) {
    #ifdef OS_WINDOWS
        int mem_flags;
        if ((flags & MEM_EXEC) && (flags & MEM_WRITE)) {
            mem_flags = PAGE_EXECUTE_READWRITE;
        } else if ((flags & MEM_EXEC)) {
            mem_flags = PAGE_EXECUTE;
        } else if ((flags & MEM_WRITE)) {
            mem_flags = PAGE_READWRITE;
        } else {
            mem_flags = PAGE_READONLY;
        }
        DWORD prev_flags;
        BOOL res = VirtualProtect(address, size, mem_flags, &prev_flags);
        if (!res) {
            DWORD val = GetLastError();
            log__printf("flag_memory: win error %u\n", (unsigned)val);
        }
    #endif
    #ifdef OS_LINUX
        int mem_flags = PROT_READ;
        if ((flags & MEM_EXEC)) {
            mem_flags |= PROT_EXEC;
        } else if ((flags & MEM_WRITE)) {
            mem_flags |= PROT_WRITE;
        }
        int page_size = getpagesize();
        uint64_t aligned_size = size % page_size == 0 ? size : size + (page_size - size) % page_size;
        int res = mprotect(address, aligned_size, mem_flags);
        if (res < 0) {
            log__printf("barf: mprotect failed, %s\n", strerror(errno));
        }
    #endif
}
void mem__unmap(void* address, uint64_t size) {
    #ifdef OS_WINDOWS
        VirtualFree(address, size, MEM_RELEASE);
    #endif
    #ifdef OS_LINUX
        munmap(address, size);
    #endif
}


// ##########################
//      Debug/logging
// ##########################

void log__printf(const char* format, ...) {
    va_list va;
    va_start(va, format);
    vfprintf(stderr, format, va);
    va_end(va);
}



// #############################
//       Threads
// #############################



#ifdef OS_WINDOWS
    #include "windows.h"
#else
    #include <pthread.h>
    #include <semaphore.h>
    #include <errno.h>
#endif

void thread__spawn(Thread* thread, ThreadRoutine func, void* arg) {
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
void thread__join(Thread* thread) {
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
bool thread__joinable(Thread* thread) {
    return thread->id != 0 && thread->id != thread__current_id();
}
u64 thread__current_id() {
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

void thread__create_mutex(Mutex* mutex) {
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
void thread__lock_mutex(Mutex* mutex) {
    #ifdef OS_WINDOWS
        u32 res = WaitForSingleObject((HANDLE)mutex->handle, INFINITE);
        ASSERT(res != WAIT_FAILED);
    #else
        int res;
        res = pthread_mutex_lock((pthread_mutex_t*)mutex->handle);
        ASSERT(res == 0);
    #endif
}
bool thread__trylock_mutex(Mutex* mutex) {
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
void thread__unlock_mutex(Mutex* mutex) {
    #ifdef OS_WINDOWS
        bool yes = ReleaseMutex((HANDLE)mutex->handle);
        ASSERT(yes);
    #else
        int res;
        res = pthread_mutex_unlock((pthread_mutex_t*)mutex->handle);
        ASSERT(res == 0);
    #endif
}
void thread__cleanup_mutex(Mutex* mutex) {
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


void thread__create_semaphore(Semaphore* semaphore, u32 initial, u32 max_locks) {
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
void thread__wait_semaphore(Semaphore* semaphore) {
    #ifdef OS_WINDOWS
        u32 res = WaitForSingleObject((HANDLE)semaphore->handle, INFINITE);
        ASSERT(res != WAIT_FAILED);
    #else
        int res;
        res = sem_wait((sem_t*)semaphore->handle);
        ASSERT(res == 0);
    #endif
}
bool thread__signal_semaphore(Semaphore* semaphore, int count) {
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
void thread__cleanup_semaphore(Semaphore* semaphore) {
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


void thread__sleep_ns(u64 ns) {
    #ifdef OS_WINDOWS
        Sleep(ns / 1000000LLU);
    #else
        struct timespec ts;
        ts.tv_sec = ns / 1000000000LLU;
        ts.tv_nsec = ns % 1000000000LLU;
        nanosleep(&ts, NULL);
    #endif
}




//##############################
//      SYSTEM INFORMATION
//##############################


int sys__cpu_count() {
    #ifdef OS_WINDOWS
        GetSystemInfo(&si);
        return si.dwNumberOfProcessors;
    #else
        long num_logical = sysconf(_SC_NPROCESSORS_ONLN);
        return num_logical;
    #endif
}