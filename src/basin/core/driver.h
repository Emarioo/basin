/*
    Driver is the glue in the compiler. It manages threads, imports, memory, and steps to perform such as lexing, parsing, code gen
*/

#pragma once

#include "basin/types.h"

#include "platform/array.h"
#include "platform/bucket_array.h"
#include "platform/thread.h"

typedef enum {
    TASK_TOKENIZE,
} TaskKind;

typedef struct {
    TaskKind kind;
    union {
        ImportID import_id;
        string text;
    };
} Task;

typedef struct {
    ImportID import_id;
    string path; // sometimes we don't have path, for small code created through metaprogramming for example.
} Import;

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

    ImportID next_import_id;

    DriverThread* threads;
    int threads_len;
} Driver;

// ##########################
//      PUBLIC FUNCTIONS
// ##########################

Driver* driver_create();

void driver_add_task(Driver* driver, Task* task);

void driver_run(Driver* driver);


// #############################
//      INTERNAL FUNCTIONS (can be used publicly too, for adding specialized tasks)
// ############################

ImportID driver_create_import_id(Driver* driver, string path);
