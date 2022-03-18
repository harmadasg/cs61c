#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "tables.h"
#include "translate_utils.h"
#include "translate.h"

const int RS_SHIFT_AMOUNT = 21;
const int RT_SHIFT_AMOUNT = 16;
const int RD_SHIFT_AMOUNT = 11;
const int SHAMT_SHIFT_AMOUNT = 6;
const int OP_CODE_SHIFT_AMOUNT = 26;
const int MIN_16_BIT_INT = -32768;
const int MAX_16_BIT_INT = 65535;
const int MIN_32_BIT_INT = -2147483648;
const int MAX_32_BIT_INT = 4294967295;

/* Writes instructions during the assembler's first pass to OUTPUT. The case
   for general instructions has already been completed, but you need to write
   code to translate the li and blt pseudoinstructions. Your pseudoinstruction 
   expansions should not have any side effects.

   NAME is the name of the instruction, ARGS is an array of the arguments, and
   NUM_ARGS specifies the number of items in ARGS.

   Error checking for regular instructions are done in pass two. However, for
   pseudoinstructions, you must make sure that ARGS contains the correct number
   of arguments. You do NOT need to check whether the registers / label are 
   valid, since that will be checked in part two.

   Also for li:
    - make sure that the number is representable by 32 bits. (Hint: the number 
        can be both signed or unsigned).
    - if the immediate can fit in the imm field of an addiu instruction, then
        expand li into a single addiu instruction. Otherwise, expand it into 
        a lui-ori pair.

   And for blt:
    - your expansion should use the fewest number of instructions possible.

   MARS has slightly different translation rules for li, and it allows numbers
   larger than the largest 32 bit number to be loaded with li. You should follow
   the above rules if MARS behaves differently.

   Use fprintf() to write. If writing multiple instructions, make sure that 
   each instruction is on a different line.

   Returns the number of instructions written (so 0 if there were any errors).
 */
unsigned write_pass_one(FILE* output, const char* name, char** args, int num_args) {
    if (!output) return 0;
    if (strcmp(name, "li") == 0) {
        if (num_args != 2) return 0;

        long int num;
        int err = translate_num(&num, args[1], MIN_32_BIT_INT, MAX_32_BIT_INT);
        if (err == -1) return 0;

        if (num >= MIN_16_BIT_INT && num <= MAX_16_BIT_INT) {
            fprintf(output, "%s %s %s %s\n", "addiu", args[0], "$0", args[1]);
            return 1;
        }
        fprintf(output, "%s, %s %ld\n", "lui", "$at", num & 0xFFFF0000);
        fprintf(output, "%s, %s %s, %ld\n", "ori", args[0], "$at", num & 0xFFFF);
        return 2;

    } else if (strcmp(name, "blt") == 0) {
        if (num_args != 3) return 0;

        fprintf(output, "%s, %s %s %s\n", "slt", "$at", args[0], args[1]);
        fprintf(output, "%s, %s %s %s\n", "bne", "$at", "$0", args[2]);
        return 2;

    } else {
        write_inst_string(output, name, args, num_args);
        return 1;
    }
}

/* Writes the instruction in hexadecimal format to OUTPUT during pass #2.
   
   NAME is the name of the instruction, ARGS is an array of the arguments, and
   NUM_ARGS specifies the number of items in ARGS. 

   The symbol table (SYMTBL) is given for any symbols that need to be resolved
   at this step. If a symbol should be relocated, it should be added to the
   relocation table (RELTBL), and the fields for that symbol should be set to
   all zeros. 

   You must perform error checking on all instructions and make sure that their
   arguments are valid. If an instruction is invalid, you should not write 
   anything to OUTPUT but simply return -1. MARS may be a useful resource for
   this step.

   Note the use of helper functions. Consider writing your own! If the function
   definition comes afterwards, you must declare it first (see translate.h).

   Returns 0 on success and -1 on error. 
 */
int translate_inst(FILE* output, const char* name, char** args, size_t num_args, uint32_t addr,
    SymbolTable* symtbl, SymbolTable* reltbl) {
    if (strcmp(name, "addu") == 0)       return write_rtype (0x21, output, args, num_args);
    else if (strcmp(name, "or") == 0)    return write_rtype (0x25, output, args, num_args);
    else if (strcmp(name, "slt") == 0)   return write_rtype (0x2a, output, args, num_args);
    else if (strcmp(name, "sltu") == 0)  return write_rtype (0x2b, output, args, num_args);
    else if (strcmp(name, "jr") == 0)    return write_jr    (0x08, output, args, num_args);
    else if (strcmp(name, "sll") == 0)   return write_shift (0x00, output, args, num_args);
    else if (strcmp(name, "addiu") == 0) return write_addiu (0x09, output, args, num_args);
    else if (strcmp(name, "ori") == 0)   return write_ori   (0x0d, output, args, num_args);
    else if (strcmp(name, "lui") == 0)   return write_lui   (0x0f, output, args, num_args);
    else if (strcmp(name, "lb") == 0)    return write_mem   (0x20, output, args, num_args);
    else if (strcmp(name, "lbu") == 0)   return write_mem   (0x24, output, args, num_args);
    else if (strcmp(name, "lw") == 0)    return write_mem   (0x23, output, args, num_args);
    else if (strcmp(name, "sb") == 0)    return write_mem   (0x28, output, args, num_args);
    else if (strcmp(name, "sw") == 0)    return write_mem   (0x2b, output, args, num_args);
    else if (strcmp(name, "beq") == 0)   return write_branch(0x04, output, args, num_args, addr, symtbl);
    else if (strcmp(name, "bne") == 0)   return write_branch(0x05, output, args, num_args, addr, symtbl);
    else if (strcmp(name, "j") == 0)     return write_jump   (0x02, output, args, num_args, addr, symtbl, reltbl);
    else if (strcmp(name, "jal") == 0)   return write_jump   (0x03, output, args, num_args, addr, symtbl, reltbl);
    else                                 return -1;
}

/* A helper function for writing most R-type instructions. You should use
   translate_reg() to parse registers and write_inst_hex() to write to 
   OUTPUT. Both are defined in translate_utils.h.

   This function is INCOMPLETE. Complete the implementation below. You will
   find bitwise operations to be the cleanest way to complete this function.
 */
int write_rtype(uint8_t funct, FILE* output, char** args, size_t num_args) {
    // Perhaps perform some error checking?
    if (!output || num_args != 3)
        return -1;

    int rd = translate_reg(args[0]);
    int rs = translate_reg(args[1]);
    int rt = translate_reg(args[2]);

    if (rd == -1 || rs == -1 || rt == -1)
        return -1;

    uint32_t instruction = (rs << RS_SHIFT_AMOUNT) + (rt << RT_SHIFT_AMOUNT) + (rd << RD_SHIFT_AMOUNT) + funct;
    write_inst_hex(output, instruction);
    return 0;
}

/* A helper function for writing shift instructions. You should use 
   translate_num() to parse numerical arguments. translate_num() is defined
   in translate_utils.h.

   This function is INCOMPLETE. Complete the implementation below. You will
   find bitwise operations to be the cleanest way to complete this function.
 */
int write_shift(uint8_t funct, FILE* output, char** args, size_t num_args) {
	// Perhaps perform some error checking?
    if (!output || num_args != 3)
        return -1;

    long int shamt;
    int rd = translate_reg(args[0]);
    int rt = translate_reg(args[1]);
    int err = translate_num(&shamt, args[2], 0, 31);

    if (rd == -1 || rt == -1 || err == -1)
        return -1;

    uint32_t instruction =  (rt << RT_SHIFT_AMOUNT) + (rd << RD_SHIFT_AMOUNT) + (shamt << SHAMT_SHIFT_AMOUNT) + funct;
    write_inst_hex(output, instruction);
    return 0;
}

int write_jr(uint8_t funct, FILE* output, char** args, size_t num_args) {
    if (!output || num_args != 1)
        return -1;

    int rs = translate_reg(args[0]);

    if (rs == -1)
        return -1;

    uint32_t instruction = (rs << RS_SHIFT_AMOUNT) + funct;
    write_inst_hex(output, instruction);
    return 0;
}

int write_itype(uint8_t opcode, FILE* output, char** args, size_t num_args) {
    if (!output || num_args != 3)
        return -1;

    long int imm;
    int rt = translate_reg(args[0]);
    int rs = translate_reg(args[1]);
    int err = translate_num(&imm, args[2], MIN_16_BIT_INT, MAX_16_BIT_INT);

    if (rt == -1 || rs == -1 || err == -1)
        return -1;

    uint32_t instruction = (opcode << OP_CODE_SHIFT_AMOUNT) + (rt << RT_SHIFT_AMOUNT) + (rs << RS_SHIFT_AMOUNT) + imm;
    write_inst_hex(output, instruction);
    return 0;
}

int write_addiu(uint8_t opcode, FILE* output, char** args, size_t num_args) {
    return write_itype(opcode, output, args, num_args);
}

int write_ori(uint8_t opcode, FILE* output, char** args, size_t num_args) {
    return write_itype(opcode, output, args, num_args);
}

int write_lui(uint8_t opcode, FILE* output, char** args, size_t num_args) {
   if (!output || num_args != 2)
        return -1;

    long int imm;
    int rt = translate_reg(args[0]);
    int err = translate_num(&imm, args[2], -32768, 65535);

    if (rt == -1 || err == -1)
        return -1;

    uint32_t instruction =  (opcode << OP_CODE_SHIFT_AMOUNT) + (rt << RT_SHIFT_AMOUNT) + imm;
    write_inst_hex(output, instruction);
    return 0;
}

int write_mem(uint8_t opcode, FILE* output, char** args, size_t num_args) {
    return write_itype(opcode, output, args, num_args);
}

int write_branch(uint8_t opcode, FILE* output, char** args, size_t num_args, uint32_t addr, SymbolTable* symtbl) {
    if (!output || !symtbl || num_args != 3)
        return -1;

    int rs = translate_reg(args[0]);
    int rt = translate_reg(args[1]);

    if (rs == -1 || rt == -1)
        return -1;

    char* name = args[2];
    long int lbl_addr = get_addr_for_symbol(symtbl, name);
    if (lbl_addr == -1)
        return -1;

    uint32_t instruction =  (opcode << OP_CODE_SHIFT_AMOUNT) + (rt << RT_SHIFT_AMOUNT) + (rs << RS_SHIFT_AMOUNT) + lbl_addr;
    write_inst_hex(output, instruction);
    return 0;
}

int write_jump(uint8_t opcode, FILE* output, char** args, size_t num_args, uint32_t addr, SymbolTable* symtbl, SymbolTable* reltbl) {
    if (!output || !reltbl || num_args != 1)
        return -1;

    char* name = args[0];
    add_to_table(reltbl, name, addr);
    long int lbl_addr = get_addr_for_symbol(symtbl, name);
    if (lbl_addr == -1)
        return -1;

    uint32_t instruction =  (opcode << OP_CODE_SHIFT_AMOUNT) + lbl_addr;
    write_inst_hex(output, instruction);
    return 0;
}
