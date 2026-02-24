#include "basin/backend/x86_gen.h"

#include "basin/backend/x86_defs.h"

static void reserve_bytes(X86Builder* builder) {
    if (builder->function->code_len + 256 >= builder->function->code_max) {
        int new_max = builder->function->code_max * 2 + 256;
        builder->function->code = mem__allocate(new_max, builder->function->code);
        builder->function->code_max = new_max;
    }
}

#define EMIT_PRELUDE() reserve_bytes(builder);

#define IS_REG_EXTENDED(REG) (REG >= 8)

#define CLAMP_EXT_REG(REG) ((REG) & 7)

#define REXW_IF_64_BIT_ARCH (builder->pointer_size==8 ? PREFIX_REXW : 0)

static void emit1(X86Builder* builder, u8 value) {
    builder->function->code[builder->function->code_len++] = value;
}
static void emit2(X86Builder* builder, u16 value) {
    *(u16*)&builder->function->code[builder->function->code_len] = value;
    builder->function->code_len += 2;
}
static void emit4(X86Builder* builder, u32 value) {
    *(u32*)&builder->function->code[builder->function->code_len] = value;
    builder->function->code_len += 4;
}
static void emit8(X86Builder* builder, u64 value) {
    *(u64*)&builder->function->code[builder->function->code_len] = value;
    builder->function->code_len += 8;
}

static void maybe_emit_prefix(X86Builder* builder, u8 inherit_prefix, int reg, int rm) {
    u8 prefix = inherit_prefix;
    if (IS_REG_EXTENDED(reg)) {
        prefix |= PREFIX_REXR;
    }
    if (IS_REG_EXTENDED(rm)) {
        prefix |= PREFIX_REXB;
    }
    if (prefix != 0) {
        emit1(builder, prefix);
    }
}

void emit_modrm_sib(X86Builder* builder, u8 mod, int reg, u8 scale, u8 index, int base_reg) {
    // u8 base = _base - 1;
    // u8 reg = _reg - 1;
    //  register to register (mod = 0b11) doesn't work with SIB byte
    ASSERT(("Use addModRM instead", mod != 0b11));

    ASSERT(("Ignored meaning in SIB byte. Look at intel x64 manual and fix it.",
                    base_reg != 0b101));

    u8 rm = 0b100;
    ASSERT((mod & ~3) == 0 && (reg & ~7) == 0 && (rm & ~7) == 0);
    ASSERT((scale & ~3) == 0 && (index & ~7) == 0 && (base_reg & ~7) == 0);

    emit1(builder, (u8)(rm | (reg << (u8)3) | (mod << (u8)6)));
    emit1(builder, (u8)(base_reg | (index << (u8)3) | (scale << (u8)6)));
}

static void emit_modrm(X86Builder* builder, u8 mod, int _reg, int _rm) {
    u8 reg = _reg;
    u8 rm = _rm;
    if (_rm == X64_REG_SP && mod != MODE_REG) {
        // SP register is not allowed with standard modrm byte, we must use a SIB
        emit_modrm_sib(builder, mod, _reg, SIB_SCALE_1, SIB_INDEX_NONE, _rm);
        return;
    }
    if (_rm == X64_REG_BP && mod == MODE_DEREF) {
        // BP register is not allowed with without a displacement.
        // But we can use a displacement of zero. HOWEVER!
        // bp with zero displacement points to the previous frame's base pointer
        // which we shouldn't overwrite so we SHOULD assert.
        // if (!disable_modrm_asserts) {
        ASSERT(("mov reg, [bp] is a bug", false));
        // }
        mod = MODE_DEREF_DISP8;
        emit1(builder, (u8)(rm | (reg << (u8)3) | (mod << (u8)6)));
        emit1(builder, (u8)0);
        return;
    }
    ASSERT((mod & ~3) == 0 && (reg & ~7) == 0 && (rm & ~7) == 0);
    // You probably made a mistake and used REG_BP thinking it works with just
    // ModRM byte.
    ASSERT(("Use addModRM_SIB instead", !(mod != 0b11 && rm == 0b100)));
    // X64_REG_BP isn't available with mod=0b00, You must use a displacement. see
    // intel x64 manual about 32 bit addressing for more details
    ASSERT(("Use addModRM_disp32 instead", !(mod == 0b00 && rm == 0b101)));
    // ASSERT(("Use addModRM_disp32 instead",(mod!=0b10)));
    emit1(builder, (u8)(rm | (reg << (u8)3) | (mod << (u8)6)));
}
static void emit_modrm_rip32(X86Builder* builder, int _reg, u32 disp32) {
    u8 reg = _reg;
    u8 mod = 0b00;
    u8 rm = 0b101;
    ASSERT((mod & ~3) == 0 && (reg & ~7) == 0 && (rm & ~7) == 0);
    emit1(builder, (u8)(rm | (reg << (u8)3) | (mod << (u8)6)));
    emit4(builder, disp32);
}

void x86_emit_load(X86Builder* builder, int dst_reg, int mem_reg, int displacement) {
    EMIT_PRELUDE()

    maybe_emit_prefix(builder, 0, dst_reg, mem_reg);

    emit1(builder, OPCODE_MOV_REG_RM);
    
    u8 mode = MODE_DEREF_DISP32;
    if (displacement == 0) {
        mode = MODE_DEREF;
    } else if (displacement >= -0x80 && displacement < 0x7F) {
        mode = MODE_DEREF_DISP8;
    }
    emit_modrm(builder, mode, CLAMP_EXT_REG(dst_reg), CLAMP_EXT_REG(mem_reg));
    if (mode == MODE_DEREF) {

    } else if (mode == MODE_DEREF_DISP8)
        emit1(builder, (u8)(i8)displacement);
    else
        emit4(builder, (u32)(i32)displacement);
}

void x86_emit_store(X86Builder* builder, int src_reg, int mem_reg, int displacement) {
    EMIT_PRELUDE()

    maybe_emit_prefix(builder, 0, src_reg, mem_reg);

    emit1(builder, OPCODE_MOV_RM_REG);
    
    u8 mode = MODE_DEREF_DISP32;
    if (displacement == 0) {
        mode = MODE_DEREF;
    } else if (displacement >= -0x80 && displacement < 0x7F) {
        mode = MODE_DEREF_DISP8;
    }
    emit_modrm(builder, mode, CLAMP_EXT_REG(src_reg), CLAMP_EXT_REG(mem_reg));
    if (mode == MODE_DEREF) {

    } else if (mode == MODE_DEREF_DISP8)
        emit1(builder, (u8)(i8)displacement);
    else
        emit4(builder, (u32)(i32)displacement);
}

void x86_emit_mov(X86Builder* builder, int dst_reg, int src_reg) {
    EMIT_PRELUDE()

    maybe_emit_prefix(builder, 0, dst_reg, src_reg);

    emit1(builder, OPCODE_MOV_REG_RM);
    emit_modrm(builder, MODE_REG, CLAMP_EXT_REG(dst_reg), CLAMP_EXT_REG(src_reg));
}

void x86_emit_lea(X86Builder* builder, int dst_reg, int mem_reg, int displacement) {
    EMIT_PRELUDE()
    
    maybe_emit_prefix(builder, REXW_IF_64_BIT_ARCH, dst_reg, mem_reg);
    emit1(builder, OPCODE_LEA_REG_M);

    u8 mode = MODE_DEREF_DISP32;
    // MODE_DEREF and BP is not allowed, we must have some displacement.
    if (displacement == 0 && mem_reg != X64_REG_BP) {
        mode = MODE_DEREF;
    } else if (displacement >= -0x80 && displacement < 0x7F) {
        mode = MODE_DEREF_DISP8;
    }
    emit_modrm(builder, mode, CLAMP_EXT_REG(dst_reg), CLAMP_EXT_REG(mem_reg));
    if (mode == MODE_DEREF) {

    } else if (mode == MODE_DEREF_DISP8)
        emit1(builder, (u8)(i8)displacement);
    else
        emit4(builder, (u32)(i32)displacement);
}

void x86_emit_lea_rip(X86Builder* builder, int dst_reg, u32* out_fixup_address) {
    EMIT_PRELUDE()
    
    maybe_emit_prefix(builder, REXW_IF_64_BIT_ARCH, dst_reg, 0);
    emit1(builder, OPCODE_LEA_REG_M);
    emit_modrm_rip32(builder, CLAMP_EXT_REG(dst_reg), 0);
    *out_fixup_address = builder->function->code_len - 4;
}

void x86_emit_imm4(X86Builder* builder, int dst_reg, u32 immediate) {
    EMIT_PRELUDE()

    maybe_emit_prefix(builder, 0, 0, dst_reg);
    emit1(builder, (u8)(OPCODE_MOV_REG_IMM_RD | CLAMP_EXT_REG(dst_reg)));
    emit4(builder, (u32)immediate);
}

