#include "basin/core/driver.h"

#include "basin/logger.h"
#include "basin/frontend/lexer.h"
#include "basin/frontend/parser.h"

#include "platform/memory.h"


Driver* driver_create() {
    Driver* driver = HEAP_ALLOC_OBJECT(Driver);
    barray_init_Task(&driver->tasks, 100, 50);
    barray_init_Import(&driver->imports, 100, 50);

    create_mutex(&driver->import_mutex);
    create_mutex(&driver->task_mutex);
    create_semaphore(&driver->may_have_task_semaphore, 0, 1000000);

    return driver;
}

void driver_add_task_with_thread_id(Driver* driver, Task* task, int thread_number) {
    lock_mutex(&driver->task_mutex);
    
    barray_push_Task(&driver->tasks, task);

    // we try to signal in case threads are waiting
    // if there already is a thread getting semaphore then that's fine.
    signal_semaphore(&driver->may_have_task_semaphore, 1);

    unlock_mutex(&driver->task_mutex);

    if(enabled_logging_driver) {
        fprintf(stderr, "[%d] Add task %s\n", thread_number, task_kind_names[task->kind]);
    }
}

u32 driver_thread_run(DriverThread* thread_driver);

void driver_run(Driver* driver) {
    // the meat and potatoes of the compiler, the game loop if you will

    // TODO: Thread-less option (useful when using compiler as a library, you may not want it spawning a bunch of threads)

    driver->threads_len = 2; // TODO: Get thread count from somewhere
    driver->threads = HEAP_ALLOC_ARRAY(DriverThread, driver->threads_len);
    
    driver->idle_threads = 0;

    for (int i=0;i<driver->threads_len;i++) {
        driver->threads[i].driver = driver;
        
        spawn_thread(&driver->threads[i].thread, (u32(*)(void*))driver_thread_run, &driver->threads[i]);
    }

    for (int i=0;i<driver->threads_len;i++) {
        join_thread(&driver->threads[i].thread);
    }
    
    if(enabled_logging_driver) {
        fprintf(stderr, "Threads finished\n");
    }
}

u32 driver_thread_run(DriverThread* thread_driver) {
    Driver* driver = thread_driver->driver;

    int id = ((u64)thread_driver - (u64)driver->threads) / sizeof(*thread_driver);

    if(enabled_logging_driver) {
        fprintf(stderr, "[%d] Started\n", id);
    }

    while(true) {
        // NOTE: We only modify and check idle_threads here in the beginning of the loop.

        int prev_value = atomic_add(&driver->idle_threads, 1);
        if(prev_value + 1 == driver->threads_len) {
            // It seems like every thread is idling BUT WE CAN'T NOW FOR SURE because in theory (very unlikely)
            // a thread may be stuck between wait_semapore and atomic_add(idle_threads).
            // So we signal the semaphore to make sure.
            signal_semaphore(&driver->may_have_task_semaphore, 1);
        }

        if(enabled_logging_driver) {
            fprintf(stderr, "[%d] waiting\n", id);
        }

        wait_semaphore(&driver->may_have_task_semaphore);

        lock_mutex(&driver->task_mutex);
        int prev_idling_threads = atomic_add(&driver->idle_threads, -1);

        if(barray_count_Task(&driver->tasks) == 0) {
            // TODO: If we read idle_threads here,
            if (prev_idling_threads == driver->threads_len) {
                atomic_add(&driver->idle_threads, 1);
                unlock_mutex(&driver->task_mutex);
                signal_semaphore(&driver->may_have_task_semaphore, 1);
                break;
            }

            unlock_mutex(&driver->task_mutex);
            continue;
        }

        // We pop last task.
        //   We may  need to pick a task where dependencies are fulfilled.
        //   perhaps we won't add the task until dependencies are completed.
        //   dependencies would be parsing after tokenizing.
        //   compile time exec after function declaration checking

        Task task;
        barray_pop_Task(&driver->tasks, &task);
        int tasks_left = barray_count_Task(&driver->tasks);

        unlock_mutex(&driver->task_mutex);


        if(enabled_logging_driver) {
            fprintf(stderr, "[%d] pick %s (%d left)\n", id, task_kind_names[task.kind], tasks_left);
        }

        // Perform task
        switch(task.kind) {
            case TASK_TOKENIZE: {
                TokenStream* stream;
                Result result = tokenize(task.tokenize.import, &stream);
                if(result.kind != SUCCESS) {
                    // Print message. We are done with this series of tasks
                    fprintf(stderr, "%s", result.message.ptr);
                } else {
                    task.kind = TASK_PARSE;
                    task.parse.stream = stream;
                    driver_add_task_with_thread_id(driver, &task, id);
                }
            } break;
            case TASK_PARSE: {
                AST* ast;
                Result result = parse_stream(task.parse.stream, &ast);
                if(result.kind != SUCCESS) {
                    // Print message. We are done with this series of tasks
                    fprintf(stderr, "%s", result.message.ptr);
                } else {
                    fprintf(stderr, "Parse success\n");
                }
            } break;
            default: {
                fprintf(stderr, "Unhandled task %d\n", task.kind);
                ASSERT(false);
            }
        }

        // once done, we may add new tasks, atomically, with mutex
        // loop again find a new task

        
    }

    if(enabled_logging_driver) {
        fprintf(stderr, "[%d] Stopped\n", id);
    }

    return 0;
}

Import* driver_create_import_id(Driver* driver, string path) {
    ASSERT(driver->next_import_id+1 <= 0xFFFF);

    Import import;
    import.path = path;
    import.import_id = atomic_add(&driver->next_import_id, 1);

    lock_mutex(&driver->import_mutex);

    Import* ptr = barray_push_Import(&driver->imports, &import);

    unlock_mutex(&driver->import_mutex);

    return ptr;
}

const char* const task_kind_names[TASK_COUNT] = {
    "TASK_INVALID",
    "TASK_TOKENIZE",
    "TASK_PARSE",
};