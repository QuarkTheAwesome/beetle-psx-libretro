/* PSX -> 32-bit big-endian PowerPC dynarec.
   Written based on docs for the 750CL, but should conform to the UISA
   750CL: https://fail0verflow.com/media/files/ppc_750cl.pdf
   PEM (referenced by 750CL manual): https://preview.tinyurl.com/ycws6xx9

   If you modify any instruction sequences, make sure to check dynarec-ppc32.h
   and update the maximum length if needed.

   Originally by Ash Logan <quarktheawesome@gmail.com>
   For licensing, see the LICENSE or COPYING file included with this repository.
*/

#include <stddef.h>
#include <stdio.h>
#include <assert.h>

#include "dynarec-compiler.h"
#include "dynarec-ppc32-codegen.h"

#define PPC_DEBUG_INSTR 1

#ifdef MSB_FIRST
#define EMIT(instr) { \
   uint32_t* map = (uint32_t*)(compiler->map); \
   *map++ = instr; \
   compiler->map = (uint8_t*)map; \
}
#else //!MSB_FIRST
#define EMIT(instr) { \
   uint32_t* map = (uint32_t*)(compiler->map); \
   *map++ = __builtin_bswap32(instr); \
   compiler->map = (uint8_t*)map; \
}
#endif

typedef int8_t ppc_reg_t;
#define PPC_REG(reg) (ppc_reg_t)reg
#define PPC_REG_INVALID PPC_REG(-1)
#define PPC_REG_VALID(reg) ((reg != PPC_REG_INVALID) \
                           && (reg < 32))

/* REG  |volatile?|usage
   r0   |y | dynarec cycle count
   r1   |n | stack pointer
   r2   |y | dynarec_state
   r3   |y | Intermediary/Temp
   r4   |y | Intermediary/Temp
   5-7  |y | PSX regs (dynamically allocated)
   8-14 |y | PSX regs (statically allocated)
   15-31|n | PSX regs (statically allocated)
*/
#define PPC_DYN_REG_FIRST PPC_REG(5)
#define PPC_DYN_REG_LAST  PPC_REG(7)
#define PPC_TMPREG_1 PPC_REG(3)
#define PPC_TMPREG_2 PPC_REG(4)
#define PPC_DYNASTATEREG PPC_REG(2)

static ppc_reg_t reg_map[33] = {
   /* PSX_REG_R0 */ PPC_REG(8),
   /* PSX_REG_AT */ PPC_REG(9),
   /* PSX_REG_V0 */ PPC_REG(10),
   /* PSX_REG_V1 */ PPC_REG(11),
   /* PSX_REG_A0 */ PPC_REG(12),
   /* PSX_REG_A1 */ PPC_REG(13),
   /* PSX_REG_A2 */ PPC_REG(14),
   /* PSX_REG_A3 */ PPC_REG(15),
   /* PSX_REG_T0 */ PPC_REG(16),
   /* PSX_REG_T1 */ PPC_REG(17),
   /* PSX_REG_T2 */ PPC_REG(18),
   /* PSX_REG_T3 */ PPC_REG(19),
   /* PSX_REG_T4 */ PPC_REG(20),
   /* PSX_REG_T5 */ PPC_REG(21),
   /* PSX_REG_T6 */ PPC_REG(22),
   /* PSX_REG_T7 */ PPC_REG(23),
   /* PSX_REG_S0 */ PPC_REG(30),
   /* PSX_REG_S1 */ PPC_REG(31),
   /* PSX_REG_S2 */ PPC_REG_INVALID,
   /* PSX_REG_S3 */ PPC_REG_INVALID,
   /* PSX_REG_S4 */ PPC_REG_INVALID,
   /* PSX_REG_S5 */ PPC_REG_INVALID,
   /* PSX_REG_S6 */ PPC_REG_INVALID,
   /* PSX_REG_S7 */ PPC_REG_INVALID,
   /* PSX_REG_T8 */ PPC_REG(24),
   /* PSX_REG_T9 */ PPC_REG(25),
   /* PSX_REG_K0 */ PPC_REG_INVALID,
   /* PSX_REG_K1 */ PPC_REG_INVALID,
   /* PSX_REG_GP */ PPC_REG_INVALID,
   /* PSX_REG_SP */ PPC_REG(26),
   /* PSX_REG_FP */ PPC_REG(27),
   /* PSX_REG_RA */ PPC_REG(28),
   /* PSX_REG_DT */ PPC_REG(29),
};
static bool dyn_reg_free[PPC_DYN_REG_LAST - PPC_DYN_REG_FIRST + 1] = { true };

static ppc_reg_t load_psx_reg(struct dynarec_compiler* compiler,
                              enum PSX_REG psx_reg) {
   if (psx_reg > 32) return PPC_REG_INVALID;
   ppc_reg_t ppc_reg = reg_map[psx_reg];

   if (PPC_REG_VALID(ppc_reg)) return ppc_reg;

   for (ppc_reg = PPC_DYN_REG_FIRST; ppc_reg <= PPC_DYN_REG_LAST; ppc_reg++) {
      if (dyn_reg_free[ppc_reg - PPC_DYN_REG_FIRST]) {
         printf("dyna: assigned psx%d to ppc%d\n", psx_reg, ppc_reg);
         dyn_reg_free[ppc_reg - PPC_DYN_REG_FIRST] = false;
         EMIT(LWZ(ppc_reg, \
            DYNAREC_STATE_REG_OFFSET(psx_reg), PPC_DYNASTATEREG));
         return ppc_reg;
      }
   }

   return PPC_REG_INVALID;
}

static void save_psx_reg(struct dynarec_compiler* compiler,
                         enum PSX_REG psx_reg,
                         ppc_reg_t ppc_reg) {
   if (psx_reg > 32) return;
   if (!PPC_REG_VALID(ppc_reg) || ppc_reg < PPC_DYN_REG_FIRST \
      || ppc_reg > PPC_DYN_REG_LAST) return;

   printf("dyna: saving psx%d from ppc%d\n", psx_reg, ppc_reg);
   dyn_reg_free[ppc_reg - PPC_DYN_REG_FIRST] = true;
   EMIT(STW(ppc_reg, \
      DYNAREC_STATE_REG_OFFSET(psx_reg), PPC_DYNASTATEREG));
}

/****************** Codegen time! ******************/

#define PPC_OVERFLOW_CHECK() { \
   /*TODO*/ \
}

#define PPC_UNIMPLEMENTED() { \
   printf("dyna: %s not implemented\n", __PRETTY_FUNCTION__); \
}

#define GET_REG(psx_reg, ppc_reg) \
   ppc_reg_t ppc_reg = load_psx_reg(compiler, psx_reg); \
   if (ppc_reg < 0) return

#define SAVE_REG(psx_reg, ppc_reg) \
   save_psx_reg(compiler, psx_reg, ppc_reg)

/*  TODO: ask what this is supposed to do */
void dynasm_counter_maintenance(struct dynarec_compiler *compiler,
                                unsigned cycles) {
   PPC_UNIMPLEMENTED();
}

int32_t dynasm_execute(struct dynarec_state *state,
                       dynarec_fn_t target,
                       int32_t counter) {
   PPC_UNIMPLEMENTED();
   return 0;
}

void dynasm_emit_exception(struct dynarec_compiler *compiler,
                           enum PSX_CPU_EXCEPTION exception) {
   PPC_UNIMPLEMENTED();
}

void dynasm_emit_page_local_jump(struct dynarec_compiler *compiler,
                                 int32_t offset,
                                 bool placeholder) {
   PPC_UNIMPLEMENTED();
}

void dynasm_emit_addi(struct dynarec_compiler *compiler,
                      enum PSX_REG reg_t,
                      enum PSX_REG reg_s,
                      uint32_t val) {
#if defined(PPC_DEBUG_INSTR)
   printf("dyna: doing addi %d, %d, %04X\n", reg_t, reg_s, val);
#endif
   GET_REG(reg_t, ppc_target);
   GET_REG(reg_s, ppc_source);

/* PowerPC doesn't have an immediate add with overflow.

   li tmpReg, val
   addo reg_t, reg_s, tmpReg
   overflow_check */

   EMIT(LI(PPC_TMPREG_1, val));
   EMIT(ADDO_(ppc_target, ppc_source, PPC_TMPREG_1));
   PPC_OVERFLOW_CHECK();

   SAVE_REG(reg_t, ppc_target);
   SAVE_REG(reg_s, ppc_source);
}

void dynasm_emit_addiu(struct dynarec_compiler *compiler,
                       enum PSX_REG reg_t,
                       enum PSX_REG reg_s,
                       uint32_t val) {
#if defined(PPC_DEBUG_INSTR)
   printf("dyna: doing addiu %d, %d, %04X\n", reg_t, reg_s, val);
#endif
   GET_REG(reg_t, ppc_target);
   GET_REG(reg_s, ppc_source);

/* MIPS' addiu matches perfectly with PowerPC's addi! Woo! */
   EMIT(ADDI(ppc_target, ppc_source, val));

   SAVE_REG(reg_t, ppc_target);
   SAVE_REG(reg_s, ppc_source);
}

void dynasm_emit_sltiu(struct dynarec_compiler *compiler,
                       enum PSX_REG reg_t,
                       enum PSX_REG reg_s,
                       uint32_t val) {
#if defined(PPC_DEBUG_INSTR)
   printf("dyna: doing sltiu %d, %d, %04X\n", reg_t, reg_s, val);
#endif
   GET_REG(reg_t, ppc_target);
   GET_REG(reg_s, ppc_source);

/* This one is annoying because of its boolean result...

   li reg_t, 1 ;set target to true
   li tmpReg, val ;sign-extend val, cmpli doesn't sign-extend
   cmpl reg_s, tmpReg
   blt 8 ;if less than, skip next instruction
   li reg_t, 0 ;it's not less than, set target to false */

   EMIT(LI(ppc_target, 1));
   EMIT(LI(PPC_TMPREG_1, val));
   EMIT(CMPL(ppc_source, PPC_TMPREG_1));
   EMIT(BLT(8));
   EMIT(LI(ppc_target, 0));

   SAVE_REG(reg_t, ppc_target);
   SAVE_REG(reg_s, ppc_source);
}
void dynasm_emit_li(struct dynarec_compiler *compiler,
                    enum PSX_REG reg,
                    uint32_t val) {
#if defined(PPC_DEBUG_INSTR)
   printf("dyna: doing li %d, %04X\n", reg, val);
#endif
   GET_REG(reg, ppc_target);

/* TODO: this looks like a pseudo-instruction.
   Ask if it should be sign-extended.

   Conditionally emit lis/addi, li or lis as needed */
   if (val & 0xFFFF) {
      if (val & 0xFFFF0000) {
         EMIT(LIS(ppc_target, val >> 16));
         EMIT(ADDI(ppc_target, ppc_target, val));
      } else { //val & 0xFFFF
         EMIT(LI(ppc_target, val));
      }
   } else { //val & 0xFFFF == 0
      EMIT(LIS(ppc_target, val >> 16));
   }

   SAVE_REG(reg, ppc_target);
}
void dynasm_emit_mov(struct dynarec_compiler *compiler,
                     enum PSX_REG reg_t,
                     enum PSX_REG reg_s) {
#if defined(PPC_DEBUG_INSTR)
   printf("dyna: doing mov %d, %d\n", reg_t, reg_s);
#endif
   GET_REG(reg_t, ppc_target);
   GET_REG(reg_s, ppc_source);

   EMIT(MR(ppc_target, ppc_source));

   SAVE_REG(reg_t, ppc_target);
   SAVE_REG(reg_s, ppc_source);
}
void dynasm_emit_sll(struct dynarec_compiler *compiler,
                     enum PSX_REG reg_t,
                     enum PSX_REG reg_s,
                     uint8_t shift) {
#if defined(PPC_DEBUG_INSTR)
   printf("dyna: doing sll r%d, r%d, %d\n", reg_t, reg_s, shift);
#endif
   GET_REG(reg_t, ppc_target);
   GET_REG(reg_s, ppc_source);

   EMIT(RLWINM(ppc_target, ppc_source, shift, 0, 31 - shift));

   SAVE_REG(reg_t, ppc_target);
   SAVE_REG(reg_s, ppc_source);
}
void dynasm_emit_sra(struct dynarec_compiler *compiler,
                     enum PSX_REG reg_t,
                     enum PSX_REG reg_s,
                     uint8_t shift) {
#if defined(PPC_DEBUG_INSTR)
   printf("dyna: doing sra r%d, r%d, %d\n", reg_t, reg_s, shift);
#endif
   GET_REG(reg_t, ppc_target);
   GET_REG(reg_s, ppc_source);

   EMIT(SRAWI(ppc_target, ppc_source, shift));

   SAVE_REG(reg_t, ppc_target);
   SAVE_REG(reg_s, ppc_source);
}
void dynasm_emit_addu(struct dynarec_compiler *compiler,
                      enum PSX_REG reg_target,
                      enum PSX_REG reg_op0,
                      enum PSX_REG reg_op1) {
#if defined(PPC_DEBUG_INSTR)
   printf("dyna: doing addu %d, %d, %d\n", reg_target, reg_op0, reg_op1);
#endif
   GET_REG(reg_target, ppc_target);
   GET_REG(reg_op0, ppc_op0);
   GET_REG(reg_op1, ppc_op1);

   EMIT(ADD(ppc_target, ppc_op0, ppc_op1));

   SAVE_REG(reg_target, ppc_target);
   SAVE_REG(reg_op0, ppc_op0);
   SAVE_REG(reg_op1, ppc_op1);
}
void dynasm_emit_or(struct dynarec_compiler *compiler,
                    enum PSX_REG reg_target,
                    enum PSX_REG reg_op0,
                    enum PSX_REG reg_op1) {
#if defined(PPC_DEBUG_INSTR)
   printf("dyna: doing or %d, %d, %d\n", reg_target, reg_op0, reg_op1);
#endif
   GET_REG(reg_target, ppc_target);
   GET_REG(reg_op0, ppc_op0);
   GET_REG(reg_op1, ppc_op1);

   EMIT(OR(ppc_target, ppc_op0, ppc_op1));

   SAVE_REG(reg_target, ppc_target);
   SAVE_REG(reg_op0, ppc_op0);
   SAVE_REG(reg_op1, ppc_op1);
}
void dynasm_emit_ori(struct dynarec_compiler *compiler,
                     enum PSX_REG reg_t,
                     enum PSX_REG reg_s,
                     uint32_t val) {
#if defined(PPC_DEBUG_INSTR)
   printf("dyna: doing ori %d, %d, %04X\n", reg_t, reg_s, val);
#endif
   GET_REG(reg_t, ppc_target);
   GET_REG(reg_s, ppc_source);

/* Perfect match! */
   EMIT(ORI(ppc_target, ppc_source, val));

   SAVE_REG(reg_t, ppc_target);
   SAVE_REG(reg_s, ppc_source);
}
void dynasm_emit_andi(struct dynarec_compiler *compiler,
                      enum PSX_REG reg_t,
                      enum PSX_REG reg_s,
                      uint32_t val) {
#if defined(PPC_DEBUG_INSTR)
   printf("dyna: doing andi %d, %d, %04X\n", reg_t, reg_s, val);
#endif
   GET_REG(reg_t, ppc_target);
   GET_REG(reg_s, ppc_source);

   EMIT(ANDI_(ppc_target, ppc_source, val));

   SAVE_REG(reg_t, ppc_target);
   SAVE_REG(reg_s, ppc_source);
}
void dynasm_emit_sltu(struct dynarec_compiler *compiler,
                      enum PSX_REG reg_target,
                      enum PSX_REG reg_op0,
                      enum PSX_REG reg_op1) {
#if defined(PPC_DEBUG_INSTR)
   printf("dyna: doing sltu %d, %d, %d\n", reg_target, reg_op0, reg_op1);
#endif
   GET_REG(reg_target, ppc_target);
   GET_REG(reg_op0, ppc_op0);
   GET_REG(reg_op1, ppc_op1);

/* Here we go again...

   see sltiu for an explanation. Only difference is the missing
   sign-extension.
*/
   EMIT(LI(ppc_target, 1));
   EMIT(CMPL(ppc_op0, ppc_op1));
   EMIT(BLT(8));
   EMIT(LI(ppc_target, 0));

   SAVE_REG(reg_target, ppc_target);
   SAVE_REG(reg_op0, ppc_op0);
   SAVE_REG(reg_op1, ppc_op1);
}
void dynasm_emit_sw(struct dynarec_compiler *compiler,
                           enum PSX_REG reg_addr,
                           int16_t offset,
                           enum PSX_REG reg_val) {
   PPC_UNIMPLEMENTED();
}
void dynasm_emit_sh(struct dynarec_compiler *compiler,
                           enum PSX_REG reg_addr,
                           int16_t offset,
                           enum PSX_REG reg_val) {
   PPC_UNIMPLEMENTED();
}
void dynasm_emit_lw(struct dynarec_compiler *compiler,
                           enum PSX_REG reg_target,
                           int16_t offset,
                           enum PSX_REG reg_addr) {
   PPC_UNIMPLEMENTED();
}

void dynasm_emit_mfhi(struct dynarec_compiler *compiler,
                             enum PSX_REG reg_target) {
   PPC_UNIMPLEMENTED();
}
void dynasm_emit_mtlo(struct dynarec_compiler *compiler,
                             enum PSX_REG ret_source) {
   PPC_UNIMPLEMENTED();
}

void dynasm_emit_mtc0(struct dynarec_compiler *compiler,
                             enum PSX_REG reg_source,
                             enum PSX_COP0_REG reg_cop0) {
   PPC_UNIMPLEMENTED();
}