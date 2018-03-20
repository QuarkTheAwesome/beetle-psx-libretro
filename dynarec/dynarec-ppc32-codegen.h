#ifndef __DYNAREC_PPC32_CODEGEN_H__
#define __DYNAREC_PPC32_CODEGEN_H__

#define PPCG_OP(op) (uint32_t) (op << 26)
#define PPCG_REG(reg) (uint8_t) (reg & 0x1F)
#define PPCG_IMM(imm) (uint16_t) (imm & 0xFFFF)

#define PPCG_IMM16(op, rD, simm, rA) (uint32_t) \
	(PPCG_OP(op) | (PPCG_REG(rD) << 22) | (PPCG_REG(rA) << 16) | PPCG_IMM(simm))

/* lwz rD, simm(rA) */
#define LWZ(rD, simm, rA) (uint32_t) \
	PPCG_IMM16(32, rD, simm, rA)

/* stw rS, simm(rA) */
#define STW(rS, simm, rA) (uint32_t) \
	PPCG_IMM16(36, rS, simm, rA)

#endif