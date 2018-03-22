/* PSX -> 32-bit big-endian PowerPC dynarec.
   Written based on docs for the 750CL, but should conform to the UISA
   750CL: https://fail0verflow.com/media/files/ppc_750cl.pdf
   PEM (referenced by 750CL manual): https://preview.tinyurl.com/ycws6xx9

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
#define PPC_REG_VALID(reg) ((reg != PPC_REG_INVALID)\
                     && (reg >= PPC_REG_FIRST)\
                     && (reg <= PPC_REG_LAST))

/* REG  |volatile?|usage
   r0   |y | dynarec cycle count
   r1   |n | stack pointer
   r2   |y | dynarec_state
   r3   |y | Intermediary/Temp
   r4   |y | Intermediary/Temp
   5-14 |y | PSX regs (dynamically allocated)
   15-31|n | PSX regs (dynamically allocated)
*/
#define PPC_REG_FIRST PPC_REG(5)
#define PPC_REG_LAST  PPC_REG(31)
#define PPC_TMPREG_1 PPC_REG(3)
#define PPC_TMPREG_2 PPC_REG(4)
#define PPC_DYNASTATEREG PPC_REG(2)

/* Mappings of psx regs to ppc regs.
   Use PSX_REG to index. */
ppc_reg_t ppc_reg_map[/*PSX_REG*/32] = { PPC_REG_INVALID };
/* Last PC value where a given PowerPC reg was used.
   Use ppc_reg_t to index.
   TODO: come up with a better optimisation? this could fall over in loops. */
uint32_t ppc_reg_lastuse[/*ppc_reg_t*/32] = { 0 };
#define UPDATE_LAST_USE(compiler, reg) ppc_reg_lastuse[reg] = compiler->pc

static ppc_reg_t get_ppc_reg(enum PSX_REG psxreg) {
   if (psxreg < 32)
      return ppc_reg_map[psxreg];

   return PPC_REG_INVALID;
}

static void prepare_reg(struct dynarec_compiler* compiler,
                        enum PSX_REG psxreg) {
   if (PPC_REG_VALID(ppc_reg_map[psxreg])) return;

/* We need to assign this PSX reg a PowerPC reg!
   Try to find the PowerPC reg that has been left unused for the longest */
   ppc_reg_t best_ppcreg = PPC_REG_FIRST;
   uint32_t best_ppcreg_lastuse = ppc_reg_lastuse[best_ppcreg];

   for (ppc_reg_t ppcreg = PPC_REG_FIRST; ppcreg <= PPC_REG_LAST; ppcreg++) {
      if (ppc_reg_lastuse[ppcreg] < best_ppcreg_lastuse) {
      /* We have a new best reg! */
         best_ppcreg = ppcreg;
         best_ppcreg_lastuse = ppc_reg_lastuse[best_ppcreg];
      }
   }

/* Find out what PSX reg this corresponds to */
   int old_psxreg = -1;
   for (int i = 0; i < 32; i++) {
      if (ppc_reg_map[i] == best_ppcreg) {
         old_psxreg = i;
         break;
      }
   }

/* We've picked a register, woo!
   First, save its old value to dynarec_state. */
   if (old_psxreg >= 0) {
      EMIT(STW(best_ppcreg, \
         DYNAREC_STATE_REG_OFFSET(old_psxreg), PPC_DYNASTATEREG));
   }

/* Now, load in the new psxreg value. */
   EMIT(LWZ(best_ppcreg, \
      DYNAREC_STATE_REG_OFFSET(psxreg), PPC_DYNASTATEREG));

/* Update tracking bits. */
   ppc_reg_map[old_psxreg] = PPC_REG_INVALID;
   ppc_reg_map[psxreg] = best_ppcreg;
   UPDATE_LAST_USE(compiler, best_ppcreg);
   printf("dyna: ppc-%d was psx-%d, is now psx-%d\n", \
      best_ppcreg, old_psxreg, psxreg);
}

/****************** Codegen time! ******************/

#define PPC_OVERFLOW_CHECK() { \
   /*TODO*/ \
}

#define PPC_UNIMPLEMENTED() { \
   printf("dyna: %s not implemented\n", __PRETTY_FUNCTION__); \
}

#define BOILERPLATE_TARGET_SRC \
   prepare_reg(compiler, reg_t); \
   prepare_reg(compiler, reg_s); \
   ppc_reg_t ppc_target = get_ppc_reg(reg_t); \
   ppc_reg_t ppc_source = get_ppc_reg(reg_s); \
   if (ppc_target < 0 || ppc_source < 0) return; \
   UPDATE_LAST_USE(compiler, ppc_target); \
   UPDATE_LAST_USE(compiler, ppc_source);

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
   BOILERPLATE_TARGET_SRC

/* PowerPC doesn't have an immediate add with overflow.

   li tmpReg, val
   addo reg_t, reg_s, tmpReg
   overflow_check */

   EMIT(LI(PPC_TMPREG_1, val));
   EMIT(ADDO_(ppc_target, ppc_source, PPC_TMPREG_1));
   PPC_OVERFLOW_CHECK();
}

void dynasm_emit_addiu(struct dynarec_compiler *compiler,
                       enum PSX_REG reg_t,
                       enum PSX_REG reg_s,
                       uint32_t val) {
#if defined(PPC_DEBUG_INSTR)
   printf("dyna: doing addiu %d, %d, %04X\n", reg_t, reg_s, val);
#endif
   BOILERPLATE_TARGET_SRC

/* MIPS' addiu matches perfectly with PowerPC's addi! Woo! */
   EMIT(ADDI(ppc_target, ppc_source, val));
}

void dynasm_emit_sltiu(struct dynarec_compiler *compiler,
                       enum PSX_REG reg_t,
                       enum PSX_REG reg_s,
                       uint32_t val) {
#if defined(PPC_DEBUG_INSTR)
   printf("dyna: doing sltiu %d, %d, %04X\n", reg_t, reg_s, val);
#endif
   BOILERPLATE_TARGET_SRC

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
}
void dynasm_emit_li(struct dynarec_compiler *compiler,
                    enum PSX_REG reg,
                    uint32_t val) {
   prepare_reg(compiler, reg);
   ppc_reg_t ppc_target = get_ppc_reg(reg);
   if (ppc_target < 0) return;
   UPDATE_LAST_USE(ppc_target);

/* TODO: this looks like a pseudo-instruction.
   Ask if it should be sign-extended. */
   EMIT(LI(ppc_target, val));
}
void dynasm_emit_mov(struct dynarec_compiler *compiler,
                            enum PSX_REG reg_target,
                            enum PSX_REG reg_source) {
   PPC_UNIMPLEMENTED();
}
void dynasm_emit_sll(struct dynarec_compiler *compiler,
                            enum PSX_REG reg_target,
                            enum PSX_REG reg_op,
                            uint8_t shift) {
   PPC_UNIMPLEMENTED();
}
void dynasm_emit_sra(struct dynarec_compiler *compiler,
                            enum PSX_REG reg_target,
                            enum PSX_REG reg_op,
                            uint8_t shift) {
   PPC_UNIMPLEMENTED();
}
void dynasm_emit_addu(struct dynarec_compiler *compiler,
                             enum PSX_REG reg_target,
                             enum PSX_REG reg_op0,
                             enum PSX_REG reg_op1) {
   PPC_UNIMPLEMENTED();
}
void dynasm_emit_or(struct dynarec_compiler *compiler,
                           enum PSX_REG reg_target,
                           enum PSX_REG reg_op0,
                           enum PSX_REG reg_op1) {
   PPC_UNIMPLEMENTED();
}
void dynasm_emit_ori(struct dynarec_compiler *compiler,
                     enum PSX_REG reg_t,
                     enum PSX_REG reg_s,
                     uint32_t val) {
   BOILERPLATE_TARGET_SRC

/* Perfect match! */
   EMIT(ORI(ppc_target, ppc_source, val));
}
void dynasm_emit_andi(struct dynarec_compiler *compiler,
                             enum PSX_REG reg_t,
                             enum PSX_REG reg_s,
                             uint32_t val) {
   PPC_UNIMPLEMENTED();
}
void dynasm_emit_sltu(struct dynarec_compiler *compiler,
                             enum PSX_REG reg_target,
                             enum PSX_REG reg_op0,
                             enum PSX_REG reg_op1) {
   PPC_UNIMPLEMENTED();
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