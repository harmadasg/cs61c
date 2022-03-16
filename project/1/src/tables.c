
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "utils.h"
#include "tables.h"

const int SYMTBL_NON_UNIQUE = 0;
const int SYMTBL_UNIQUE_NAME = 1;

const int INIT_CAPACITY = 10;

/*******************************
 * Helper Functions
 *******************************/

void allocation_failed() {
    write_to_log("Error: allocation failed\n");
    exit(1);
}

void addr_alignment_incorrect() {
    write_to_log("Error: address is not a multiple of 4.\n");
}

void name_already_exists(const char* name) {
    write_to_log("Error: name '%s' already exists in table.\n", name);
}

void write_symbol(FILE* output, uint32_t addr, const char* name) {
    fprintf(output, "%u\t%s\n", addr, name);
}

/*******************************
 * Symbol Table Functions
 *******************************/

/* Creates a new SymbolTable containg 0 elements and returns a pointer to that
   table. Multiple SymbolTables may exist at the same time. 
   If memory allocation fails, you should call allocation_failed(). 
   Mode will be either SYMTBL_NON_UNIQUE or SYMTBL_UNIQUE_NAME. You will need
   to store this value for use during add_to_table().
 */
SymbolTable* create_table(int mode) {
    SymbolTable* retval = malloc(sizeof(SymbolTable));

    if (!retval) {
        allocation_failed();
    }

    Symbol* tbl = malloc(INIT_CAPACITY * sizeof(Symbol));

    if (!tbl) {
        free(retval);
        allocation_failed();
    }

    retval->tbl = tbl;
    retval->len = 0;
    retval->cap = INIT_CAPACITY;
    retval->mode = mode;

    return retval;
}

/* Frees the given SymbolTable and all associated memory. */
void free_table(SymbolTable* table) {
    if(!table) return;
    if (!table->tbl) {
        free(table);
        return;
    }

    for(int i = 0; i < table->len; i++) {
        Symbol curr = table->tbl[i];
        if (!curr.name) continue;
        free(curr.name);
    }
    free(table->tbl);
    free(table);
}

void resize(SymbolTable* table) {
    if (!table || !table->tbl) return;

    int newcap = 2 * table->cap;
    Symbol* tbl = malloc(newcap * sizeof(Symbol));

    if (!tbl) {
        free_table(table);
        allocation_failed();
    }

    for (int i = 0; i < table->len; i++) {
        tbl[i].name = table->tbl[i].name;
        tbl[i].addr = table->tbl[i].addr;
    }

    free(table->tbl);
    table->cap = newcap;
    table->tbl = tbl;
}

/* Adds a new symbol and its address to the SymbolTable pointed to by TABLE. 
   ADDR is given as the byte offset from the first instruction. The SymbolTable
   must be able to resize itself as more elements are added. 

   Note that NAME may point to a temporary array, so it is not safe to simply
   store the NAME pointer. You must store a copy of the given string.

   If ADDR is not word-aligned, you should call addr_alignment_incorrect() and
   return -1. If the table's mode is SYMTBL_UNIQUE_NAME and NAME already exists 
   in the table, you should call name_already_exists() and return -1. If memory
   allocation fails, you should call allocation_failed(). 

   Otherwise, you should store the symbol name and address and return 0.
 */
int add_to_table(SymbolTable* table, const char* name, uint32_t addr) {
    char* newstr = malloc(sizeof(name));
    if (!newstr) {
        free_table(table);
        allocation_failed();
    }
    strcpy(newstr, name);

    if (addr % 4 != 0) {
        addr_alignment_incorrect();
        return -1;
    }

    if (table->mode == SYMTBL_UNIQUE_NAME
        && get_addr_for_symbol(table, name) != -1) {
        name_already_exists(name);
        return -1;
    }

    int index = table->len;
    if (index == table->cap)
        resize(table);

    table->tbl[index].name = newstr;
    table->tbl[index].addr = addr;
    table->len++;

    return 0;
}

/* Returns the address (byte offset) of the given symbol. If a symbol with name
   NAME is not present in TABLE, return -1.
 */
int64_t get_addr_for_symbol(SymbolTable* table, const char* name) {
    int retval = -1;
    if(!table) return retval;

    for(int i = 0; i < table->len; i++) {
        Symbol curr = table->tbl[i];
        if (strcmp(curr.name, name) == 0)
            retval = curr.addr;
    }
    return retval;
}

/* Writes the SymbolTable TABLE to OUTPUT. You should use write_symbol() to
   perform the write. Do not print any additional whitespace or characters.
 */
void write_table(SymbolTable* table, FILE* output) {
    for (int i = 0; i < table->len; i++) {
        Symbol curr = table->tbl[i];
        write_symbol(output, curr.addr, curr.name);
    }
}
