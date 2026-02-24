#pragma once

#include "basin/backend/codegen.h"

typedef struct X86Builder {

    CodegenFunction* function;

    int pointer_size;

} X86Builder;


void x86_emit_load(X86Builder* builder, int dst_reg, int mem_reg, int displacement);

void x86_emit_store(X86Builder* builder, int src_reg, int mem_reg, int displacement);

void x86_emit_mov(X86Builder* builder, int dst_reg, int src_reg);

void x86_emit_lea(X86Builder* builder, int dst_reg, int mem_reg, int displacement);
void x86_emit_lea_rip(X86Builder* builder, int dst_reg, u32* out_fixup_address);

void x86_emit_imm4(X86Builder* builder, int dst_reg, u32 immediate);

