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

#include "basin/common.h"
// #include "basin/frontend/lexer.h"
#include "basin/basin.h"
#include "basin/backend/ir.h"

#include "basin/util/array.h"
#include "basin/util/bucket_array.h"
#include "platform/platform.h"

typedef struct AST AST;

typedef enum {
    TASK_INVALID,
    TASK_PARSE,
    TASK_GEN_IR,
    TASK_GEN_MACHINE,
    TASK_GEN_OBJECT,
    TASK_COUNT,
} TaskKind;
extern const char* task_kind_names[TASK_COUNT];

typedef struct Task Task;
typedef struct TaskItem TaskItem;

struct TaskItem {
    Task* current;
    TaskItem* next;
};

// A unit of work to be performed by the driver
struct Task {
    TaskKind     kind;
    volatile u32 remainingDependencies;
    Compilation* compilation;
    TaskItem*    tasksBlockedByMe;

    union {
        struct {
            Import* import;
        } lex_and_parse;
        struct {
            Import* import;
        } gen_ir;
        struct {
            // Import* import;
            IRFunction* ir_function;
        } gen_machine;
        struct {
            Import* import;
        } gen_object;
    };
};

typedef Task* TaskP;

DEF_BUCKET_ARRAY(Task)
DEF_ARRAY(TaskP)


typedef struct Driver Driver;

typedef struct {
    Driver* driver;
    Thread thread;
} DriverThread;


DEF_BUCKET_ARRAY(Compilation)

typedef struct Driver {
    DriverThread*    threads;
    u32              threads_len;
    u32              threads_cap;

    Array_TaskP      queuedTasks;

    BucketArray_Task tasks;
    Mutex            tasks_mutex;
    Semaphore        may_have_task_semaphore;

    BucketArray_Compilation compilations;
    Mutex                   compilations_mutex;
    
    BucketArray_Import imports;
    ImportID           next_import_id;
    Mutex              import_mutex;
    
    volatile u32 idle_threads;
    volatile u32 task_process_limit;
    
} Driver;

// ##########################
//      PUBLIC FUNCTIONS
// ##########################

Driver* driver_create();
void    driver_run(Driver* driver, u32 thread_count, u32 task_process_limit);
void    driver_cleanup(Driver* driver);

#define driver_add_task(...) driver_add_task_with_thread_id(__VA_ARGS__, -1)
Task* driver_add_task_with_thread_id(Driver* driver, Task* task, int thread_number);

void driver_add_task_dependency(Driver* driver, Task* task, Task* dep);
void driver_mark_task_ready(Driver* driver, Task* task, bool alreadyHasLockedMutex);


Compilation* driver_create_compilation(Driver* driver, const BasinCompileOptions* options);
// Compilation* driver_submit_compilation(Driver* driver, const BasinCompileOptions* options);


// #############################
//      INTERNAL FUNCTIONS (can be used publicly too, for adding specialized tasks)
// ############################

// THREAD SAFE
Import* driver_create_import_id(Driver* driver, cstring path);
// THREAD SAFE
string comp_resolve_import_path(Compilation* compilation, const Import* origin, cstring path);
// THREAD SAFE
// string comp_resolve_library_path(Compilation* compilation, const Import* origin, cstring path);
