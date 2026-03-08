#ifndef COSMERE_AST_H
#define COSMERE_AST_H

#include "value.h"

typedef enum {
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_LT,
    OP_GT,
    OP_LE,
    OP_GE,
    OP_EQ,
    OP_NE,
    OP_AND,
    OP_OR,
    OP_BITAND,
    OP_BITOR,
    OP_BITXOR,
    OP_SHL,
    OP_SHR,
    OP_COALESCE,
    OP_NEG,
    OP_NOT,
    OP_BITNOT,
    OP_PRE_INC,
    OP_PRE_DEC,
    OP_POST_INC,
    OP_POST_DEC
} Operator;

typedef enum {
    NODE_PROGRAM,
    NODE_FUNCTION,
    NODE_STRUCT_DECL,
    NODE_ENUM_DECL,
    NODE_TYPE_ALIAS,
    NODE_FIELD_DECL,
    NODE_ENUM_MEMBER,
    NODE_PARAM,
    NODE_BLOCK,
    NODE_VAR_DECL,
    NODE_ASSIGN,
    NODE_IF,
    NODE_WHILE,
    NODE_DO_WHILE,
    NODE_FOR,
    NODE_RETURN,
    NODE_BREAK,
    NODE_CONTINUE,
    NODE_DEFER,
    NODE_REVEAL,
    NODE_EXPR_STMT,
    NODE_BINARY,
    NODE_UNARY,
    NODE_CALL,
    NODE_IDENTIFIER,
    NODE_FIELD_ACCESS,
    NODE_INDEX_ACCESS,
    NODE_ARRAY_LITERAL,
    NODE_LITERAL
} NodeKind;

typedef struct ASTNode ASTNode;

struct ASTNode {
    NodeKind kind;
    int line;
    ASTNode *next;
    union {
        struct { ASTNode *items; } program;
        struct {
            char *name;
            TypeRef *return_type;
            ASTNode *params;
            ASTNode *body;
        } function;
        struct {
            char *name;
            TypeRef *self_type;
            ASTNode *fields;
        } struct_decl;
        struct {
            char *name;
            ASTNode *members;
        } enum_decl;
        struct {
            char *name;
            TypeRef *target_type;
        } type_alias;
        struct {
            char *name;
            TypeRef *type;
        } field_decl;
        struct {
            char *name;
            long long value;
        } enum_member;
        struct {
            char *name;
            TypeRef *type;
        } param;
        struct { ASTNode *statements; } block;
        struct {
            char *name;
            TypeRef *declared_type;
            int is_const;
            ASTNode *initializer;
        } var_decl;
        struct { ASTNode *target; ASTNode *value; } assign;
        struct {
            ASTNode *condition;
            ASTNode *then_branch;
            ASTNode *else_branch;
        } if_stmt;
        struct { ASTNode *condition; ASTNode *body; } while_stmt;
        struct { ASTNode *body; ASTNode *condition; } do_while_stmt;
        struct {
            ASTNode *init;
            ASTNode *condition;
            ASTNode *update;
            ASTNode *body;
        } for_stmt;
        struct { ASTNode *expr; } return_stmt;
        struct { ASTNode *stmt; } defer_stmt;
        struct { ASTNode *expr; } reveal_stmt;
        struct { ASTNode *expr; } expr_stmt;
        struct { Operator op; ASTNode *left; ASTNode *right; } binary;
        struct { Operator op; ASTNode *operand; } unary;
        struct { char *name; ASTNode *args; } call;
        struct { char *name; } identifier;
        struct { ASTNode *object; char *field; } field_access;
        struct { ASTNode *array; ASTNode *index; } index_access;
        struct { ASTNode *elements; } array_literal;
        struct { Value value; } literal;
    } as;
};

ASTNode *ast_make_program(ASTNode *items, int line);
ASTNode *ast_make_function(const char *name, TypeRef *return_type, ASTNode *params, ASTNode *body, int line);
ASTNode *ast_make_struct_decl(const char *name, ASTNode *fields, int line);
ASTNode *ast_make_enum_decl(const char *name, ASTNode *members, int line);
ASTNode *ast_make_type_alias(const char *name, TypeRef *target_type, int line);
ASTNode *ast_make_field_decl(const char *name, TypeRef *type, int line);
ASTNode *ast_make_enum_member(const char *name, long long value, int line);
ASTNode *ast_make_param(const char *name, TypeRef *type, int line);
ASTNode *ast_make_block(ASTNode *statements, int line);
ASTNode *ast_make_var_decl(const char *name, TypeRef *type, int is_const, ASTNode *initializer, int line);
ASTNode *ast_make_assign(ASTNode *target, ASTNode *value, int line);
ASTNode *ast_make_if(ASTNode *condition, ASTNode *then_branch, ASTNode *else_branch, int line);
ASTNode *ast_make_while(ASTNode *condition, ASTNode *body, int line);
ASTNode *ast_make_do_while(ASTNode *body, ASTNode *condition, int line);
ASTNode *ast_make_for(ASTNode *init, ASTNode *condition, ASTNode *update, ASTNode *body, int line);
ASTNode *ast_make_return(ASTNode *expr, int line);
ASTNode *ast_make_break(int line);
ASTNode *ast_make_continue(int line);
ASTNode *ast_make_defer(ASTNode *stmt, int line);
ASTNode *ast_make_reveal(ASTNode *expr, int line);
ASTNode *ast_make_expr_stmt(ASTNode *expr, int line);
ASTNode *ast_make_binary(Operator op, ASTNode *left, ASTNode *right, int line);
ASTNode *ast_make_unary(Operator op, ASTNode *operand, int line);
ASTNode *ast_make_call(const char *name, ASTNode *args, int line);
ASTNode *ast_make_identifier(const char *name, int line);
ASTNode *ast_make_field_access(ASTNode *object, const char *field, int line);
ASTNode *ast_make_index_access(ASTNode *array, ASTNode *index, int line);
ASTNode *ast_make_array_literal(ASTNode *elements, int line);
ASTNode *ast_make_literal(Value value, int line);
ASTNode *ast_append(ASTNode *list, ASTNode *node);
void ast_free(ASTNode *node);

#endif
