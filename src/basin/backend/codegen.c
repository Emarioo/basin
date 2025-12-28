#include "basin/backend/codegen.h"



static CodegenResult validate_platform_options(const PlatformOptions* options);


typedef struct {
    const IRFunction* ir_func;
    CodegenFunction* machine_func;
} CodegenContext;

//###########################
//      PUBLIC FUNCTIONS
//###########################

void x86_generate(CodegenContext* context);


CodegenResult codegen_generate_function(const IRFunction* in_function, CodegenFunction** out_function, const PlatformOptions* options, BasinAllocator* allocator) {
    CodegenResult result = {};
    result.error_type = CODEGEN_SUCCESS;
    result.error_message = NULL;
    *out_function = NULL;

    result = validate_platform_options(options);
    if (result.error_type != CODEGEN_SUCCESS)
        return result;

    // switch(options->cpu_kind) {
    //     case CPU_x86_64:
    //         gen_x86();
    //     case CPU_ARMv6:
    //         gen_arm();
    // }

    // debug info to consider

    // whether it's entry point or not

    // nocheckin TODO: DO NOT HEAP ALLOC LIKE THIS, call allocator->heap_alloc
    
    
    CodegenContext context = {};
    context.ir_func = in_function;
    context.machine_func = allocator->allocate(sizeof(CodegenFunction), NULL, allocator->user_data);

    x86_generate(&context);

    // on success do this
    *out_function = context.machine_func;
    return result;
}




//#############################
//     PRIVATE FUNCTIONS
//#############################

void x86_generate(CodegenContext* context) {
    const IRFunction* ir = context->ir_func;
    CodegenFunction* mac = context->machine_func;

    // optimization

    // register allocation

    // reuse patterns for x86, ARM, powerpc, RISC V, arduino arch
    
    // x86_emit_mov_rr(builder, reg1, reg2, operand_size);
}




static CodegenResult validate_platform_options(const PlatformOptions* options) {
    CodegenResult result = {};
    result.error_message = NULL;
    result.error_type = CODEGEN_SUCCESS;

    switch (options->cpu_kind) {
        case CPU_x86_64: break;
        default:
            result.error_message = "Invalid CPU kind options";
            result.error_type = CODEGEN_INVALID_PLATFORM_OPTIONS;
            return result;
    }

    switch (options->host_kind) {
        case HOST_windows:
        case HOST_linux: break;
        default:
            result.error_message = "Invalid HOST kind options";
            result.error_type = CODEGEN_INVALID_PLATFORM_OPTIONS;
            return result;
    }

    return result;

LABEL_invalid:
}

const char* platform_string(const PlatformOptions* options) {
    static char buffer[128];
    snprintf(buffer, sizeof(buffer), "cpu: %s, host: %s", cpu_kind_string(options->cpu_kind), host_kind_string(options->host_kind));
}

const char* cpu_kind_string(CPUKind kind) {
    switch(kind) {
        case CPU_x86:    return "x86";
        case CPU_x86_64: return "x86";
        case CPU_ARM: return "ARM";
        case CPU_ARMv6: return "ARMv6";
    }
    ASSERT((false, "invalid cpu kind"));
    return NULL;
}

const char* host_kind_string(HostKind kind) {
    static const char* names[HOST_KIND_MAX] = {
        "none",
        "baremetal",
        "windows",
        "linux",
    };
    ASSERT_INDEX(kind >= 0 && kind <= HOST_KIND_MAX);
    return names[kind];
}
