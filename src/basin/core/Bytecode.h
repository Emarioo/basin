
#include "platform/array.h"


typedef enum {
    I_NONE = 0,
    I_LOAD,
    I_STORE,
    I_ADD,
    I_SUB,
    I_MUL,
    I_DIV,
    I_MOD,

    I_IMM,


} InstructionKind;

typedef struct {
    u8* data;
    int len;
    int max;
} ByteFunction;

DEF_ARRAY(ByteFunction)

typedef struct {
    Array_ByteFunction functions;
} ByteProgram;

typedef struct {

} BCBuilder;

// bool, char, pointers are integers
typedef enum {
    BCT_INVALID = 0,
    BCT_SINT8,  
    BCT_SINT16,
    BCT_SINT32,
    BCT_SINT64,
    
    BCT_UINT8,
    BCT_UINT16,
    BCT_UINT32,
    BCT_UINT64,
    
    BCT_FLOAT32,
    BCT_FLOAT64,
} ByteType;

// This is the interface for building instructions. They are represented differently in binary.
//   For example, add instructions have specific ones for 


void bc_load(BCBuilder* builder, int reg, int reg_mem, int offset, ByteType type);
void bc_store(BCBuilder* builder, int reg, int reg_mem, int offset, ByteType type);

void bc_add(BCBuilder* builder, int reg_dst, int reg0, int reg1, ByteType type);
void bc_sub(BCBuilder* builder, int reg_dst, int reg0, int reg1, ByteType type);
void bc_mul(BCBuilder* builder, int reg_dst, int reg0, int reg1, ByteType type);
void bc_div(BCBuilder* builder, int reg_dst, int reg0, int reg1, ByteType type);
void bc_mod(BCBuilder* builder, int reg_dst, int reg0, int reg1, ByteType type);

void bc_imm8(BCBuilder* builder, int reg, i8 imm, ByteType type);
void bc_imm16(BCBuilder* builder, int reg, i16 imm, ByteType type);
void bc_imm32(BCBuilder* builder, int reg, i32 imm, ByteType type);
void bc_imm64(BCBuilder* builder, int reg, i64 imm, ByteType type);