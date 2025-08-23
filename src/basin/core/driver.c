#include "basin/core/driver.h"

#include "platform/memory.h"

Driver* driver_create() {
    Driver* driver = HEAP_ALLOC_OBJECT(Driver);
    barray_init_Task(&driver->tasks, 100, 50);
    barray_init_Import(&driver->imports, 100, 50);

    create_mutex(&driver->import_mutex);
    create_mutex(&driver->task_mutex);

    return driver;
}

void driver_add_task(Driver* driver, Task* task) {
    lock_mutex(&driver->import_mutex);
    
    barray_push_Task(&driver->tasks, task);

    unlock_mutex(&driver->import_mutex);
}

u32 driver_thread_run(DriverThread* thread_driver);

void driver_run(Driver* driver) {
    // the meat and potatoes of the compiler, the game loop if you will

    // TODO: Thread-less option (useful when using compiler as a library, you may not want it spawning a bunch of threads)

    driver->threads_len = 2; // TODO: Get thread count from somewhere
    driver->threads = heap_alloc(sizeof(Thread) * driver->threads_len);
    
    for (int i=0;i<driver->threads_len;i++) {
        driver->threads[i].driver = driver;
        spawn_thread(&driver->threads[i].thread, driver_thread_run, &driver->threads[i]);
    }

    for (int i=0;i<driver->threads_len;i++) {
        join_thread(&driver->threads[i].thread);
    }
    
    fprintf(stderr, "Threads finished\n");
}

u32 driver_thread_run(DriverThread* thread_driver) {
    Driver* driver = thread_driver->driver;

    int id = ((u64)thread_driver - (u64)driver->threads) / sizeof(*thread_driver);

    fprintf(stderr, "[%d] Started\n", id);



    return 0;
}

ImportID driver_create_import_id(Driver* driver, string path) {
    ASSERT(driver->next_import_id+1 <= 0xFFFF);

    Import import;
    import.path = path;
    import.import_id = atomic_add(&driver->next_import_id, 1);

    lock_mutex(&driver->import_mutex);

    barray_push_Import(&driver->imports, &import);

    unlock_mutex(&driver->import_mutex);

    return import.import_id;
}