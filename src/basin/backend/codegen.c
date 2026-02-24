#include "basin/backend/codegen.h"

#include "basin/core/driver.h"
#include "basin/backend/ir.h"

#include "basin/backend/x86_gen.h"
#include "basin/backend/x86_defs.h"

#include "basin/frontend/ast.h"

#include "basin/logger.h"

#include <setjmp.h>
#include <stdarg.h>

static CodegenResult validate_platform_options(const PlatformOptions* options);


typedef struct Instruction Instruction;
struct Instruction {
    // IROpcode opcode;
    IROpcode* base;
    
    union {
        struct {
            Instruction* output;
            Instruction* input0;
            Instruction* input1;
        };
        Instruction** inputs_outputs;
    };
};

typedef struct MachineDataObject {
    int machine_register;
    // int stack_offset;
} MachineDataObject;

typedef struct {
    const IRFunction* ir_func;
    CodegenFunction* machine_func;
    
    Instruction* reg_to_inst_mapping[256];

    Instruction* instructions;
    int instructions_len;
    int instructions_cap;

    Instruction** operands;
    int operands_len;
    int operands_cap;

    Instruction** inst_sequence;
    int inst_sequence_len;
    int inst_sequence_cap;

    MachineDataObject reg_to_machine_register[256];

    bool used_machine_registers[256];

    // jmp_buf jump_state;
    // SourceLocation bad_location;
    // CLocation c_location;
    // char error_message[512];
    
    // // Zones for tracy are cleaned up when long jumping.
    // int zones_cap;
    // int zones_len;
    // TracyCZoneCtx* zones;
} CodegenContext;


#define PROFILE_START() TracyCZone(zone, 1)

#define PROFILE_END() TracyCZoneEnd(zone)


//###########################
//      PUBLIC FUNCTIONS
//###########################

void x86_generate(CodegenContext* context);


CodegenResult codegen_generate_function(Driver* driver, const IRFunction* in_function, CodegenFunction** out_function, const PlatformOptions* options) {
    PROFILE_START();
    
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
    context.machine_func = mem__alloc(sizeof(CodegenFunction));
    memset(context.machine_func, 0, sizeof(CodegenFunction));

    x86_generate(&context);

    dump_hex(context.machine_func->code, context.machine_func->code_len, 12);

    // on success do this
    *out_function = context.machine_func;
    PROFILE_END();
    return result;
}

// void codegen_generate(Driver* driver, const Import* ast_import) {
//     PROFILE_START();
    
//     for (int fi=0;fi<ast_import->;fi++) {

//     CodegenContext context = {};
//     context.ir_func = in_function;
//     context.machine_func = mem__alloc(sizeof(CodegenFunction));

//     x86_generate(&context);

//     PROFILE_END();
// }

//#############################
//     PRIVATE FUNCTIONS
//#############################ty

void reserve_code_sequence(CodegenContext* context) {
    if (context->inst_sequence_len + 1 >= context->inst_sequence_cap) {
        int new_cap = context->inst_sequence_cap*2 + 256;
        Instruction** new_insts = mem__allocate(new_cap * sizeof(Instruction*), context->inst_sequence);
        context->inst_sequence = new_insts;
        context->inst_sequence_cap = new_cap;
    }
}
Instruction* alloc_inst(CodegenContext* context) {
    if (context->instructions_len + 1 >= context->instructions_cap) {
        int new_cap = context->instructions_cap*2 + 256;
        Instruction* new_insts = mem__allocate(new_cap * sizeof(Instruction), context->instructions);
        context->instructions = new_insts;
        context->instructions_cap = new_cap;
    }

    Instruction* inst = &context->instructions[context->instructions_len];
    context->instructions_len++;
    return inst;
}

Instruction** alloc_operands(CodegenContext* context, int count) {
    if (context->operands_len + count >= context->operands_cap) {
        // @TODO We leak memory for previous IROperand array.
        //    We'll improve the 'Instruction' struct later so it's fine for now.
        //    I must explore the complexity before I know which layout to use.
        int new_cap = context->operands_cap*2 + count + 20;
        Instruction** new_operands = mem__allocate(new_cap * sizeof(Instruction*), NULL);
        context->operands = new_operands;
        context->operands_cap = new_cap;
        context->operands_len = 0;
    }

    Instruction** operands = &context->operands[context->operands_len];
    context->operands_len += count;
    return operands;
}

int alloc_machine_register(CodegenContext* context) {
    int found = -1;
    for (int i=0;i<ARRAY_LENGTH(context->used_machine_registers);i++) {
        if (i == X64_REG_SP || i == X64_REG_BP)
            continue;
        if (!context->used_machine_registers[i]) {
            found = i;
            break;
        }
    }
    ASSERT(found != -1);

    context->used_machine_registers[found] = true;
    return found;

}

void x86_generate(CodegenContext* context) {
    PROFILE_START();
    const IRFunction* ir = context->ir_func;
    CodegenFunction* mac = context->machine_func;

    /*
    
        var_addr r1, [.stack + 4]
            lea rax, [rsp + 4]
        imm r2, 7
            mov ecx, 7
        var_addr r3, [.rodata + 0]
            lea rdx, [.rodata + 0]
            RELOCATION .rodata
        store [r1 + 0], r3
            mov [rax], rdx
        store [r1 + 8], r2
            mov [rax + 8], ecx
        imm r1, 1
            mov eax, 1
        var_addr r2, [.stack + 16]
        load r2, [r2 + 6]
        var_addr r3, [.stack + 16]
        load r3, [r3 + 6]
        call main, r1, r2, r3 -> r4
        imm r5, 5
        ret r5
    */

    int head = 0;

    #define APPEND_INST() context->inst_sequence[context->inst_sequence_len++] = inst
    
    // 
    // Compute instruction dependencies
    // 

    while (head < ir->code_len) {
        IROpcode* opcode = (IROpcode*)&ir->code[head];
        
        // @TODO Optimize, IR builder can keep a count on number of instructions, we can reserve that
        //    many because we'll never need more. This buffer can be kept per thread.
        reserve_code_sequence(context);
        
        switch(*opcode) {
            case IR_ADD: {
                
            } break;
            case IR_LOAD: {
                IRInstruction_load* irinst = (IRInstruction_load*)opcode;
                Instruction* inst = alloc_inst(context);
                inst->base = (IROpcode*)irinst;
                inst->input0 = context->reg_to_inst_mapping[irinst->memory];
                
                context->reg_to_inst_mapping[irinst->output] = inst;
                head += sizeof(IRInstruction_load);
                APPEND_INST();
            } break;
            case IR_STORE: {
                IRInstruction_store* irinst = (IRInstruction_store*)opcode;
                Instruction* inst = alloc_inst(context);
                inst->base = (IROpcode*)irinst;
                inst->input0 = context->reg_to_inst_mapping[irinst->memory];
                inst->input1 = context->reg_to_inst_mapping[irinst->input];

                head += sizeof(IRInstruction_store);
                APPEND_INST();
            } break;
            case IR_CALL: {
                IRInstruction_call* irinst = (IRInstruction_call*)opcode;
                Instruction* inst = alloc_inst(context);
                inst->base = (IROpcode*)irinst;
                if (irinst->arg_count + irinst->ret_count > 0) {
                    inst->inputs_outputs = alloc_operands(context, irinst->arg_count + irinst->ret_count);
                    for (int i=0;i<irinst->arg_count;i++) {
                        inst->inputs_outputs[i] = context->reg_to_inst_mapping[irinst->operands[i]];
                    }
                    for (int i=0;i<irinst->ret_count;i++) {
                        context->reg_to_inst_mapping[irinst->operands[irinst->arg_count + i]] = inst;
                    }
                }
                
                head += sizeof(IRInstruction_call) + irinst->arg_count + irinst->ret_count;
                APPEND_INST();
            } break;
            case IR_IMM8:
            case IR_IMM16:
            case IR_IMM32:
            case IR_IMM64: {
                IRInstruction_imm* irinst = (IRInstruction_imm*)opcode;
                Instruction* inst = alloc_inst(context);
                inst->base = (IROpcode*)irinst;
                
                context->reg_to_inst_mapping[irinst->output] = inst;
                head += sizeof(IRInstruction_imm);
                APPEND_INST();
            } break;
            case IR_ADDRESS_OF_VARIABLE: {
                IRInstruction_address_of_variable* irinst = (IRInstruction_address_of_variable*)opcode;
                Instruction* inst = alloc_inst(context);
                inst->base = (IROpcode*)irinst;
                
                context->reg_to_inst_mapping[irinst->output] = inst;
                head += sizeof(IRInstruction_address_of_variable);
                APPEND_INST();
            } break;
            case IR_RET: {
                IRInstruction_ret* irinst = (IRInstruction_ret*)opcode;
                Instruction* inst = alloc_inst(context);
                inst->base = (IROpcode*)irinst;
                if (irinst->ret_count > 0) {
                    inst->inputs_outputs = alloc_operands(context, irinst->ret_count);
                    for (int i=0;i<irinst->ret_count;i++) {
                        inst->inputs_outputs[i] = context->reg_to_inst_mapping[irinst->operands[i]];
                    }
                }
                
                head += sizeof(IRInstruction_ret) + irinst->ret_count;
                APPEND_INST();
            } break;
        }
    }

    // 
    // Compute register allocations
    // 

    /*
        Complexities to deal with:
        - Running out of x86 registers. Store stuff that should have been in registers on stack.
        - Branching. Do we optimize and keep values in registers before and after branching of a for loop?
        - Function calls. Non volatile registers.
    */

    X86Builder _builder = {};
    _builder.function = context->machine_func;
    X86Builder* builder = &_builder;

    for (int i=0;i<context->inst_sequence_len;i++) {
        Instruction* inst = context->inst_sequence[i];

        switch (*inst->base) {
            case IR_ADDRESS_OF_VARIABLE: {
                IRInstruction_address_of_variable* ir_inst = (IRInstruction_address_of_variable*)inst->base;
                
                if (ir_inst->section == SECTION_ID_STACK) {
                    int machine_reg = alloc_machine_register(context);
                    context->reg_to_machine_register[ir_inst->output].machine_register = machine_reg;

                    x86_emit_lea(builder, machine_reg, X64_REG_SP, ir_inst->offset);

                } else {
                    int machine_reg = alloc_machine_register(context);
                    context->reg_to_machine_register[ir_inst->output].machine_register = machine_reg;

                    u32 fixup_address;
                    x86_emit_lea_rip(builder, machine_reg, &fixup_address);

                    printf("  %04x: lea %d, [rip+?] (requires relocation)\n", fixup_address, machine_reg);
                }
            } break;
            case IR_IMM8:
            case IR_IMM16:
            case IR_IMM32:
            case IR_IMM64: {
                IRInstruction_imm* ir_inst = (IRInstruction_imm*)inst->base;
                
                // @TODO How do we know when we are done with the register?
                //   We have limited machine registers and must free them.
                //   If we need to keep the register values then we put onto the stack
                //   until we need them again. To be efficient we should put the register
                //   that is used 7 instructions from now rather than registers that are used
                //   in the next 3. How to calculate register usage like this?
                //   What about jumps, basic blocks, and function calls?

                int machine_reg = alloc_machine_register(context);
                context->reg_to_machine_register[ir_inst->output].machine_register = machine_reg;

                x86_emit_imm4(builder, machine_reg, ir_inst->immediate);
            } break;
            case IR_LOAD: {
                IRInstruction_load* ir_inst = (IRInstruction_load*)inst->base;
                
                int machine_reg = alloc_machine_register(context);
                context->reg_to_machine_register[ir_inst->output].machine_register = machine_reg;
                int machine_mem_reg = context->reg_to_machine_register[ir_inst->memory].machine_register;

                x86_emit_load(builder, machine_reg, machine_mem_reg, ir_inst->displacement);
            } break;
            case IR_STORE: {
                IRInstruction_store* ir_inst = (IRInstruction_store*)inst->base;
                
                int machine_src_reg = context->reg_to_machine_register[ir_inst->input].machine_register;
                int machine_mem_reg = context->reg_to_machine_register[ir_inst->memory].machine_register;

                x86_emit_store(builder, machine_src_reg, machine_mem_reg, ir_inst->displacement);
            } break;
        }

    }

    // 


    //
    // Code generation
    // 

    // optimization

    // register allocation

    // reuse patterns for x86, ARM, powerpc, RISC V, arduino arch
    
    PROFILE_END();
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
