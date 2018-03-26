/* PSX -> 32-bit big-endian PowerPC dynarec.
   Written based on docs for the 750CL, but should conform to the UISA
   750CL: https://fail0verflow.com/media/files/ppc_750cl.pdf
   PEM (referenced by 750CL manual): https://preview.tinyurl.com/ycws6xx9

   Originally by Ash Logan <quarktheawesome@gmail.com>
   For licensing, see the LICENSE or COPYING file included with this repository.
*/

#ifndef __DYNAREC_PPC32_CODEGEN_H__
#define __DYNAREC_PPC32_CODEGEN_H__

#define PPCG_OP(op) (uint32_t) (op << 26)
#define PPCG_REG(reg) (uint8_t) (reg & 0x1F)
#define PPCG_CR(crf) (uint8_t) (crf & 3)
#define PPCG_IMM(imm) (uint16_t) (imm & 0xFFFF)
#define PPCG_BIT(bit) (uint8_t) (bit & 0x1)

#define PPCG_IMM16(op, rD, rA, imm) (uint32_t) \
   (PPCG_OP(op) | (PPCG_REG(rD) << 21) | (PPCG_REG(rA) << 16) | PPCG_IMM(imm))
#define PPCG_ADD(op, rD, rA, rB, oe, op2, rc) (uint32_t) \
   (PPCG_OP(op) | (PPCG_REG(rD) << 21) | (PPCG_REG(rA) << 16) \
   | (PPCG_REG(rB) << 11) | (PPCG_BIT(oe) << 10) | ((op2) << 1) | PPCG_BIT(rc))
#define PPCG_CMP(op, cr, rA, rB, op2) (uint32_t) \
   (PPCG_OP(op) | (PPCG_CR(cr) << 24) | (PPCG_REG(rA) << 16) \
   | (PPCG_REG(rB) << 11) | ((op2) << 1))
#define PPCG_BC(op, bo, bi, bd, aa, lk) (uint32_t) \
   (PPCG_OP(op) | ((bo) << 21) | ((bi) << 16) | ((bd) & ~3) \
   | (PPCG_BIT(aa) << 1) | (PPCG_BIT(lk) << 1))
#define PPCG_ROT(op, rA, rS, sh, mb, me, rc) (uint32_t) \
   (PPCG_OP(op) | (PPCG_REG(rA) << 16) | (PPCG_REG(rS) << 21) | ((sh) << 11) \
   | ((mb) << 6) | ((me) << 1) | PPCG_BIT(rc))

/* lwz rD, imm(rA) */
#define LWZ(rD, imm, rA) (uint32_t) \
   PPCG_IMM16(32, rD, rA, imm)

/* stw rS, imm(rA) */
#define STW(rS, imm, rA) (uint32_t) \
   PPCG_IMM16(36, rS, rA, imm)

/* addi rD, rA, imm */
#define ADDI(rD, rA, imm) (uint32_t) \
   PPCG_IMM16(14, rD, rA, imm)

/* addis rD, rA, imm */
#define ADDIS(rD, rA, imm) (uint32_t) \
   PPCG_IMM16(15, rD, rA, imm)

/* li rD, imm */
#define LI(rD, imm) (uint32_t) \
   ADDI(rD, 0, imm)

/* lis rD, imm */
#define LIS(rD, imm) (uint32_t) \
   ADDIS(rD, 0, imm)

/* base for add, add., addo, addo. */
#define ADDx(rD, rA, rB, oe, rc) (uint32_t) \
   PPCG_ADD(31, rD, rA, rB, oe, 266, rc)

#define ADD(rD, rA, rB) (uint32_t) \
   ADDx(rD, rA, rB, 0, 0)

/* addo. rD, rA, rB */
#define ADDO_(rD, rA, rB) (uint32_t) \
   ADDx(rD, rA, rB, 1, 1)

/* cmpl rA, rB */
#define CMPL(rA, rB) (uint32_t) \
   PPCG_CMP(31, 0, rA, rB, 32)

/* base for bc (+bne,blt,bdnz...), bcl, bca, bcla */
#define BCx(bo, bi, bd, aa, lk) (uint32_t) \
   PPCG_BC(16, bo, bi, bd, aa, lk)

/* bc bo, bi, bd ;see PowerPC manuals */
#define BC(bo, bi, bd) (uint32_t) \
   BCx(bo, bi, bd, 0, 0)

/* blt bd ;always uses cr0 */
#define BLT(bd) (uint32_t) \
   BC(12, 0, bd)

/* ori rA, rS, imm */
#define ORI(rA, rS, imm) (uint32_t) \
   PPCG_IMM16(24, rS, rA, imm) /* yes, really */

/* base for or, or. Can we get away with ADD here?
   It's technically a different format... */
#define ORx(rA, rS, rB, rc) (uint32_t) \
   PPCG_ADD(31, rS, rA, rB, 0, 444, rc)

/* or rA, rS, rB */
#define OR(rA, rS, rB) (uint32_t) \
   ORx(rA, rS, rB, 0)

/* mr rA, rS
   this is the preferred method accoring to the UISA */
#define MR(rA, rS) (uint32_t) \
   OR(rA, rS, rS)

/* Base for rlwinm, rlwinm. */
#define RLWINMx(rA, rS, sh, mb, me, rc) (uint32_t) \
   PPCG_ROT(21, rA, rA, sh, mb, me, rc)

/* rlwinm rA, rS, SH, MB, ME */
#define RLWINM(rA, rS, sh, mb, me) (uint32_t) \
   RLWINMx(rA, rS, sh, mb, me, 0)

/* base for srawi, srawi.
   Same deal with OR, we're using a format we really shouldn't */
#define SRAWIx(rA, rS, SH, rc) (uint32_t) \
   PPCG_ADD(31, rA, rS, SH, 0, 284, rc)

#define SRAWI(rA, rS, SH) (uint32_t) \
   SRAWIx(rA, rS, SH, 0)

#endif