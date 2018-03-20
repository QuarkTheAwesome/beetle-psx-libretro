#ifndef __DYNAREC_PPC32_CODEGEN_H__
#define __DYNAREC_PPC32_CODEGEN_H__

#define PPCG_OP(op) (uint32_t) (op << 26)
#define PPCG_REG(reg) (uint8_t) (reg & 0x1F)
#define PPCG_IMM(imm) (uint16_t) (imm & 0xFFFF)
#define PPCG_BIT(bit) (uint8_t) (bit & 0x1)

#define PPCG_IMM16(op, rD, rA, imm) (uint32_t) \
	(PPCG_OP(op) | (PPCG_REG(rD) << 21) | (PPCG_REG(rA) << 16) | PPCG_IMM(imm))
#define PPCG_ADD(op, rD, rA, rB, oe, op2, rc) (uint32_t) \
	(PPCG_OP(op) | (PPCG_REG(rD) << 21) | (PPCG_REG(rA) << 16) \
	| (PPCG_REG(rB) << 11) | (PPCG_BIT(oe) << 10) | (op2 << 1) | PPCG_BIT(rc))

/* lwz rD, imm(rA) */
#define LWZ(rD, imm, rA) (uint32_t) \
	PPCG_IMM16(32, rD, rA, imm)

/* stw rS, imm(rA) */
#define STW(rS, imm, rA) (uint32_t) \
	PPCG_IMM16(36, rS, rA, imm)

/* addi rD, rA, imm */
#define ADDI(rD, rA, imm) (uint32_t) \
	PPCG_IMM16(14, rD, rA, imm)

/* li rD, imm */
#define LI(rD, imm) (uint32_t) \
	ADDI(rD, 0, imm)

/* base for add, add., addo, addo. */
#define ADDx(rD, rA, rB, oe, rc) (uint32_t) \
	PPCG_ADD(31, rD, rA, rB, oe, 266, rc)

#define ADDO_(rD, rA, rB) (uint32_t) \
	ADDx(rD, rA, rB, 1, 1)

#endif