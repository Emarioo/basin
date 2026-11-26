#include "basin/core/driver.h"

#include "basin/logger.h"
#include "basin/frontend/parser.h"
#include "basin/frontend/ast.h"
#include "basin/frontend/gen_ir.h"
#include "basin/frontend/ir.h"
#include "basin/backend/codegen.h"
#include "basin/backend/objfile.h"

#include "basin/error.h"
#include "basin/basin.h"

#include "platform/platform.h"
#include "basin/util/assert.h"
#include "basin/util/file.h"


Driver* driver_create() {
    TracyCZone(zone, 1);
    
    Driver* driver = HEAP_ALLOC_OBJECT(Driver);
    array_init(&driver->queuedTasks, 100);
    barray_init(&driver->tasks, 200, 200);
    barray_init(&driver->imports, 100, 50);
    barray_init(&driver->compilations, 10, 10);

    thread__create_mutex(&driver->compilations_mutex);
    thread__create_mutex(&driver->import_mutex);
    thread__create_mutex(&driver->tasks_mutex);
    thread__create_semaphore(&driver->may_have_task_semaphore, 0, 1000000);

    TracyCZoneEnd(zone);
    return driver;
}

Task* driver_add_task_with_thread_id(Driver* driver, Task* task, int thread_number) {
    TracyCZone(zone, 1);

    ASSERT_DEBUG(task->compilation);

    thread__lock_mutex(&driver->tasks_mutex);
    
    Task* newTask = barray_push(&driver->tasks, task);

    // we try to signal in case threads are waiting
    // if there already is a thread getting semaphore then that's fine.
    thread__signal_semaphore(&driver->may_have_task_semaphore, 1);

    thread__unlock_mutex(&driver->tasks_mutex);

    if(DUMP_DRIVER_TASKING()) {
        debug("[%d] Add task %s\n", thread_number, task_kind_names[task->kind]);
    }
    TracyCZoneEnd(zone);

    return newTask;
}

void driver_add_task_dependency(Driver* driver, Task* task, Task* dep) {
    TaskItem* item = mem__alloc(sizeof(TaskItem));
    *item = (TaskItem){};
    
    atomic_add(&task->remainingDependencies, 1);
    
    
    thread__lock_mutex(&driver->tasks_mutex);

    item->current = task;
    item->next = dep->tasksBlockedByMe;
    dep->tasksBlockedByMe = item;

    thread__unlock_mutex(&driver->tasks_mutex);

}
void driver_mark_task_ready(Driver* driver, Task* task, bool alreadyHasLockedMutex) {
    if (task->remainingDependencies == 0) {
        if(DUMP_DRIVER_TASKING()) {
            debug("[?] Ready task %s\n", task_kind_names[task->kind]);
        }
        if (!alreadyHasLockedMutex)
            thread__lock_mutex(&driver->tasks_mutex);

        array_push(&driver->queuedTasks, &task);

        if (!alreadyHasLockedMutex)
            thread__unlock_mutex(&driver->tasks_mutex);
    } else {
        if(DUMP_DRIVER_TASKING()) {
            debug("[?] Tried mark ready %s\n", task_kind_names[task->kind]);
        }
    }
}

Compilation* driver_create_compilation(Driver* driver, const BasinCompileOptions* options) {
    TracyCZone(zone, 1);

    thread__lock_mutex(&driver->compilations_mutex);

    Compilation _empty_comp = {};
    Compilation* comp = barray_push(&driver->compilations, &_empty_comp);

    thread__unlock_mutex(&driver->compilations_mutex);

    comp->driver  = driver;
    comp->options = options;

    for (int i=0;i<options->import_dirs_len;i++) {
        string s = string_clone_cptr(options->import_dirs[i]);
        string_good_slashes(&s);
        array_push(&comp->import_dirs, &s);
    }

    string exe_path = string_create(400);
    fs__exepath(exe_path.max, exe_path.ptr);
    exe_path.len = strlen(exe_path.ptr);
    if (!options->disable_default_import_dirs) {
        int closest_slash_pos = string_rfind(exe_path.ptr, exe_path.len-1, "/");
        ASSERT(closest_slash_pos != -1); // @NOCHECKIN Give good error message
        // basin_root/bin/basin
        //               ^
        // basin_root/basin
        //           ^
        int root_slash_pos = string_rfind(exe_path.ptr, closest_slash_pos-1, "/");
        ASSERT(root_slash_pos != -1); // @NOCHECKIN Give good error message

        cstring view = {};
        view.ptr = exe_path.ptr + root_slash_pos;
        view.len = 1 + closest_slash_pos - root_slash_pos;
        bool has_bin = string_equal_cstr(view, "/bin/");
        const char* import_folder = "import";
        int import_len = strlen(import_folder);
        if (has_bin) {
            string s = string_create(root_slash_pos+1 + import_len);
            memcpy(s.ptr, exe_path.ptr, root_slash_pos+1);
            memcpy(s.ptr + root_slash_pos+1, import_folder, import_len);
            s.len = root_slash_pos+1 + import_len;
            s.ptr[s.len] = '\0';
            array_push(&comp->import_dirs, &s);
        } else {
            string s = string_create(closest_slash_pos+1 + import_len);
            memcpy(s.ptr, exe_path.ptr, closest_slash_pos+1);
            memcpy(s.ptr + closest_slash_pos+1, import_folder, import_len);
            s.len = closest_slash_pos+1 + import_len;
            s.ptr[s.len] = '\0';
            array_push(&comp->import_dirs, &s);
        }
    }

    string_cleanup(&exe_path);

    // Made where and when?
    comp->program = HEAP_ALLOC_OBJECT(IRProgram);
    atomic_array_init(&comp->program->functions, 1000, 1000);
    atomic_array_init(&comp->program->sections, 1000, 1000);
    atomic_array_init(&comp->program->variables, 1000, 1000);
    {
        IRSection section = {};
        section.name = string_clone_cptr(".stack");
        int id = atomic_array_push(&comp->program->sections, &section);
        ASSERT(id == SECTION_ID_STACK);
    }
    {
        IRSection section = {};
        section.name = string_clone_cptr(".rodata");
        comp->sectionid_rodata = atomic_array_push(&comp->program->sections, &section);
        comp->section_rodata = atomic_array_getptr(&comp->program->sections, comp->sectionid_rodata);

        comp->section_rodata->data_cap = 0x100000;
        comp->section_rodata->data = mem__alloc(comp->section_rodata->data_cap);
    }
    {
        IRSection section = {};
        section.name = string_clone_cptr(".data");
        comp->sectionid_data = atomic_array_push(&comp->program->sections, &section);
        comp->section_data = atomic_array_getptr(&comp->program->sections, comp->sectionid_data);
        
        comp->section_data->data_cap = 0x100000;
        comp->section_data->data = mem__alloc(comp->section_data->data_cap);
    }
    {
        IRSection section = {};
        section.name = string_clone_cptr(".bss");
        comp->sectionid_bss = atomic_array_push(&comp->program->sections, &section);
        comp->section_bss = atomic_array_getptr(&comp->program->sections, comp->sectionid_bss);
    }

    comp->machine_program = HEAP_ALLOC_OBJECT(MachineProgram);
    atomic_array_init(&comp->machine_program->functions, 1000, 1000);

    TracyCZoneEnd(zone);
    return comp;
}

u32 driver_thread_run(DriverThread* thread_driver);

void driver_run(Driver* driver, u32 thread_count, u32 task_process_limit) {
    TracyCZone(zone, 1);

    // the meat and potatoes of the compiler, the game loop if you will
    
    driver->idle_threads = 0;
    if (task_process_limit == 0)
        driver->task_process_limit = 0xFFFFFFFF;

    // Starting threads is slow (15 threads roughly ~10ms depending on computer).
    // But doing work in parallel is very beneficial so this static cost is fine.
    if (thread_count == 0) {
        thread_count = sys__cpu_count();
    }

    // @TODO Temporary Buggy printing without this
    thread_count = 1;
    
    if (thread_count > driver->threads_cap) {
        int new_max = thread_count;
        driver->threads = mem__allocate(new_max * sizeof(DriverThread), driver->threads);
        driver->threads_cap = new_max;
    }
    driver->threads_len = thread_count;
    
    memset(driver->threads, 0, driver->threads_cap * sizeof(DriverThread));

    for (u32 i = 1; i<driver->threads_len; i++) {
        driver->threads[i].driver = driver;
        thread__spawn(&driver->threads[i].thread, (u32(*)(void*))driver_thread_run, &driver->threads[i]);
    }
    
    driver->threads[0].driver = driver;
    driver_thread_run(&driver->threads[0]);

    for (u32 i = 1; i < driver->threads_len; i++) {
        thread__join(&driver->threads[i].thread);
    }
    
    if(DUMP_DRIVER_TASKING()) {
        debug("Threads finished\n");
    }
    TracyCZoneEnd(zone);
}

void driver_cleanup(Driver* driver) {
    TracyCZone(zone, 1);

    // @TODO We need to cleanup allocations in imports

    barray_cleanup(&driver->imports);
    thread__cleanup_mutex(&driver->import_mutex);

    array_cleanup(&driver->queuedTasks);

    barray_cleanup(&driver->tasks);
    thread__cleanup_mutex(&driver->tasks_mutex);
    thread__cleanup_semaphore(&driver->may_have_task_semaphore);
    
    barray_cleanup(&driver->compilations);
    thread__cleanup_mutex(&driver->compilations_mutex);
    
    
    
    
    mem__free(driver->threads);
    mem__free(driver);
    TracyCZoneEnd(zone);
}


u32 driver_thread_run(DriverThread* thread_driver) {
    TracyCZone(zone, 1);

    Driver* driver = thread_driver->driver;

    int id = ((u64)thread_driver - (u64)driver->threads) / sizeof(*thread_driver);

    if(DUMP_DRIVER_TASKING()) {
        debug("[%d] Started\n", id);
    }

    while(true) {
        // NOTE: We only modify and check idle_threads here in the beginning of the loop.

        u32 prev_value = atomic_add(&driver->idle_threads, 1);
        if(prev_value + 1 == driver->threads_len) {
            // It seems like every thread is idling BUT WE CAN'T NOW FOR SURE because in theory (very unlikely)
            // a thread may be stuck between wait_semapore and atomic_add(idle_threads).
            // So we signal the semaphore to make sure.
            thread__signal_semaphore(&driver->may_have_task_semaphore, 1);
        }

        if(DUMP_DRIVER_TASKING()) {
            debug("[%d] waiting\n", id);
        }

        thread__wait_semaphore(&driver->may_have_task_semaphore);

        thread__lock_mutex(&driver->tasks_mutex);
        u32 prev_idling_threads = atomic_add(&driver->idle_threads, -1);

        // @TODO Handle task_process_limit

        if(array_size(&driver->queuedTasks) == 0) {
        // if(barray_count(&driver->tasks) == 0) {
            // TODO: If we read idle_threads here,
            if (prev_idling_threads == driver->threads_len) {
                atomic_add(&driver->idle_threads, 1);
                thread__unlock_mutex(&driver->tasks_mutex);
                thread__signal_semaphore(&driver->may_have_task_semaphore, 1);
                break;
            }

            thread__unlock_mutex(&driver->tasks_mutex);
            continue;
        }

        // We pop last task.
        //   We may need to pick a task where dependencies are fulfilled.
        //   perhaps we won't add the task until dependencies are completed.
        //   dependencies would be parsing after tokenizing.
        //   compile time exec after function declaration checking

        // Check fulfilled tasks, if none.

        // Calculate dependencies for a task and run it.

        Task* task = array_pop(&driver->queuedTasks);
        int tasks_left = array_size(&driver->queuedTasks);

        atomic_add(&task->compilation->active_tasks, 1);
        atomic_add(&task->compilation->pending_tasks, -1);

        thread__unlock_mutex(&driver->tasks_mutex);


        if(DUMP_DRIVER_TASKING()) {
            debug("[%d] pick %s (%d left)\n", id, task_kind_names[task->kind], tasks_left);
        }

        // Perform task
        switch(task->kind) {
            case TASK_PARSE: {
                if (!task->lex_and_parse.import->text.ptr) {
                    BasinResult result = {};
                    string text = util_read_whole_file(task->lex_and_parse.import->path.ptr);
                    if(!text.ptr) {
                        FORMAT_ERROR(result, BASIN_FILE_NOT_FOUND, "Cannot read '%s'\n", task->lex_and_parse.import->path.ptr);
                        fprintf(stderr, "%s", result.error_message);
                        break;
                    }
                    task->lex_and_parse.import->text = text;
                    import_init_line_positions(task->lex_and_parse.import);
                }
                // TokenStream* stream = NULL;
                // Result result = tokenize(task.lex_and_parse.import, &stream);
                // if(result.kind != SUCCESS) {
                //     // Print message. We are done with this series of tasks
                //     fprintf(stderr, "%s", result.message.ptr);
                //     break;
                // }

                AST* ast = NULL;
                Result result = parse_import(task->compilation, task->lex_and_parse.import, &ast);
                if(result.kind != SUCCESS) {
                    // Print message. We are done with this series of tasks
                    fprintf(stderr, "%s", result.message.ptr);
                    driver->failed = true;
                    break;
                }
                
                task->lex_and_parse.import->ast = ast;
                

                if (DUMP_INTERNAL_DATA()) {
                    print_ast(ast);
                }
                // fprintf(stderr, "Parse success\n");

                Task newTask = {};
                newTask.kind = TASK_GEN_IR;
                newTask.compilation = task->compilation;
                newTask.gen_ir.import = task->lex_and_parse.import;
                
                Task* pTask = driver_add_task_with_thread_id(driver, &newTask, id);
                // @TODO This task should have all imports as dependencies.
                for (int i = 0; i < array_size(&ast->tasks); i++) {
                    Task* dep = array_get(&ast->tasks, i);
                    driver_add_task_dependency(driver, pTask, dep);
                }

                driver_mark_task_ready(driver, pTask, false);
            } break;
            case TASK_GEN_IR: {
                // All previous parse tasks for the compilation unit must be done at this point.
                // If we in comp time add parse tasks we are kind of doomed. off-sync.
                
                // We cannot add dependency check here and add it back to the task queue.
                // It's a waste of CPU time. We would need to add it to the front too.
                
                // Tasks for next phase, do we put them in a temp queue and then add
                // them to the real queue when all tasks are done?


                Result result = generate_ir(task->compilation, task->gen_ir.import->ast, task->compilation->program);
                if(result.kind != SUCCESS) {
                    // Print message. We are done with this series of tasks
                    fprintf(stderr, "%s", result.message.ptr);
                    driver->failed = true;
                } else {
                    if (DUMP_GENIR_STAGE()) {
                        debug("Gen ir success\n");
                    }
                }
                
                // task->kind = TASK_GEN_MACHINE;
                // task->gen_machine.import = task->gen_ir.import;
                // driver_add_task_with_thread_id(driver, &task, id);
            } break;
            case TASK_GEN_MACHINE: {
                
                MachineFunction* func;
                // This function adds the MachineFunction to machine program.
                CodegenResult result = codegen_generate_function(task->compilation, task->gen_machine.ir_function, &func);
                if(result.error_type != CODEGEN_SUCCESS) {
                    // Print message. We are done with this series of tasks
                    fprintf(stderr, "%s", result.error_message);
                    driver->failed = true;
                } else {
                    if (DUMP_CODEGEN_STAGE()) {
                        debug("Gen machine success\n");
                    }
                }

                // if not tasks in compilation unit

                // If last task then gen object.
                // Well, what if we add another gen machine task.

            } break;
            case TASK_GEN_OBJECT: {

                bool result = generate_object_file(task->compilation);
                if (!result) {
                    driver->failed = true;
                }
            } break;
            default: {
                debug("Unhandled task %d\n", task->kind);
                ASSERT(false);
            }
        }

        thread__lock_mutex(&driver->tasks_mutex);
        
        TaskItem* nextTask = task->tasksBlockedByMe;
        while (nextTask) {
            u32 result = atomic_add(&nextTask->current->remainingDependencies, -1) - 1;

            ASSERT((int)result >= 0);
            if (result == 0) {
                driver_mark_task_ready(driver, nextTask->current, true);
            }

            nextTask = nextTask->next;
        }

        thread__unlock_mutex(&driver->tasks_mutex);


        atomic_add(&task->compilation->active_tasks, -1);

        // @TODO Task can be freed. Including TaskItems

        // once done, we may add new tasks, atomically, with mutex
        // loop again find a new task
    }

    // @TODO Temporary
    if (id == 0) {
        bool result = generate_object_file(barray_get(&driver->compilations, 0));
        if (!result) {
            driver->failed = true;
        }
    }

    if(DUMP_DRIVER_TASKING()) {
        debug("[%d] Stopped\n", id);
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
    string_good_slashes(&import.path);

    import.import_id = atomic_add(&driver->next_import_id, 1);

    thread__lock_mutex(&driver->import_mutex);

    Import* ptr = barray_push(&driver->imports, &import);

    thread__unlock_mutex(&driver->import_mutex);

    TracyCZoneEnd(zone);
    return ptr;
}

string comp_resolve_import_path(Compilation* compilation, const Import* origin, cstring path) {
    TracyCZone(zone, 1);
    // 1. if path has . then relative to origin
    // 2. otherwise search import directories
    string str = {};
    if (path.len == 0 || path.ptr[0] == '/') {
        // empty/invalid
    } else if (path.len >= 2 && path.ptr[0] == '.' && path.ptr[1] == '/') {
        int slash_pos = string_rfind(origin->path.ptr, origin->path.len-1, "/");
        str.max = slash_pos + 1 + path.len - 2;
        str.ptr = mem__alloc(str.max + 1);
        str.len = str.max;
        memcpy(str.ptr, origin->path.ptr, slash_pos + 1);
        memcpy(str.ptr + slash_pos + 1, path.ptr+2, path.len-2);
        str.ptr[str.len] = '\0';
    } else if (path.len >= 3 && path.ptr[0] == '.' && path.ptr[1] == '.' && path.ptr[2] == '/') {
        int slash_pos = string_rfind(origin->path.ptr, origin->path.len-1, "/");
        str.max = slash_pos + 1 + path.len;
        str.ptr = mem__alloc(str.max + 1);
        str.len = str.max;
        memcpy(str.ptr, origin->path.ptr, slash_pos + 1);
        memcpy(str.ptr + slash_pos + 1, path.ptr, path.len);
        str.ptr[str.len] = '\0';
    } else {
        str.max = 300;
        str.ptr = mem__alloc(str.max + 1);
        bool found = false;
        for (int i=compilation->import_dirs.len-1;i>=0;i--) {
            string dir = compilation->import_dirs.ptr[i];
            
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
            
            if (fs__exists(str.ptr)) {
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

const char* task_kind_names[TASK_COUNT] = {
    "TASK_INVALID",
    "TASK_PARSE",
    "TASK_GEN_IR",
    "TASK_GEN_MACHINE",
    "TASK_GEN_OBJECT",
};