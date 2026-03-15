#ifndef COSMERE_DEBUG_OUTPUT_H
#define COSMERE_DEBUG_OUTPUT_H

#include <stdio.h>
#include "ast.h"

void dump_ast_file(ASTNode *root, FILE *out);
void dump_tac_file(ASTNode *root, FILE *out);
void dump_symbol_table_file(ASTNode *root, FILE *out);

#endif