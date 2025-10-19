/*
    Interface for Basin backend.

    Generates machine code from Basin's bytecode (intermediate representation)
*/

#pragma once

#include "basin/basin.h" // needs BasinAllocator
#include "basin/backend/ir.h"


//###################################
//     PUBLIC TYPES AND STRUCTS
//##################################


// 32-bit integer
typedef enum {
    CPU_none,

    CPU_x86            = 0x0100,            // 32-bit x86
    CPU_x86_64         = CPU_x86    | 0x80, // 64-bit

    CPU_ARM            = 0x0200, // generic ARM
    CPU_ARMv6          = CPU_ARM  | 0x01, // 32-bit
    // CPU_ARMv7          = CPU_ARM  | 0x02,
    // CPU_ARMv8          = CPU_ARM  | 0x03,
    // CPU_ARMv8_64       = CPU_ARM  | 0x83, // 64-bit
} CPUKind;

// 32-bit integer
typedef enum {
    HOST_none,

    HOST_baremetal,
    HOST_windows,
    HOST_linux,

    HOST_KIND_MAX,
} HostKind;

typedef struct {
    CPUKind cpu_kind;
    HostKind host_kind;
    union {
        struct {
            // unsigned int has_avx : 1;
            // unsigned int has_avx512 : 1;
        };
        char _reserved[8];
    };
} PlatformOptions;


typedef struct {
    IRFunction_id id;
    
    u8* code;
    int code_len;
    int code_max;

    // TODO: Debug information
} CodegenFunction;

typedef enum {
    CODEGEN_SUCCESS,
    CODEGEN_INVALID_PLATFORM_OPTIONS,
    CODEGEN_INVALID_IR_CODE,
} CodegenError;

typedef struct {
    CodegenError error_type;
    char* error_message;
} CodegenResult;

//###################################
//        PUBLIC FUNCTIONS
//##################################


CodegenResult codegen_generate_function(const IRFunction* in_function, CodegenFunction** out_function, const PlatformOptions* options, BasinAllocator* allocator);


const char* platform_string(const PlatformOptions* options);

const char* cpu_kind_string(CPUKind kind);

const char* host_kind_string(HostKind kind);

