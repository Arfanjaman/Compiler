#include <stdlib.h>
#include <string.h>
#include "ast.h"

static ASTNode *node_new(NodeKind kind, int line) {
    ASTNode *n = (ASTNode *)calloc(1, sizeof(ASTNode));
    n->kind = kind;
    n->line = line;
    return n;
}

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    memcpy(p, s, n);
    return p;
}

ASTNode *ast_make_program(ASTNode *items, int line) {
    ASTNode *n = node_new(NODE_PROGRAM, line);
    n->as.program.items = items;
    return n;
}

ASTNode *ast_make_function(const char *name, TypeRef *return_type, ASTNode *params, ASTNode *body, int line) {
    ASTNode *n = node_new(NODE_FUNCTION, line);
    n->as.function.name = xstrdup(name);
    n->as.function.return_type = return_type;
    n->as.function.params = params;
    n->as.function.body = body;
    return n;
}

ASTNode *ast_make_struct_decl(const char *name, ASTNode *fields, int line) {
    ASTNode *n = node_new(NODE_STRUCT_DECL, line);
    n->as.struct_decl.name = xstrdup(name);
    n->as.struct_decl.self_type = type_ref_make_struct(name);
    n->as.struct_decl.fields = fields;
    return n;
}

ASTNode *ast_make_enum_decl(const char *name, ASTNode *members, int line) {
    ASTNode *n = node_new(NODE_ENUM_DECL, line);
    n->as.enum_decl.name = xstrdup(name);
    n->as.enum_decl.members = members;
    return n;
}

ASTNode *ast_make_type_alias(const char *name, TypeRef *target_type, int line) {
    ASTNode *n = node_new(NODE_TYPE_ALIAS, line);
    n->as.type_alias.name = xstrdup(name);
    n->as.type_alias.target_type = target_type;
    return n;
}

ASTNode *ast_make_field_decl(const char *name, TypeRef *type, int line) {
    ASTNode *n = node_new(NODE_FIELD_DECL, line);
    n->as.field_decl.name = xstrdup(name);
    n->as.field_decl.type = type;
    return n;
}

ASTNode *ast_make_enum_member(const char *name, long long value, int line) {
    ASTNode *n = node_new(NODE_ENUM_MEMBER, line);
    n->as.enum_member.name = xstrdup(name);
    n->as.enum_member.value = value;
    return n;
}

ASTNode *ast_make_param(const char *name, TypeRef *type, int line) {
    ASTNode *n = node_new(NODE_PARAM, line);
    n->as.param.name = xstrdup(name);
    n->as.param.type = type;
    return n;
}

ASTNode *ast_make_block(ASTNode *statements, int line) {
    ASTNode *n = node_new(NODE_BLOCK, line);
    n->as.block.statements = statements;
    return n;
}

ASTNode *ast_make_var_decl(const char *name, TypeRef *type, int is_const, ASTNode *initializer, int line) {
    ASTNode *n = node_new(NODE_VAR_DECL, line);
    n->as.var_decl.name = xstrdup(name);
    n->as.var_decl.declared_type = type;
    n->as.var_decl.is_const = is_const;
    n->as.var_decl.initializer = initializer;
    return n;
}

ASTNode *ast_make_assign(ASTNode *target, ASTNode *value, int line) {
    ASTNode *n = node_new(NODE_ASSIGN, line);
    n->as.assign.target = target;
    n->as.assign.value = value;
    return n;
}

ASTNode *ast_make_if(ASTNode *condition, ASTNode *then_branch, ASTNode *else_branch, int line) {
    ASTNode *n = node_new(NODE_IF, line);
    n->as.if_stmt.condition = condition;
    n->as.if_stmt.then_branch = then_branch;
    n->as.if_stmt.else_branch = else_branch;
    return n;
}

ASTNode *ast_make_while(ASTNode *condition, ASTNode *body, int line) {
    ASTNode *n = node_new(NODE_WHILE, line);
    n->as.while_stmt.condition = condition;
    n->as.while_stmt.body = body;
    return n;
}

ASTNode *ast_make_do_while(ASTNode *body, ASTNode *condition, int line) {
    ASTNode *n = node_new(NODE_DO_WHILE, line);
    n->as.do_while_stmt.body = body;
    n->as.do_while_stmt.condition = condition;
    return n;
}

ASTNode *ast_make_for(ASTNode *init, ASTNode *condition, ASTNode *update, ASTNode *body, int line) {
    ASTNode *n = node_new(NODE_FOR, line);
    n->as.for_stmt.init = init;
    n->as.for_stmt.condition = condition;
    n->as.for_stmt.update = update;
    n->as.for_stmt.body = body;
    return n;
}

ASTNode *ast_make_return(ASTNode *expr, int line) {
    ASTNode *n = node_new(NODE_RETURN, line);
    n->as.return_stmt.expr = expr;
    return n;
}

ASTNode *ast_make_break(int line) {
    return node_new(NODE_BREAK, line);
}

ASTNode *ast_make_continue(int line) {
    return node_new(NODE_CONTINUE, line);
}

ASTNode *ast_make_defer(ASTNode *stmt, int line) {
    ASTNode *n = node_new(NODE_DEFER, line);
    n->as.defer_stmt.stmt = stmt;
    return n;
}

ASTNode *ast_make_reveal(ASTNode *expr, int line) {
    ASTNode *n = node_new(NODE_REVEAL, line);
    n->as.reveal_stmt.expr = expr;
    return n;
}

ASTNode *ast_make_expr_stmt(ASTNode *expr, int line) {
    ASTNode *n = node_new(NODE_EXPR_STMT, line);
    n->as.expr_stmt.expr = expr;
    return n;
}

ASTNode *ast_make_binary(Operator op, ASTNode *left, ASTNode *right, int line) {
    ASTNode *n = node_new(NODE_BINARY, line);
    n->as.binary.op = op;
    n->as.binary.left = left;
    n->as.binary.right = right;
    return n;
}

ASTNode *ast_make_unary(Operator op, ASTNode *operand, int line) {
    ASTNode *n = node_new(NODE_UNARY, line);
    n->as.unary.op = op;
    n->as.unary.operand = operand;
    return n;
}

ASTNode *ast_make_call(const char *name, ASTNode *args, int line) {
    ASTNode *n = node_new(NODE_CALL, line);
    n->as.call.name = xstrdup(name);
    n->as.call.args = args;
    return n;
}

ASTNode *ast_make_identifier(const char *name, int line) {
    ASTNode *n = node_new(NODE_IDENTIFIER, line);
    n->as.identifier.name = xstrdup(name);
    return n;
}

ASTNode *ast_make_field_access(ASTNode *object, const char *field, int line) {
    ASTNode *n = node_new(NODE_FIELD_ACCESS, line);
    n->as.field_access.object = object;
    n->as.field_access.field = xstrdup(field);
    return n;
}

ASTNode *ast_make_index_access(ASTNode *array, ASTNode *index, int line) {
    ASTNode *n = node_new(NODE_INDEX_ACCESS, line);
    n->as.index_access.array = array;
    n->as.index_access.index = index;
    return n;
}

ASTNode *ast_make_array_literal(ASTNode *elements, int line) {
    ASTNode *n = node_new(NODE_ARRAY_LITERAL, line);
    n->as.array_literal.elements = elements;
    return n;
}

ASTNode *ast_make_literal(Value value, int line) {
    ASTNode *n = node_new(NODE_LITERAL, line);
    n->as.literal.value = value_copy(value);
    return n;
}

ASTNode *ast_append(ASTNode *list, ASTNode *node) {
    if (!list) return node;
    ASTNode *cur = list;
    while (cur->next) cur = cur->next;
    cur->next = node;
    return list;
}

void ast_free(ASTNode *node) {
    if (!node) return;
    ast_free(node->next);
    switch (node->kind) {
        case NODE_PROGRAM:
            ast_free(node->as.program.items);
            break;
        case NODE_FUNCTION:
            free(node->as.function.name);
            type_ref_free(node->as.function.return_type);
            ast_free(node->as.function.params);
            ast_free(node->as.function.body);
            break;
        case NODE_STRUCT_DECL:
            free(node->as.struct_decl.name);
            type_ref_free(node->as.struct_decl.self_type);
            ast_free(node->as.struct_decl.fields);
            break;
        case NODE_ENUM_DECL:
            free(node->as.enum_decl.name);
            ast_free(node->as.enum_decl.members);
            break;
        case NODE_TYPE_ALIAS:
            free(node->as.type_alias.name);
            type_ref_free(node->as.type_alias.target_type);
            break;
        case NODE_FIELD_DECL:
            free(node->as.field_decl.name);
            type_ref_free(node->as.field_decl.type);
            break;
        case NODE_ENUM_MEMBER:
            free(node->as.enum_member.name);
            break;
        case NODE_PARAM:
            free(node->as.param.name);
            type_ref_free(node->as.param.type);
            break;
        case NODE_BLOCK:
            ast_free(node->as.block.statements);
            break;
        case NODE_VAR_DECL:
            free(node->as.var_decl.name);
            type_ref_free(node->as.var_decl.declared_type);
            ast_free(node->as.var_decl.initializer);
            break;
        case NODE_ASSIGN:
            ast_free(node->as.assign.target);
            ast_free(node->as.assign.value);
            break;
        case NODE_IF:
            ast_free(node->as.if_stmt.condition);
            ast_free(node->as.if_stmt.then_branch);
            ast_free(node->as.if_stmt.else_branch);
            break;
        case NODE_WHILE:
            ast_free(node->as.while_stmt.condition);
            ast_free(node->as.while_stmt.body);
            break;
        case NODE_DO_WHILE:
            ast_free(node->as.do_while_stmt.body);
            ast_free(node->as.do_while_stmt.condition);
            break;
        case NODE_FOR:
            ast_free(node->as.for_stmt.init);
            ast_free(node->as.for_stmt.condition);
            ast_free(node->as.for_stmt.update);
            ast_free(node->as.for_stmt.body);
            break;
        case NODE_RETURN:
            ast_free(node->as.return_stmt.expr);
            break;
        case NODE_DEFER:
            ast_free(node->as.defer_stmt.stmt);
            break;
        case NODE_REVEAL:
            ast_free(node->as.reveal_stmt.expr);
            break;
        case NODE_EXPR_STMT:
            ast_free(node->as.expr_stmt.expr);
            break;
        case NODE_BINARY:
            ast_free(node->as.binary.left);
            ast_free(node->as.binary.right);
            break;
        case NODE_UNARY:
            ast_free(node->as.unary.operand);
            break;
        case NODE_CALL:
            free(node->as.call.name);
            ast_free(node->as.call.args);
            break;
        case NODE_IDENTIFIER:
            free(node->as.identifier.name);
            break;
        case NODE_FIELD_ACCESS:
            ast_free(node->as.field_access.object);
            free(node->as.field_access.field);
            break;
        case NODE_INDEX_ACCESS:
            ast_free(node->as.index_access.array);
            ast_free(node->as.index_access.index);
            break;
        case NODE_ARRAY_LITERAL:
            ast_free(node->as.array_literal.elements);
            break;
        case NODE_LITERAL:
            value_free(&node->as.literal.value);
            break;
        case NODE_BREAK:
        case NODE_CONTINUE:
            break;
    }
    free(node);
}
