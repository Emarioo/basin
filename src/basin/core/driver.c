#include "basin/core/driver.h"

#include "basin/logger.h"
#include "basin/frontend/lexer.h"
#include "basin/frontend/parser.h"
#include "basin/backend/gen_ir.h"
#include "basin/backend/ir.h"
#include "basin/backend/codegen.h"

#include "basin/error.h"
#include "basin/basin.h"

#include "platform/platform.h"
#include "util/assert.h"


Driver* driver_create() {
    Driver* driver = HEAP_ALLOC_OBJECT(Driver);
    barray_init(&driver->tasks, 100, 50);
    barray_init(&driver->imports, 100, 50);

    thread__create_mutex(&driver->import_mutex);
    thread__create_mutex(&driver->task_mutex);
    thread__create_semaphore(&driver->may_have_task_semaphore, 0, 1000000);

    return driver;
}

void driver_add_task_with_thread_id(Driver* driver, Task* task, int thread_number) {
    TracyCZone(zone, 1);

    thread__lock_mutex(&driver->task_mutex);
    
    barray_push(&driver->tasks, task);

    // we try to signal in case threads are waiting
    // if there already is a thread getting semaphore then that's fine.
    thread__signal_semaphore(&driver->may_have_task_semaphore, 1);

    thread__unlock_mutex(&driver->task_mutex);

    if(enabled_logging_driver) {
        fprintf(stderr, "[%d] Add task %s\n", thread_number, task_kind_names[task->kind]);
    }
    TracyCZoneEnd(zone);
}

u32 driver_thread_run(DriverThread* thread_driver);

void driver_run(Driver* driver) {
    TracyCZone(zone, 1);

    // the meat and potatoes of the compiler, the game loop if you will

    if (driver->options->threads == 0)
        driver->threads_len = 1;
    else
        driver->threads_len = driver->options->threads;
    driver->threads = HEAP_ALLOC_ARRAY(DriverThread, driver->threads_len);

    driver->program = (IRProgram*)HEAP_ALLOC_OBJECT(IRProgram);
    atomic_array_init(&driver->program->functions, 1000, 1000);
    atomic_array_init(&driver->program->sections, 1000, 1000);
    atomic_array_init(&driver->program->variables, 1000, 1000);

    {
        IRSection section = {};
        section.name = string_clone_cptr(".stack");
        int id = atomic_array_push(&driver->program->sections, &section);
        ASSERT(id == SECTION_ID_STACK);
    }
    {
        IRSection section = {};
        section.name = string_clone_cptr(".rodata");
        driver->sectionid_rodata = atomic_array_push(&driver->program->sections, &section);
        driver->section_rodata = atomic_array_getptr(&driver->program->sections, driver->sectionid_rodata);

        driver->section_rodata->data_cap = 0x100000;
        driver->section_rodata->data = mem__alloc(driver->section_rodata->data_cap);
    }
    {
        IRSection section = {};
        section.name = string_clone_cptr(".data");
        driver->sectionid_data = atomic_array_push(&driver->program->sections, &section);
        driver->section_data = atomic_array_getptr(&driver->program->sections, driver->sectionid_data);
        
        driver->section_data->data_cap = 0x100000;
        driver->section_data->data = mem__alloc(driver->section_data->data_cap);
    }
    
    driver->idle_threads = 0;

    if (driver->threads_len == 1) {
        driver->threads[0].driver = driver;
        driver_thread_run(&driver->threads[0]);
    } else {
        for (int i=0;i<driver->threads_len;i++) {
            driver->threads[i].driver = driver;
            thread__spawn(&driver->threads[i].thread, (u32(*)(void*))driver_thread_run, &driver->threads[i]);
        }
        for (int i=0;i<driver->threads_len;i++) {
            thread__join(&driver->threads[i].thread);
        }
    }
    
    if(enabled_logging_driver) {
        fprintf(stderr, "Threads finished\n");
    }
    TracyCZoneEnd(zone);
}

u32 driver_thread_run(DriverThread* thread_driver) {
    TracyCZone(zone, 1);

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
            thread__signal_semaphore(&driver->may_have_task_semaphore, 1);
        }

        if(enabled_logging_driver) {
            fprintf(stderr, "[%d] waiting\n", id);
        }

        thread__wait_semaphore(&driver->may_have_task_semaphore);

        thread__lock_mutex(&driver->task_mutex);
        int prev_idling_threads = atomic_add(&driver->idle_threads, -1);

        if(barray_count(&driver->tasks) == 0) {
            // TODO: If we read idle_threads here,
            if (prev_idling_threads == driver->threads_len) {
                atomic_add(&driver->idle_threads, 1);
                thread__unlock_mutex(&driver->task_mutex);
                thread__signal_semaphore(&driver->may_have_task_semaphore, 1);
                break;
            }

            thread__unlock_mutex(&driver->task_mutex);
            continue;
        }

        // We pop last task.
        //   We may  need to pick a task where dependencies are fulfilled.
        //   perhaps we won't add the task until dependencies are completed.
        //   dependencies would be parsing after tokenizing.
        //   compile time exec after function declaration checking

        Task task = {};
        barray_pop(&driver->tasks, &task);
        int tasks_left = barray_count(&driver->tasks);

        thread__unlock_mutex(&driver->task_mutex);


        if(enabled_logging_driver) {
            fprintf(stderr, "[%d] pick %s (%d left)\n", id, task_kind_names[task.kind], tasks_left);
        }

        // Perform task
        switch(task.kind) {
            case TASK_LEX_AND_PARSE: {
                if (!task.lex_and_parse.import->text.ptr) {
                    BasinResult result = {};
                    basin_string text = basin_read_whole_file(task.lex_and_parse.import->path.ptr, driver->options);
                    if(!text.ptr) {
                        FORMAT_ERROR(result, BASIN_FILE_NOT_FOUND, "\033[31mERROR:\033[0m Cannot read '%s'\n", task.lex_and_parse.import->path.ptr);
                        fprintf(stderr, "%s", result.error_message);
                        break;
                    }
                    // @TODO When should we add text to import?
                    //   In driver? What if another thread is accessig this?
                    task.lex_and_parse.import->text.ptr = text.ptr;
                    task.lex_and_parse.import->text.len = text.len;
                    task.lex_and_parse.import->text.max = text.cap;
                }
                TokenStream* stream = NULL;
                Result result = tokenize(task.lex_and_parse.import, &stream);
                if(result.kind != SUCCESS) {
                    // Print message. We are done with this series of tasks
                    fprintf(stderr, "%s", result.message.ptr);
                    break;
                }

                AST* ast = NULL;
                result = parse_stream(driver, stream, &ast);
                if(result.kind != SUCCESS) {
                    // Print message. We are done with this series of tasks
                    fprintf(stderr, "%s", result.message.ptr);
                    break;
                }
                task.lex_and_parse.import->ast = ast;

                print_ast(ast);
                // fprintf(stderr, "Parse success\n");
                
                task.kind = TASK_GEN_IR;
                task.gen_ir.import = task.lex_and_parse.import;
                driver_add_task_with_thread_id(driver, &task, id);
            } break;
            case TASK_GEN_IR: {
                Result result = generate_ir(driver, task.gen_ir.import->ast, driver->program);
                if(result.kind != SUCCESS) {
                    // Print message. We are done with this series of tasks
                    fprintf(stderr, "%s", result.message.ptr);
                } else {
                    fprintf(stderr, "Gen ir success\n");
                }
                
                task.kind = TASK_GEN_MACHINE;
                task.gen_machine.import = task.gen_ir.import;
                driver_add_task_with_thread_id(driver, &task, id);
            } break;
            case TASK_GEN_MACHINE: {
                // driver->program
                static int next_func = 0;

                atomic_add(&next_func, 1);
                IRFunction* ir_func = atomic_array_getptr(&driver->program->functions, next_func);
                CodegenFunction* func;
                PlatformOptions opts = {};
                opts.host_kind = HOST_windows;
                opts.cpu_kind = CPU_x86_64;
                CodegenResult result = codegen_generate_function(driver, ir_func, &func, &opts);
                if(result.error_type != CODEGEN_SUCCESS) {
                    // Print message. We are done with this series of tasks
                    fprintf(stderr, "%s", result.error_message);
                } else {
                    fprintf(stderr, "Gen machine success\n");
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

    TracyCZoneEnd(zone);

    return 0;
}

Import* driver_create_import_id(Driver* driver, cstring path) {
    TracyCZone(zone, 1);
    ASSERT(driver->next_import_id+1 <= 0xFFFF);

    // If we run driver again for incremental linking
    // we may already have added the import, if so
    // we can check if it was updated on disc
    // if it was we create new import otherwise
    // we reuse the import?

    // @TODO Check if path already exists!

    Import import = {};
    import.path = string_clone_cstr(path);
    import.import_id = atomic_add(&driver->next_import_id, 1);

    thread__lock_mutex(&driver->import_mutex);

    Import* ptr = barray_push(&driver->imports, &import);

    thread__unlock_mutex(&driver->import_mutex);

    TracyCZoneEnd(zone);
    return ptr;
}

string driver_resolve_import_path(Driver* driver, const Import* origin, cstring path) {
    TracyCZone(zone, 1);
    // 1. if path has . then relative to origin
    // 2. otherwise search import directories
    string str = {};
    if (path.len == 0 || path.ptr[0] == '/') {
        // empty/invalid
    } else if (path.ptr[0] == '.' && path.ptr[1] == '/') {
        str.max = origin->path.len + path.len - 1;
        str.ptr = mem__alloc(str.max + 1);
        str.len = str.max;
        memcpy(str.ptr, origin->path.ptr, origin->path.len);
        memcpy(str.ptr + origin->path.len, path.ptr+1, path.len-1);
        str.ptr[str.len] = '\0';
    } else {
        str.max = 300;
        str.ptr = mem__alloc(str.max + 1);
        bool found = false;
        for (int i=driver->import_dirs.len-1;i>=0;i--) {
            string dir = driver->import_dirs.ptr[i];
            
            // @TODO Buffer overflow if directory is longer than str.max
            //   DO NOT ASSUME IT FITS
            memcpy(str.ptr, dir.ptr, dir.len);
            str.ptr[dir.len] = '/';
            memcpy(str.ptr + dir.len + 1, path.ptr, path.len);
            str.len = dir.len + 1 + path.len;
            str.ptr[str.len] = '\0';
            
            int slash_pos = string_rfind(str.ptr, str.len-1, "/");
            int dot_pos = string_rfind(str.ptr, str.len-1, ".");
            
            if (slash_pos > dot_pos || dot_pos == -1) {
                // no file extension, add implicit .bsn
                memcpy(str.ptr + str.len, ".bsn", 4);
                str.len += 4;
                str.ptr[str.len] = '\0';
            }
            
            ASSERT(str.len < str.max);
            
            BasinFS_FileInfo info = basin_file_info(str.ptr, driver->options);
            if (info.exists) {
                found = true;
                break;
            }
        }
        if (!found) {
            // Most of the time the path will be resolved.
            // If it wasn't then we might not want to heap allocate
            // just to free it later.
            // @TODO We want to use an optimized linear string allocator.
            mem__free(str.ptr);
            str.ptr = NULL;
            str.len = 0;
            str.max = 0;
        }
    }
    TracyCZoneEnd(zone);
    return str;
}
// string driver_resolve_library_path(Driver* driver, const Import* origin, cstring path) {

// }

const char* const task_kind_names[TASK_COUNT] = {
    "TASK_INVALID",
    "TASK_LEX_AND_PARSE",
    "TASK_GEN_IR",
    "TASK_GEN_MACHINE",
};