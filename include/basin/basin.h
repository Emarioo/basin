/*
    Public interface for the Basin compiler library
*/

#pragma once

#include <stdint.h>

#if defined(BASIN_DLL) || defined(BASIN_DLL_EXPORT)
    #define BASIN_API extern __attribute__((visibility("default")))
#else
    #define BASIN_API extern
#endif

//##########################
//    TYPES AND OPTIONS
//##########################

typedef struct BasinAllocator BasinAllocator;
typedef struct BasinCompileOptions BasinCompileOptions;
typedef struct basin_string basin_string;
typedef struct BasinFS_FileInfo BasinFS_FileInfo;


typedef void* (*BasinFn_allocate)(uint64_t new_size, void* old_ptr, void* user_data);

typedef basin_string (*BasinFn_read_whole_file)(const char* path, void* user_data);
typedef bool (*BasinFn_write_whole_file)(const char* path, char* data, u64 size, void* user_data);
typedef BasinFS_FileInfo (*BasinFn_file_info)(const char* path, void* user_data);
// typedef void (*BasinFn_list_files)(const char* path, void* user_data);

typedef struct basin_string {
    void* ptr;
    u32 len;
    u32 cap;
    const BasinAllocator* allocator;
} basin_string;

typedef struct BasinAllocator {
    void* user_data;
    BasinFn_allocate allocate;
} BasinAllocator;

typedef struct BasinFS_FileInfo {
    u64 file_size;
    bool exists;
    bool is_directory;
} BasinFS_FileInfo;

typedef struct BasinFileSystem {
    void* user_data;
    BasinFn_read_whole_file read_whole_file;
    BasinFn_write_whole_file write_whole_file;
    BasinFn_file_info file_info;
    // BasinFn_list_files list_files;
} BasinFileSystem;

typedef enum {
    BASIN_SUCCESS,
    BASIN_INVALID_COMPILE_OPTIONS,
    BASIN_COMPILE_ERROR,
    BASIN_FILE_NOT_FOUND,
    BASIN_OUT_OF_MEMORY,
    // TODO: BASIN_VM_ERROR,
} BasinError;

typedef enum {
    BASIN_TARGET_ARCH_none,
    BASIN_TARGET_ARCH_host,
    BASIN_TARGET_ARCH_x86,
    BASIN_TARGET_ARCH_x86_64,
    BASIN_TARGET_ARCH_arm,
    BASIN_TARGET_ARCH_arm_64,
    BASIN_TARGET_ARCH_COUNT,
} BasinTargetArch;

typedef enum {
    BASIN_TARGET_OS_none,
    BASIN_TARGET_OS_host,
    BASIN_TARGET_OS_windows,
    BASIN_TARGET_OS_linux,
    BASIN_TARGET_OS_COUNT,
} BasinTargetOS;

typedef enum {
    BASIN_BINARY_executable,
    BASIN_BINARY_static_library,
    BASIN_BINARY_dynamic_library,
    BASIN_BINARY_object_file,
} BasinBinaryType;

typedef enum {
    BASIN_OPTIMIZE_FLAG_none,
    BASIN_OPTIMIZE_FLAG_all = 0xFFFFFFFF,
} BasinOptimizeFlags;

typedef struct {
    BasinError  error_type;
    char* error_message;        // free with basin_allocate(0, error_message, options)

    char** compile_errors;      // free with basin_allocate(...), the two dimensional array is allocated contiguously in memory, do not free each individual error
    int    compile_errors_len;
} BasinResult;

typedef struct BasinCompileOptions {
    BasinTargetArch    target_arch;
    BasinTargetOS      target_os;
    BasinBinaryType    binary_output_type;
    BasinOptimizeFlags optimize_flags;
    bool               disable_debug;
    bool               skip_default_import_dirs;
    bool               skip_default_library_dirs;
    int                threads;
    
    const char* const* import_dirs;
    int                import_dirs_len;

    const char* const* library_dirs;
    int                library_dirs_len;

    const char*        input_file;
    const char*        output_file;
    bool               print_help;
    int                start_of_user_args; // args passed to program/comp time execution

    BasinAllocator allocator;
    BasinFileSystem filesystem;
} BasinCompileOptions;

#ifdef __cplusplus
extern "C" {
#endif

//#########################################
//         HIGH LEVEL FUNCTIONS
//   (similar to command line arguments)
//#########################################

// Returned pointer is static, do not free it
BASIN_API const char* basin_version(int out_version[3]);
BASIN_API const char* basin_commit();
BASIN_API const char* basin_build_date();

/*

*/
BASIN_API BasinResult basin_compile_file(const char* path,                             const char* output_path, const BasinCompileOptions* options);
BASIN_API BasinResult basin_compile_text(const char* text, u64 size, const char* path, const char* output_path, const BasinCompileOptions* options);

/*
    Creates compile options from a string of arguments

    @param arguments String of command line arguments
    @param options Pointer to options to fill with options
*/
BASIN_API BasinResult basin_parse_arguments(const char* arguments, BasinCompileOptions* out_options);

/*
    Creates compile options from argv/argc from main function

    @param argv Array of arguments
    @param argc Number of arguments, if -1 then argv is assumed to be terminated by a NULL pointer.
*/
BASIN_API BasinResult basin_parse_argv(int argc, const char** argv, BasinCompileOptions* out_options);


BASIN_API const char* basin_target_arch_string(BasinTargetArch arch);
BASIN_API const char* basin_target_os_string(BasinTargetOS os);

//#########################################
//         LOW LEVEL FUNCTIONS
//#########################################

typedef struct BasinDriver BasinDriver;
typedef struct BasinTask BasinTask;

// @TODO move driver functions here, or add wrappers for them here
BASIN_API BasinResult basin_driver_create(BasinDriver** out_driver);
BASIN_API BasinResult basin_driver_submit(BasinDriver* driver, BasinTask* task);
BASIN_API BasinResult basin_driver_cleanup(BasinDriver* driver);

//########################################
//      USER OVERRIDABLE FUNCTIONS
//       (allocator, file system)
//########################################


// Parameter usage:
//    free      new_size == 0           
//    realloc   old_ptr && new_size > 0 
//    malloc    !old_ptr && new_size > 0
//
//   Will call options->allocator.func if available (user provided allocator)
BASIN_API void* basin_allocate(int new_size, void* old_ptr, const BasinCompileOptions* options);

BASIN_API basin_string     basin_read_whole_file (const char* path,                       const BasinCompileOptions* options);
BASIN_API bool             basin_write_whole_file(const char* path, char* data, u64 size, const BasinCompileOptions* options);
BASIN_API BasinFS_FileInfo basin_file_info       (const char* path,                       const BasinCompileOptions* options);



#ifdef __cplusplus
}
#endif