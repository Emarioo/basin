/*
    Intermediate Representation (IR)

    A compact representation of a program which is understood by the backend.
*/

#pragma once

#include "platform/array.h"
#include "platform/atomic_array.h"
#include "platform/string.h"
#include "platform/thread.h"

typedef enum {
    IR_ADD,
    IR_SUB,
    IR_MUL,
    IR_DIV,
    IR_MOD,
    
    IR_UADD,
    IR_USUB,
    IR_UMUL,
    IR_UDIV,
    IR_UMOD,

    IR_FADD,
    IR_FSUB,
    IR_FMUL,
    IR_FDIV,
    IR_FMOD,

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

    IR_EXTEND1 = 253, // extended opcode
    IR_EXTEND2 = 254,
    IR_RESERVED_255 = 255,
} _IROpcode;

typedef enum {
    IR_SECTION_LOCAL,
    IR_SECTION_PARAMETER,
    IR_SECTION_DATA,
} _IRSectionID;

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
    int offset;
} IRInstruction_address_of_variable;

typedef struct {
    IROpcode opcode;
    IROperand output;
    IRFunction_id function_id;
} IRInstruction_address_of_function;

typedef struct {
    IROpcode opcode;
    IROperand output;
    i64 immediate;
} IRInstruction_imm; // TODO: 8,16,32 versions

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
    IROperand operands[/* arg_count + ret_count */];
} IRInstruction_call;

typedef struct {
    IROpcode opcode;
    IROperand input_ptr;
    u8 arg_count;
    u8 ret_count;
    IROperand operands[/* arg_count + ret_count */];
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

    u8* data;
    int len;
    int max;
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
    Memory data;
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
typedef struct IRCollection {
    AtomicArray_IRSection    sections;
    AtomicArray_IRDataObject variables;
    AtomicArray_IRFunction   functions;
} IRCollection;

typedef struct {
    // IRProgram* collection;
    IRCollection* collection;
} IRBuilder;

// bool, char, pointers are integers
typedef enum {
    IR_TYPE_INVALID = 0,
    IR_TYPE_SINT8,  
    IR_TYPE_SINT16,
    IR_TYPE_SINT32,
    IR_TYPE_SINT64,
    
    IR_TYPE_UINT8,
    IR_TYPE_UINT16,
    IR_TYPE_UINT32,
    IR_TYPE_UINT64,
    
    IR_TYPE_FLOAT32,
    IR_TYPE_FLOAT64,

    // Here for calling conventions (some put 16-byte struct in two registers)
    IR_TYPE_STRUCT8,
    IR_TYPE_STRUCT16,
    IR_TYPE_STRUCT32,
    IR_TYPE_STRUCT64,
    IR_TYPE_STRUCT128,
    IR_TYPE_STRUCT_LARGE,
} IRType;

// This is the interface for building instructions. They are represented differently in binary.
//   For example, add instructions have specific ones for 


void ir_load(IRBuilder* builder, int reg, int reg_mem, int offset, IRType type);
void ir_store(IRBuilder* builder, int reg, int reg_mem, int offset, IRType type);

void ir_add(IRBuilder* builder, int reg_dst, int reg0, int reg1, IRType type);
void ir_sub(IRBuilder* builder, int reg_dst, int reg0, int reg1, IRType type);
void ir_mul(IRBuilder* builder, int reg_dst, int reg0, int reg1, IRType type);
void ir_div(IRBuilder* builder, int reg_dst, int reg0, int reg1, IRType type);
void ir_mod(IRBuilder* builder, int reg_dst, int reg0, int reg1, IRType type);

void ir_imm8(IRBuilder* builder, int reg, i8 imm, IRType type);
void ir_imm16(IRBuilder* builder, int reg, i16 imm, IRType type);
void ir_imm32(IRBuilder* builder, int reg, i32 imm, IRType type);
void ir_imm64(IRBuilder* builder, int reg, i64 imm, IRType type);

void ir_call(IRBuilder* builder, IRFunction_id func_id, u8 arg_count, u8 ret_count, IROperand* operands);
