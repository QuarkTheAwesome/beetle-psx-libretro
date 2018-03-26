/* PSX -> 32-bit big-endian PowerPC dynarec.
   Written based on docs for the 750CL, but should conform to the UISA
   750CL: https://fail0verflow.com/media/files/ppc_750cl.pdf
   PEM (referenced by 750CL manual): https://preview.tinyurl.com/ycws6xx9

   Originally by Ash Logan <quarktheawesome@gmail.com>
   For licensing, see the LICENSE or COPYING file included with this repository.
*/

#ifndef __DYNAREC_PPC32_H__
#define __DYNAREC_PPC32_H__

/* Maximum length of a recompiled instruction in bytes.
   Worst cases:
      - sltiu: 5 instructions, + possible 4 regsaves/loads = 9 instructions
      - sltu: 4 instructions + possible 6 regsaves/loads = 10 instructions
      - addi: 2 instructions + overflow + possible 4 regloads = 6+ instructions
      - seriously sltu is the awful one here
   12 is a safe bet for now? Will have to update as time goes on. */
#define DYNAREC_INSTRUCTION_MAX_LEN  (12 * 4)

#endif //__DYNAREC_PPC32_H__