/*
    Public interface for the Basin Compiler Library
*/

#pragma once

#include <stdint.h>

#if defined(BASIN_DLL) || defined(BASIN_DLL_EXPORT)
    #define BASIN_API extern __attribute__((visibility("default")))
#else
    #define BASIN_API extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

//##########################
//    TYPES AND OPTIONS
//##########################

typedef enum {
    BASIN_SUCCESS,
    BASIN_INVALID_COMPILE_OPTIONS,
    BASIN_COMPILE_ERROR,
    BASIN_FILE_NOT_FOUND,
    BASIN_OUT_OF_MEMORY,
    // TODO: BASIN_VM_ERROR,
} BasinError;

typedef enum {
    BASIN_TARGET_ARCH_host,
    BASIN_TARGET_ARCH_x86_32,
    BASIN_TARGET_ARCH_x86_64,
    BASIN_TARGET_ARCH_arm_32,
    BASIN_TARGET_ARCH_arm_64,
    BASIN_TARGET_ARCH_COUNT,
} BasinTargetArch;

typedef enum {
    BASIN_TARGET_OS_host,
    BASIN_TARGET_OS_windows,
    BASIN_TARGET_OS_linux,
    BASIN_TARGET_OS_COUNT,
} BasinTargetOS;

typedef enum {
    BASIN_BINARY_none,
    BASIN_BINARY_executable,
    BASIN_BINARY_object_file,
    BASIN_BINARY_static_library,
    BASIN_BINARY_dynamic_library,
} BasinBinaryType;

typedef enum {
    BASIN_OPTIMIZE_FLAG_none,
    BASIN_OPTIMIZE_FLAG_all = 0xFFFFFFFF,
} BasinOptimizeFlags;

// Returned from functions and its contents can
// be freed with basin_free_result.
typedef struct {
    BasinError  error_type;
    char* error_message;        // free with basin_allocate(0, error_message, options)

    char** compile_errors;      // free with basin_allocate(...), the two dimensional array is allocated contiguously in memory, do not free each individual error
    int    compile_errors_len;
} BasinResult;

/*
    The input path in compile options will show up in error messages and __file__. Can be NULL.
*/
typedef struct BasinCompileOptions {
    BasinTargetArch    target_arch;
    BasinTargetOS      target_os;
    BasinBinaryType    binary_output_type;
    BasinOptimizeFlags optimize_flags;
    bool               disable_debug;
    bool               run_output;
    bool               skip_default_import_dirs;
    bool               skip_default_library_dirs;
    
    const char* const* import_dirs;
    int                import_dirs_len;

    const char* const* library_dirs;
    int                library_dirs_len;

    const char*        input_text;
    u32                input_text_len;
    const char*        input_file;
    const char*        output_file;

    const char**       run_output_argv; // args passed to program/comp time execution
    int                run_output_argc;
} BasinCompileOptions;




//#########################################
//         HIGH LEVEL FUNCTIONS
//   (similar to command line arguments)
//#########################################



/*
    Compile a file.

    @param options Compile options.
    @return Result of compilation. Contains error messages or SUCCESS status.
*/
BASIN_API BasinResult basin_compile(const BasinCompileOptions* options);
BASIN_API void basin_free_result(BasinResult* result);



//#########################################
//            EXTRA FUNCTIONS
//#########################################



// Returned pointer is static, do not free it
BASIN_API const char* basin_version(int out_version[3]);
BASIN_API const char* basin_commit();
BASIN_API const char* basin_build_date();

BASIN_API const char* basin_target_arch_string(BasinTargetArch arch);
BASIN_API const char* basin_target_os_string(BasinTargetOS os);

/*
    Creates compile options from a string of arguments

    @param arguments String of command line arguments
    @param options Pointer to options to fill with options
    @return Result of compilation. Contains error messages or SUCCESS status.
*/
BASIN_API BasinResult basin_parse_arguments(const char* arguments, BasinCompileOptions* out_options);

/*
    Creates compile options from argv/argc from main function

    @param argv Array of arguments
    @param argc Number of arguments, if -1 then argv is assumed to be terminated by a NULL pointer.
    @return Result of compilation. Contains error messages or SUCCESS status.
*/
BASIN_API BasinResult basin_parse_argv(int argc, const char** argv, BasinCompileOptions* out_options);



#ifdef __cplusplus
}
#endif