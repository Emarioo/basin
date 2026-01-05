/*
    Driver is the glue in the compiler. It manages threads, imports, memory, and steps to perform such as lexing, parsing, code gen

    Driver begins empty.

    User submits tasks to the driver.

    User starts the driver which launches threads (unless thread count is set to 1).
    The driver performs tasks and modifies the driver state until list is empty.

    Driver now has source text, AST, IR, machine code in memory and intermediate
    files written to the disc.

    User queries the result of the tasks (compilation successful/failed)
*/

#pragma once

#include "basin/types.h"
#include "basin/frontend/lexer.h"
#include "basin/basin.h"
#include "basin/backend/ir.h"

#include "platform/array.h"
#include "platform/bucket_array.h"
#include "platform/thread.h"

typedef struct AST AST;

typedef enum {
    TASK_INVALID,
    TASK_LEX_AND_PARSE,
    TASK_GEN_IR,
    TASK_COUNT,
} TaskKind;
extern const char* const task_kind_names[TASK_COUNT];

typedef struct Task {
    TaskKind kind;
    union {
        struct {
            Import* import;
        } lex_and_parse;
        struct {
            Import* import;
        } gen_ir;
    };
} Task;


DEF_ARRAY(string)
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

    IRCollection* collection;

    DriverThread* threads;
    int threads_len;

    Array_string import_dirs;
    Array_string library_dirs;

    const BasinCompileOptions* options;
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

// THREAD SAFE
Import* driver_create_import_id(Driver* driver, cstring path);
// THREAD SAFE
string driver_resolve_import_path(Driver* driver, const Import* origin, cstring path);
// THREAD SAFE
string driver_resolve_library_path(Driver* driver, const Import* origin, cstring path);
