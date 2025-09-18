/*
    Driver is the glue in the compiler. It manages threads, imports, memory, and steps to perform such as lexing, parsing, code gen
*/

#pragma once

#include "basin/types.h"
#include "basin/core/lexer.h"

#include "platform/array.h"
#include "platform/bucket_array.h"
#include "platform/thread.h"

typedef enum {
    TASK_INVALID,
    TASK_TOKENIZE,
    TASK_PARSE,
    TASK_COUNT,
} TaskKind;
extern const char* const task_kind_names[TASK_COUNT];

typedef struct {
    TaskKind kind;
    union {
        struct {
            Import* import;
            // string text;
        } tokenize;
        struct {
            Import* import;
            // string text;
            TokenStream* stream;
        } parse;
    };
} Task;


DEF_BUCKET_ARRAY(Task)
DEF_BUCKET_ARRAY(Import)

typedef struct Driver Driver;

typedef struct {
    Driver* driver;
    Thread thread;
} DriverThread;

typedef struct Driver {
    BucketArray_Task tasks;
    BucketArray_Import imports;

    Mutex import_mutex;
    Mutex task_mutex;
    Semaphore may_have_task_semaphore;

    volatile u32 idle_threads;

    ImportID next_import_id;

    DriverThread* threads;
    int threads_len;
} Driver;

// ##########################
//      PUBLIC FUNCTIONS
// ##########################

Driver* driver_create();

#define driver_add_task(...) driver_add_task_with_thread_id(__VA_ARGS__, -1)
void driver_add_task_with_thread_id(Driver* driver, Task* task, int thread_number);

void driver_run(Driver* driver);


// #############################
//      INTERNAL FUNCTIONS (can be used publicly too, for adding specialized tasks)
// ############################

Import* driver_create_import_id(Driver* driver, string path);
