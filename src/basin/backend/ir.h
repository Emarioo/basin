/*
    Intermediate Representation (IR)

    A compact representation of a program which is understood by the backend.


    The IR does not represent structs. Pointers are just U64 (or U32 depending on architecture).
    This means calling convention of struct passing is 


    v : struct16 = [ ptr0, ptr1 ]




*/

#pragma once

#include "util/array.h"
#include "util/atomic_array.h"
#include "util/string.h"
#include "platform/platform.h"

typedef enum {
    IR_ADD,
    IR_SUB,
    IR_MUL,
    IR_DIV,
    IR_MOD,

    IR_BIT_OR,
    IR_BIT_AND,
    IR_BIT_XOR,
    IR_BIT_LSHIFT,
    IR_BIT_RSHIFT,
    IR_BIT_NEGATE,

    IR_NOT, // logical operations, not bitwise
    IR_AND,
    IR_OR,

    IR_EQUAL,
    IR_NOT_EQUAL,
    IR_LESS,
    IR_GREATER,
    IR_LESS_EQUAL,
    IR_GREATER_EQUAL,

    IR_LOAD,
    IR_STORE,

    IR_ADDRESS_OF_VARIABLE,
    IR_ADDRESS_OF_FUNCTION,

    IR_IMM8,
    IR_IMM16,
    IR_IMM32,
    IR_IMM64,

    IR_JMP,
    IR_JMP_NON_ZERO,
    IR_JMP_ZERO,
    IR_CALL,
    IR_CALL_PTR,
    IR_RET,

    IR_ASSEMBLY,

    // @TODO Vector instructions

    IR_EXTEND1 = 253, // extended opcode
    IR_EXTEND2 = 254,
    IR_RESERVED_255 = 255,
} _IROpcode;

typedef enum {
    IR_SECTION_LOCAL,
    IR_SECTION_PARAMETER,
    IR_SECTION_DATA,
} _IRSectionID;

#define SECTION_ID_STACK 0


// bool, char, pointers are integers
typedef enum {
    IR_TYPE_U8 = 0x00,
    IR_TYPE_U16,
    IR_TYPE_U32,
    IR_TYPE_U64,
    IR_TYPE_U128,
    IR_TYPE_U256,
    IR_TYPE_U512,

    IR_TYPE_S8 = 0x10,
    IR_TYPE_S16,
    IR_TYPE_S32,
    IR_TYPE_S64,
    IR_TYPE_S128,
    IR_TYPE_S256,
    IR_TYPE_S512,
    
    
    IR_TYPE_F8 = 0x20,
    IR_TYPE_F16, // we add f8, f16 for completeness, we don't actually support these.
    IR_TYPE_F32,
    IR_TYPE_F64,
    IR_TYPE_F128,
    IR_TYPE_F256,
    IR_TYPE_F512,
} _IRType;

#define BYTE_SIZE_OF_IR_TYPE(T) (1 << ((T) & 0xF))

#define IR_TYPE_IS_FLOAT(T)     ((T & ~0xf) == IR_TYPE_F8)
#define IR_TYPE_IS_SIGNED(T)    ((T & ~0xf) == IR_TYPE_S8)
#define IR_TYPE_IS_UNSIGNED(T)  ((T & ~0xf) == IR_TYPE_U8)

typedef u8  IRType;
typedef u8  IROpcode;
typedef u8  IROperand;
typedef u8  IRSectionID;
typedef u32 IRLabel;
typedef u32 IRFunction_id;

#pragma pack(push, 1)

typedef struct {
    IROpcode opcode;
    IROperand output;
    IROperand input0;
    IROperand input1;
} IRInstruction_op3;

typedef struct {
    IROpcode opcode;
    IROperand output;
    IROperand input;
} IRInstruction_op2;

typedef struct {
    IROpcode opcode;
    IROperand output;
    IROperand memory;
    IRType type;
    int displacement;
} IRInstruction_load;

typedef struct {
    IROpcode opcode;
    IROperand input;
    IROperand memory;
    int displacement;
} IRInstruction_store;

typedef struct {
    IROpcode opcode;
    IROperand output;
    IRSectionID section;
    u32 offset;
} IRInstruction_address_of_variable;

typedef struct {
    IROpcode opcode;
    IROperand output;
    IRFunction_id function_id;
} IRInstruction_address_of_function;

typedef struct {
    IROpcode opcode;
    IROperand output;
    IRType type;
    i8 immediate;
} IRInstruction_imm8;

typedef struct {
    IROpcode opcode;
    IROperand output;
    IRType type;
    i16 immediate;
} IRInstruction_imm16;

typedef struct {
    IROpcode opcode;
    IROperand output;
    IRType type;
    i32 immediate;
} IRInstruction_imm32;

typedef struct {
    IROpcode opcode;
    IROperand output;
    IRType type;
    i64 immediate;
} IRInstruction_imm64; // larger immediates should be put in .rodata section.

typedef struct {
    IROpcode opcode;
    IRLabel label;
} IRInstruction_jmp;

typedef struct {
    IROpcode opcode;
    IROpcode input;
    IRLabel label;
} IRInstruction_jmp_non_zero;

typedef struct {
    IROpcode opcode;
    IROpcode input;
    IRLabel label;
} IRInstruction_jmp_zero;

typedef struct {
    IROpcode opcode;
    IRFunction_id function_id;
    u8 arg_count;
    u8 ret_count;
    u8 _data[/* arg_count + 2*ret_count */]; // use macros to access arguments and return value
} IRInstruction_call;

#define CALL_GET_ARG(IRINST, INDEX) ((IROperand)(IRINST)->_data[INDEX])
#define CALL_GET_RET_VALUE(IRINST, INDEX) ((IROperand)(IRINST)->_data[(IRINST)->arg_count + 2*(INDEX)])
#define CALL_GET_RET_TYPE(IRINST, INDEX) ((IRType)(IRINST)->_data[(IRINST)->arg_count + 1 + 2*(INDEX)])

typedef struct {
    IROpcode opcode;
    IROperand input_ptr;
    u8 arg_count;
    u8 ret_count;
    u8 _data[/* arg_count + 2*ret_count */]; // use macros to access arguments and return value
    // IROperand operands[/* arg_count + ret_count */];
} IRInstruction_call_ptr;

typedef struct {
    IROpcode opcode;
    u8 ret_count;
    IROperand operands[/* ret_count */];
} IRInstruction_ret;


#pragma pack(pop)


typedef struct {
    IRFunction_id id;
    string name;

    // debug info
    // number of instructions (for statistics)

    int frame_size;

    u8* code;
    int code_len;
    int code_cap;
} IRFunction;

typedef struct IRDataObject {
    string name;
    u8     section_index;  // index into IRCollection's IRSections
    u8     type;           // type
    int    section_offset; // Offset into IRSection where data is located
    int    size;           // size of variable
} IRDataObject;

typedef struct IRSection {
    string name;
    u64 data_len;
    u64 data_cap;
    u8* data;
} IRSection;

DEF_ARRAY(IRSection)

DEF_ARRAY(IRDataObject)

DEF_ARRAY(IRFunction)

DEF_ATOMIC_ARRAY(IRSection)
DEF_ATOMIC_ARRAY(IRDataObject)
DEF_ATOMIC_ARRAY(IRFunction)

typedef struct IRProgram {
    AtomicArray_IRSection    sections;
    AtomicArray_IRDataObject variables;
    AtomicArray_IRFunction   functions;
} IRProgram;

typedef struct {
    IRProgram* program;
    IRFunction* function;
} IRBuilder;


// This is the interface for building instructions. They are represented differently in binary.
//   For example, add instructions have specific ones for 


void ir_load(IRBuilder* builder, int reg, int reg_mem, int offset, IRType type);
void ir_store(IRBuilder* builder, int reg_mem, int reg, int offset, IRType type);

void ir_add(IRBuilder* builder, int reg_dst, int reg0, int reg1, IRType type);
void ir_sub(IRBuilder* builder, int reg_dst, int reg0, int reg1, IRType type);
void ir_mul(IRBuilder* builder, int reg_dst, int reg0, int reg1, IRType type);
void ir_div(IRBuilder* builder, int reg_dst, int reg0, int reg1, IRType type);
void ir_mod(IRBuilder* builder, int reg_dst, int reg0, int reg1, IRType type);

void ir_imm8(IRBuilder* builder, int reg, i8 imm, IRType type);
void ir_imm16(IRBuilder* builder, int reg, i16 imm, IRType type);
void ir_imm32(IRBuilder* builder, int reg, i32 imm, IRType type);
void ir_imm64(IRBuilder* builder, int reg, i64 imm, IRType type);

void ir_call(IRBuilder* builder, IRFunction_id func_id, u8 arg_count, u8 ret_count, IROperand* args, IROperand* ret_values, IROperand* ret_types);
void ir_ret(IRBuilder* builder, u8 ret_count, IROperand* operands);

void ir_address_of_variable(IRBuilder* builder, int reg, IRSectionID section, int offset);

void print_ir_function(IRProgram* program, IRFunction* function);
