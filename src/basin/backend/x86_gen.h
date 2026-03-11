#pragma once

#include "basin/backend/codegen.h"

typedef struct X86Builder {

    MachineFunction* function;

    int pointer_size;

} X86Builder;


void x86_emit_push(X86Builder* builder, int reg);
void x86_emit_pop(X86Builder* builder, int reg);

void x86_emit_add(X86Builder* builder, int dst_reg, int input_reg);
void x86_emit_sub(X86Builder* builder, int dst_reg, int input_reg);

void x86_emit_add_imm(X86Builder* builder, int reg, int immediate);
void x86_emit_sub_imm(X86Builder* builder, int reg, int immediate);

void x86_emit_load(X86Builder* builder, int dst_reg, int mem_reg, int displacement);

void x86_emit_store(X86Builder* builder, int src_reg, int mem_reg, int displacement);

void x86_emit_mov(X86Builder* builder, int dst_reg, int src_reg);

void x86_emit_lea(X86Builder* builder, int dst_reg, int mem_reg, int displacement);
void x86_emit_lea_rip(X86Builder* builder, int dst_reg, u32* out_fixup_address);

// The reason we don't use this is because it sets the lower byte of the register
// and doesn't clear the higher bytes. not useful if we want to load a fresh small immediate.
// void x86_emit_imm8(X86Builder* builder, int dst_reg, u8 immediate);
void x86_emit_imm32_signext(X86Builder* builder, int dst_reg, u32 immediate);
void x86_emit_imm32_zeroext(X86Builder* builder, int dst_reg, u32 immediate);
void x86_emit_imm64(X86Builder* builder, int dst_reg, u64 immediate);


void x86_emit_ret(X86Builder* builder);

void x86_emit_call_rel(X86Builder* builder, u32* out_fixup_address);

void x86_emit_call_rip(X86Builder* builder, u32* out_fixup_address);

void x86_emit_call_reg(X86Builder* builder, int reg);


