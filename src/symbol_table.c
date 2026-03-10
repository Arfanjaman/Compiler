#include <stdlib.h>
#include <string.h>
#include "symbol_table.h"

static char *xstrdup(const char *s)
{
    if (!s)
        return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    memcpy(p, s, n);
    return p;
}

SymbolTable *symtab_create(SymbolTable *parent)
{
    SymbolTable *t = (SymbolTable *)calloc(1, sizeof(SymbolTable));
    t->parent = parent;
    return t;
}

void symtab_destroy(SymbolTable *table)
{
    if (!table)
        return;
    Symbol *s = table->symbols;
    while (s)
    {
        Symbol *next = s->next;
        free(s->name);
        type_ref_free(s->declared_type);
        value_free(&s->value);
        free(s);
        s = next;
    }
    free(table);
}

Symbol *symtab_lookup_current(SymbolTable *table, const char *name)
{
    for (Symbol *s = table ? table->symbols : NULL; s; s = s->next)
    {
        if (strcmp(s->name, name) == 0)
            return s;
    }
    return NULL;
}

Symbol *symtab_lookup(SymbolTable *table, const char *name)
{
    for (SymbolTable *t = table; t; t = t->parent)
    {
        Symbol *s = symtab_lookup_current(t, name);
        if (s)
            return s;
    }
    return NULL;
}

int symtab_define(SymbolTable *table, const char *name, const TypeRef *type, int is_const, Value initial_value)
{
    if (!table || symtab_lookup_current(table, name))
        return 0;
    Symbol *s = (Symbol *)calloc(1, sizeof(Symbol));
    s->name = xstrdup(name);
    s->declared_type = type_ref_copy(type);
    s->is_const = is_const;
    s->value = value_copy(initial_value);
    s->next = table->symbols;
    table->symbols = s;
    return 1;
}

int symtab_assign(SymbolTable *table, const char *name, Value value)
{
    Symbol *s = symtab_lookup(table, name);
    if (!s || s->is_const)
        return 0;
    value_free(&s->value);
    s->value = value_copy(value);
    return 1;
}
