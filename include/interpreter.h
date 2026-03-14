#ifndef COSMERE_INTERPRETER_H
#define COSMERE_INTERPRETER_H

#include "ast.h"

int interpret_program(ASTNode *program, FILE *out, FILE *err);

#endif
