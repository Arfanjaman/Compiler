#ifndef COSMERE_SYMBOL_TABLE_H
#define COSMERE_SYMBOL_TABLE_H

#include "value.h"

typedef struct Symbol {
    char *name;
    TypeRef *declared_type;
    int is_const;
    Value value;
    struct Symbol *next;
} Symbol;

typedef struct SymbolTable {
    Symbol *symbols;
    struct SymbolTable *parent;
} SymbolTable;

SymbolTable *symtab_create(SymbolTable *parent);
void symtab_destroy(SymbolTable *table);
Symbol *symtab_lookup_current(SymbolTable *table, const char *name);
Symbol *symtab_lookup(SymbolTable *table, const char *name);
int symtab_define(SymbolTable *table, const char *name, const TypeRef *type, int is_const, Value initial_value);
int symtab_assign(SymbolTable *table, const char *name, Value value);

#endif
