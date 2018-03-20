#include <stddef.h>
#include <stdio.h>
#include <assert.h>

#include "dynarec-compiler.h"

typedef int8_t ppc_reg_t;
#define PPC_REG(reg) (ppc_reg_t)reg
#define PPC_REG_INVALID PPC_REG(-1)
#define PPC_REG_VALID(reg) ((reg != PPC_REG_INVALID)\
                            && (reg >= PPC_REG_FIRST)\
                            && (reg <= PPC_REG_LAST))

/*  REG  |V?|
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

/*  Mappings of psx regs to ppc regs.
    Use PSX_REG to index. */
ppc_reg_t ppc_reg_map[/*PSX_REG*/32] = { PPC_REG_INVALID };
/*  Last PC value where a given PowerPC reg was used.
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

/*  We need to assign this PSX reg a PowerPC reg!
    Try to find the PowerPC reg that has been left unused for the longest */
    ppc_reg_t best_ppcreg = PPC_REG_FIRST;
    uint32_t best_ppcreg_lastuse = ppc_reg_lastuse[best_ppcreg];

    for (ppc_reg_t ppcreg = PPC_REG_FIRST; ppcreg <= PPC_REG_LAST; ppcreg++) {
        if (ppc_reg_lastuse[ppcreg] < best_ppcreg_lastuse) {
        /*  We have a new best reg! */
            best_ppcreg = ppcreg;
            best_ppcreg_lastuse = ppc_reg_lastuse[best_ppcreg];
        }
    }

/*  Find out what PSX reg this corresponds to */
    int old_psxreg = -1;
    for (int i = 0; i < 32; i++) {
        if (ppc_reg_map[i] == best_ppcreg) {
            old_psxreg = i;
            break;
        }
    }

/*  We've picked a register, woo! 
    First, save its old value to dynarec_state. */
    if (old_psxreg >= 0) {
        EMIT(STW(best_ppcreg, \
            DYNAREC_STATE_REG_OFFSET(old_psxreg), PPC_DYNASTATEREG));
    }

/*  Now, load in the new psxreg value. */
    EMIT(LWZ(best_ppcreg, \
        DYNAREC_STATE_REG_OFFSET(psxreg), PPC_DYNASTATEREG));

/*  Update tracking bits. */
    ppc_reg_map[old_psxreg] = PPC_REG_INVALID;
    ppc_reg_map[psxreg] = best_ppcreg;
    UPDATE_LAST_USE(compiler, best_ppcreg);
    printf("dyna: ppc-%d was psx-%d, is now psx-%d\n", \
        best_ppcreg, old_psxreg, psxreg);
}

