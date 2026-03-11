#include "basin/backend/ir.h"

#include "basin/common.h"

static inline void reserve_code(IRBuilder* builder) {
    if (builder->function->code_len + 256 >= builder->function->code_cap) {
        int new_cap = builder->function->code_cap * 2 + 256;
        u8* new_code = mem__allocate(new_cap, builder->function->code);
        builder->function->code = new_code;
        builder->function->code_cap = new_cap;
    }
}

#define IR_PRELUDE() reserve_code(builder)

#define NEXT_INST(T) (T*)(builder->function->code + builder->function->code_len)

void ir_load(IRBuilder* builder, int reg, int reg_mem, int offset, IRType type) {
    IR_PRELUDE();

    IRInstruction_load* inst = NEXT_INST(IRInstruction_load);
    builder->function->code_len += sizeof(IRInstruction_load);

    inst->opcode = IR_LOAD;
    inst->output = reg;
    inst->memory = reg_mem;
    inst->displacement = offset;
}
void ir_store(IRBuilder* builder, int reg_mem, int reg, int offset, IRType type) {
    IR_PRELUDE();
    
    IRInstruction_store* inst = NEXT_INST(IRInstruction_store);
    builder->function->code_len += sizeof(IRInstruction_store);

    inst->opcode = IR_STORE;
    inst->input = reg;
    inst->memory = reg_mem;
    inst->displacement = offset;
}

void ir_add(IRBuilder* builder, int reg_dst, int reg0, int reg1, IRType type) {
    IR_PRELUDE();
    
    IRInstruction_op3* inst = NEXT_INST(IRInstruction_op3);
    builder->function->code_len += sizeof(IRInstruction_op3);

    inst->opcode = IR_ADD;
    inst->output = reg_dst;
    inst->input0 = reg0;
    inst->input1 = reg1;
}
void ir_sub(IRBuilder* builder, int reg_dst, int reg0, int reg1, IRType type) {
    IR_PRELUDE();

    IRInstruction_op3* inst = NEXT_INST(IRInstruction_op3);
    builder->function->code_len += sizeof(IRInstruction_op3);

    inst->opcode = IR_SUB;
    inst->output = reg_dst;
    inst->input0 = reg0;
    inst->input1 = reg1;
}
void ir_mul(IRBuilder* builder, int reg_dst, int reg0, int reg1, IRType type) {
    IR_PRELUDE();
    
    IRInstruction_op3* inst = NEXT_INST(IRInstruction_op3);
    builder->function->code_len += sizeof(IRInstruction_op3);

    inst->opcode = IR_MUL;
    inst->output = reg_dst;
    inst->input0 = reg0;
    inst->input1 = reg1;
}
void ir_div(IRBuilder* builder, int reg_dst, int reg0, int reg1, IRType type) {
    IR_PRELUDE();

    IRInstruction_op3* inst = NEXT_INST(IRInstruction_op3);
    builder->function->code_len += sizeof(IRInstruction_op3);

    inst->opcode = IR_DIV;
    inst->output = reg_dst;
    inst->input0 = reg0;
    inst->input1 = reg1;
}
void ir_mod(IRBuilder* builder, int reg_dst, int reg0, int reg1, IRType type) {
    IR_PRELUDE();

    IRInstruction_op3* inst = NEXT_INST(IRInstruction_op3);
    builder->function->code_len += sizeof(IRInstruction_op3);

    inst->opcode = IR_MOD;
    inst->output = reg_dst;
    inst->input0 = reg0;
    inst->input1 = reg1;
}

void ir_imm8(IRBuilder* builder, int reg, i8 imm, IRType type) {
    IR_PRELUDE();

    IRInstruction_imm8* inst = NEXT_INST(IRInstruction_imm8);
    builder->function->code_len += sizeof(IRInstruction_imm8);

    inst->opcode = IR_IMM8;
    inst->output = reg;
    inst->type = type;
    inst->immediate = imm;
}
void ir_imm16(IRBuilder* builder, int reg, i16 imm, IRType type) {
    IR_PRELUDE();

    IRInstruction_imm16* inst = NEXT_INST(IRInstruction_imm16);
    builder->function->code_len += sizeof(IRInstruction_imm16);

    inst->opcode = IR_IMM16;
    inst->output = reg;
    inst->type = type;
    inst->immediate = imm;
}
void ir_imm32(IRBuilder* builder, int reg, i32 imm, IRType type) {
    IR_PRELUDE();

    IRInstruction_imm32* inst = NEXT_INST(IRInstruction_imm32);
    builder->function->code_len += sizeof(IRInstruction_imm32);

    inst->opcode = IR_IMM32;
    inst->output = reg;
    inst->type = type;
    inst->immediate = imm;
}
void ir_imm64(IRBuilder* builder, int reg, i64 imm, IRType type) {
    IR_PRELUDE();
    
    IRInstruction_imm64* inst = NEXT_INST(IRInstruction_imm64);
    builder->function->code_len += sizeof(IRInstruction_imm64);

    inst->opcode = IR_IMM64;
    inst->output = reg;
    inst->type = type;
    inst->immediate = imm;
}

void ir_call(IRBuilder* builder, IRFunction_id func_id, u8 arg_count, u8 ret_count, IROperand* args, IROperand* ret_values, IROperand* ret_types) {
    IR_PRELUDE();

    IRInstruction_call* inst = NEXT_INST(IRInstruction_call);
    builder->function->code_len += sizeof(IRInstruction_call) + (arg_count + ret_count) * sizeof(IROperand);

    inst->opcode = IR_CALL;
    inst->function_id = func_id;
    inst->arg_count = arg_count;
    inst->ret_count = ret_count;
    memcpy(inst->_data, args, arg_count);
    for (int i=0;i<ret_count;i++) {
        inst->_data[arg_count + 2*i] = ret_values[i];
        inst->_data[arg_count + 2*i+1] = ret_types[i];
    }
}

void ir_ret(IRBuilder* builder, u8 ret_count, IROperand* operands) {
    IR_PRELUDE();

    IRInstruction_ret* inst = NEXT_INST(IRInstruction_ret);
    builder->function->code_len += sizeof(IRInstruction_ret) + (ret_count) * sizeof(IROperand);

    inst->opcode = IR_RET;
    inst->ret_count = ret_count;
    if (ret_count > 0) {
        memcpy(inst->operands, operands, ret_count);
    }
}


void ir_address_of_variable(IRBuilder* builder, int reg, IRSectionID section, int offset) {
    IR_PRELUDE();

    IRInstruction_address_of_variable* inst = NEXT_INST(IRInstruction_address_of_variable);
    builder->function->code_len += sizeof(IRInstruction_address_of_variable);

    inst->opcode = IR_ADDRESS_OF_VARIABLE;
    inst->output = reg;
    inst->section = section;
    inst->offset = offset;
}


void print_ir_function(IRProgram* program, IRFunction* function) {
    int head = 0;

    #define print(...) printf("  " __VA_ARGS__)
    #define aprint(...) printf(__VA_ARGS__)

    while (head < function->code_len) {
        IROpcode* opcode = &function->code[head];

        switch(*opcode) {
            case IR_ADD: {
                IRInstruction_op3* inst = (IRInstruction_op3*)opcode;
                print("add r%u, r%u, r%u\n", inst->output, inst->input0, inst->input1);
                head += sizeof(IRInstruction_op3);
            } break;
            case IR_SUB: {
                IRInstruction_op3* inst = (IRInstruction_op3*)opcode;
                print("sub r%u, r%u, r%u\n", inst->output, inst->input0, inst->input1);
                head += sizeof(IRInstruction_op3);
            } break;
            case IR_MUL: {
                IRInstruction_op3* inst = (IRInstruction_op3*)opcode;
                print("mul r%u, r%u, r%u\n", inst->output, inst->input0, inst->input1);
                head += sizeof(IRInstruction_op3);
            } break;
            case IR_DIV: {
                IRInstruction_op3* inst = (IRInstruction_op3*)opcode;
                print("div r%u, r%u, r%u\n", inst->output, inst->input0, inst->input1);
                head += sizeof(IRInstruction_op3);
            } break;
            case IR_MOD: {
                IRInstruction_op3* inst = (IRInstruction_op3*)opcode;
                print("mod r%u, r%u, r%u\n", inst->output, inst->input0, inst->input1);
                head += sizeof(IRInstruction_op3);
            } break;

            // IR_BIT_OR,
            // IR_BIT_AND,
            // IR_BIT_XOR,
            // IR_BIT_LSHIFT,
            // IR_BIT_RSHIFT,
            // IR_BIT_NEGATE,

            // IR_NOT, // logical operations, not bitwise
            // IR_AND,
            // IR_OR,

            // IR_EQUAL,
            // IR_NOT_EQUAL,
            // IR_LESS,
            // IR_GREATER,
            // IR_LESS_EQUAL,
            // IR_GREATER_EQUAL,

            case IR_LOAD: {
                IRInstruction_load* inst = (IRInstruction_load*)opcode;
                print("load r%u, [r%u%s%u]\n", inst->output, inst->memory, (inst->displacement >= 0 ? " + " : " "), inst->displacement);
                head += sizeof(IRInstruction_load);
            } break;
            case IR_STORE: {
                IRInstruction_store* inst = (IRInstruction_store*)opcode;
                print("store [r%u%s%u], r%u\n", inst->memory, (inst->displacement >= 0 ? " + " : " "), inst->displacement, inst->input);
                head += sizeof(IRInstruction_store);
            } break;

            case IR_ADDRESS_OF_VARIABLE: {
                IRInstruction_address_of_variable* inst = (IRInstruction_address_of_variable*)opcode;
                const string section_name = atomic_array_get(&program->sections, inst->section).name;
                print("var_addr r%u, [%s + %u]\n", inst->output, section_name.ptr, inst->offset);
                head += sizeof(IRInstruction_address_of_variable);
            } break;
            case IR_ADDRESS_OF_FUNCTION: {
                IRInstruction_address_of_function* inst = (IRInstruction_address_of_function*)opcode;
                print("func_addr ?\n");
                head += sizeof(IRInstruction_address_of_variable);
            } break;

            case IR_IMM8:{
                IRInstruction_imm8* inst = (IRInstruction_imm8*)opcode;
                print("imm r%u, %d\n", inst->output, (int)inst->immediate);
                head += sizeof(IRInstruction_imm8);
            } break;
            case IR_IMM16:{
                IRInstruction_imm16* inst = (IRInstruction_imm16*)opcode;
                print("imm r%u, %d\n", inst->output, (int)inst->immediate);
                head += sizeof(IRInstruction_imm16);
            } break;
            case IR_IMM32:{
                IRInstruction_imm32* inst = (IRInstruction_imm32*)opcode;
                print("imm r%u, %d\n", inst->output, (int)inst->immediate);
                head += sizeof(IRInstruction_imm32);
            } break;
            case IR_IMM64: {
                IRInstruction_imm64* inst = (IRInstruction_imm64*)opcode;
                print("imm r%u, "FL"d\n", inst->output, inst->immediate);
                head += sizeof(IRInstruction_imm64);
            } break;

            // IR_JMP,
            // IR_JMP_NON_ZERO,
            // IR_JMP_ZERO,
            case IR_CALL: {
                IRInstruction_call* inst = (IRInstruction_call*)opcode;
                IRFunction* callee = atomic_array_getptr(&program->functions, inst->function_id);
                print("call %s", callee->name.ptr);
                for (int i=0;i<inst->arg_count;i++) {
                    aprint(", r%u", CALL_GET_ARG(inst, i));
                }
                aprint(" -> ");
                for (int i=0;i<inst->ret_count;i++) {
                    if (i != 0) {
                        aprint(", ");
                    }
                    aprint("r%u", CALL_GET_RET_VALUE(inst, i));
                }
                aprint("\n");

                head += sizeof(IRInstruction_call) + inst->arg_count + inst->ret_count;
            } break;
            // IR_CALL_PTR,
            case IR_RET: {
                IRInstruction_ret* inst = (IRInstruction_ret*)opcode;
                print("ret ");
                for (int i=0;i<inst->ret_count;i++) {
                    if (i != 0) {
                        aprint(", ");
                    }
                    aprint("r%u", inst->operands[i]);
                }
                aprint("\n");

                head += sizeof(IRInstruction_ret) + inst->ret_count;
            } break;

            // IR_ASSEMBLY,
            default: print("unknown\n"); return;
        }
    }
}
