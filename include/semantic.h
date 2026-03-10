#ifndef COSMERE_SEMANTIC_H
#define COSMERE_SEMANTIC_H

#include "ast.h"

typedef struct {
    int ok;
    int error_count;
} SemanticResult;

SemanticResult semantic_check(ASTNode *program, FILE *err);

#endif
