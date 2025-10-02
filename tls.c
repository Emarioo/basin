#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

typedef struct {
    int x;
} Profiler;

// thread_local() Profiler profiler;


// int work(void* arg) {
//     for(int i=0;i<100000;i++) {
//         profiler.x += 1;
//     }
//     fprintf(stderr, "w %d\n", profiler.x);
// }

int tls_index;

static inline Profiler* get_prof() {
    void* result;
    asm(
        "movq %%gs:0x58, %%rax\n"
        "movq (%%rax,%1,8), %%rax\n"
        : "=&a" (result)
        : "r" ((unsigned long long) tls_index)
        : "memory"
    );
    return (Profiler*)result;
}


static inline void set_prof(Profiler* profiler) {
    asm(
        "movq %%gs:0x58, %%rax\n"
        "movq %0, (%%rax,%1,8)\n"
        :
        : "r" (profiler), "r" ((unsigned long long) tls_index)
        : "%rax", "memory"
    );
}

Profiler* spawn() {
    return  get_prof();
}

int init() {
    tls_index = TlsAlloc();
    set_prof((Profiler*)0x1000);


    // for(int i=0;i<256;i++) {
    //     printf("%d\n", num);
    // }   

    // HANDLE threads[32];
    // for(int i=0;i<32;i++) {
    //     threads[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)work, NULL, 0, 0);
    // }
    // for(int i=0;i<32;i++) {
    //     WaitForSingleObject(threads[i], INFINITE);
    //     CloseHandle(threads[i]);
    // }

    return 0;
}