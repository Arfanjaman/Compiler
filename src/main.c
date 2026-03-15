#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <direct.h>
#define COSMERE_MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define COSMERE_MKDIR(path) mkdir(path, 0777)
#endif
#include "debug_output.h"
#include "ast.h"
#include "semantic.h"
#include "interpreter.h"

extern FILE *yyin;
extern int yyparse(void);
extern ASTNode *root;
extern int lexer_error_count;

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <source.cos>\n", argv[0]);
        return 1;
    }

    yyin = fopen(argv[1], "r");
    if (!yyin) {
        perror("Could not open input file");
        return 1;
    }

    if (yyparse() != 0) {
        fclose(yyin);
        return 1;
    }
    fclose(yyin);

    if (lexer_error_count != 0) {
        ast_free(root);
        return 1;
    }

    SemanticResult sem = semantic_check(root, stderr);
    if (!sem.ok) {
        ast_free(root);
        return 1;
    }

    COSMERE_MKDIR("outputs");

    FILE *f_ast = fopen("outputs/ast.txt", "w");
    FILE *f_tac = fopen("outputs/intermediate_tac.txt", "w");
    FILE *f_sym = fopen("outputs/symbol_table.txt", "w");

    if (f_ast) {
        dump_ast_file(root, f_ast);
        fclose(f_ast);
    }
    if (f_tac) {
        dump_tac_file(root, f_tac);
        fclose(f_tac);
    }
    if (f_sym) {
        dump_symbol_table_file(root, f_sym);
        fclose(f_sym);
    }

    int rc = interpret_program(root, stdout, stderr);
    ast_free(root);
    return rc;
}
