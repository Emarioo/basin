The basin C API is found in `include/basin/basin.h`.

What we value in the API is:
- A single header which doesn't include any other headers
- Doesn't use new C features
- Uses normal C strings
- Clear description of who owns memory, what to cleanup and when to do it.
- Zeroing enums and structs provides default behaviour.
- User specified allocator
- User specified file system

This doesn't mean that we can't use new C features internally. It's just the API that needs to be accessible.



# Public API


We have a black box, this is the basin compiler. It takes in data and produces data, usually text following Basin spec converted to ELF object file.

Compiler is built around a **State** and operations to modify the state. Then there are additional methods to parse arguments, get basin version which doesn't depend on the state.

The **State** is an instance of the compiler. The public API lets you submit operations to compile one ore more source files which will be turned into object files.

Then there is the **Driver** which performs the operations. The **Driver** may submit more operations to try.

The **Driver** is the execution engine which may use one or more threads to perform operations on the **State**.

In your program you may create multiple **States**, and start the **Driver** for them in threads you spawn. Each driver uses one thread.

It is recommended to have one **State** because the compiler can reuse processed imports.


```

// determines allocator, file api, thread api
PlatformOptions platform_options = {
    .memory_api = NULL, // default allocator, OS malloc
    .thread_api = NULL, // default thread allocator, pthread_create
    .file_api   = NULL, // default file api, fopen
}

// Create state
State* state = basin_state_create(&platform_options)

// Submit compile operation
CompileOptions compile_options = {
    .input_file = "dev.bsn",
    .output_file = "dev.bsn",
    // debug enabled
    // no optimizations
}
Operation op = {
    .command = BASIN_OP_COMPILE, // full compile
    .compile_options = compile_options,
}
// uses memory_api when allocting memory
int operation_id = basin_state_submit(state, &op)

DriverOptions driver_options = {
    .threads = os_core_count();
    .performace_metrics = {
        .path = "bin/metrics"
    };
}
// Spawn threads
// Destroys them when finished
// user platform options when creating threads, reading files, allocating memory
basin_state_driver(state, &driver_options)

// query results
operation_result = basin_query(operation_id)
if (operation_result.resukt_kind == SUCCESS) {
    printf("great")
} else {
    printf("great", operation_result.error_message)
    for (int i=0;i<operation_result.error_len) {
        printf("%s", operation_result.errors[i])
    }
}

```