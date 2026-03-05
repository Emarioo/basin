#pragma once

#include "basin/basin.h"

#include "util/string.h"
#include "util/array.h"
#include "util/bucket_array.h"
#include "util/atomic_array.h"

DEF_ARRAY(string)

typedef u16 ImportID;

#define INVALID_IMPORT_ID 0xFFFFu


typedef struct TokenStream TokenStream;
typedef struct AST AST;

typedef struct BasinCompileOptions BasinCompileOptions;
typedef struct IRProgram IRProgram;
typedef struct IRSection IRSection;
typedef u8 IRSectionID;
typedef u32 IRFunction_id;

typedef struct Driver Driver;

typedef enum {
    RELOCATION_TYPE_FUNCTION,
    RELOCATION_TYPE_DATA_OBJECT,
} _MachineRelocationType;
typedef u8 MachineRelocationType;

typedef struct {
    int code_offset;
    MachineRelocationType type;
    union {
        IRFunction_id function_id;
        struct {
            IRSectionID section_id;
            int         value_offset;
        };
    };
} MachineRelocation;

DEF_ARRAY(MachineRelocation)

typedef struct {
    IRFunction_id id;
    
    u8* code;
    int code_len;
    int code_max;

    Array_MachineRelocation relocations;

    // TODO: Debug information
} MachineFunction;

DEF_ATOMIC_ARRAY(MachineFunction);

typedef struct {
    AtomicArray_MachineFunction functions;
    
} MachineProgram;

typedef struct {
    ImportID import_id;
    string path; // sometimes we don't have path, for small code created through metaprogramming for example.
    string text; // text may be empty, in this case the driver needs to look at path and read the file
    TokenStream* stream;
    AST* ast;
} Import;


DEF_BUCKET_ARRAY(Import)

/*
    A compilation with build options and internal structures.
    A driver normally has one but user can have a build script
    that submits multiple compilations.

    This also scales well with the API.

    Each normal compilation produces an object file.
    A non-normal compilation can:
      - run code JIT-style
      - link object file into executable
*/
typedef struct Compilation {
    const BasinCompileOptions* options;
    // build options
    //    output paths
    //    import paths
    //    optimization level
    //    debug level
    // initital source file or text
    
    // internal structures, AST, IR, Machine programs
    // indexes and IDs are relative to this struct

    IRProgram* program;
    // Cache them here so we don't have to look for them
    IRSection* section_rodata;
    IRSection* section_data;
    IRSectionID sectionid_rodata;
    IRSectionID sectionid_data;

    MachineProgram* machine_program;
    
    Array_string import_dirs;
    Array_string library_dirs;

    volatile u32 pending_tasks;
    volatile u32 active_tasks;

    // Keeping driver here lets us pass around Compilation to functions
    // without also specifying the driver.
    Driver* driver;
} Compilation;

typedef enum {
    SUCCESS,
    FAILURE,
} ResultKind;

typedef struct {
    ResultKind kind;
    string message;
} Result;

typedef struct {
    const char* path;
    int line;
} CLocation;

#define CLOCATION { __FILE__, __LINE__ }

#define ARRAY_LENGTH(A) (sizeof(A)/sizeof(*(A)))

#ifdef _WIN32
    #define FL "%ll"
#else
    #define FL "%l"
#endif


static inline BasinTargetFormat determine_format(const BasinCompileOptions* options) {
    if (options->target_format != BASIN_TARGET_FORMAT_host)
        return options->target_format;
    
    if (options->target_os == BASIN_TARGET_OS_linux)
        return BASIN_TARGET_FORMAT_ELF;
    else if (options->target_os == BASIN_TARGET_OS_windows)
        return BASIN_TARGET_FORMAT_COFF;
    
    #ifdef OS_WINDOWS
        return BASIN_TARGET_FORMAT_COFF;
    #elif defined(OS_LINUX)
        return BASIN_TARGET_FORMAT_ELF;
    #endif
}

static inline BasinTargetArch determine_arch(const BasinCompileOptions* options) {
    if (options->target_arch != BASIN_TARGET_FORMAT_host)
        return options->target_format;
    
    #ifdef ARCH_X86_64
        return BASIN_TARGET_ARCH_x86_64;
    #elif defined(ARCH_X86_32)
        return BASIN_TARGET_ARCH_x86_32;
    #elif defined(ARCH_ARM_64)
        return BASIN_TARGET_ARCH_arm_64;
    #elif defined(ARCH_ARM_32)
        return BASIN_TARGET_ARCH_arm_32;
    #else
        // @TODO Defaulting to x86 for now.
        return BASIN_TARGET_ARCH_x86_64;
    #endif
}


static inline BasinTargetArch determine_os(const BasinCompileOptions* options) {
    if (options->target_os != BASIN_TARGET_OS_host)
        return options->target_os;
    
    #ifdef OS_WINDOWS
        return BASIN_TARGET_OS_windows;
    #elif defined(OS_LINUX)
        return BASIN_TARGET_OS_linux;
    #endif
}
