#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug_output.h"

static const char *op_name(Operator op) {
    switch (op) {
        case OP_ADD: return "+";
        case OP_SUB: return "-";
        case OP_MUL: return "*";
        case OP_DIV: return "/";
        case OP_MOD: return "%";
        case OP_LT: return "<";
        case OP_GT: return ">";
        case OP_LE: return "<=";
        case OP_GE: return ">=";
        case OP_EQ: return "==";
        case OP_NE: return "!=";
        case OP_AND: return "&&";
        case OP_OR: return "||";
        case OP_BITAND: return "&";
        case OP_BITOR: return "|";
        case OP_BITXOR: return "^";
        case OP_SHL: return "<<";
        case OP_SHR: return ">>";
        case OP_COALESCE: return "??";
        case OP_NEG: return "NEG";
        case OP_NOT: return "!";
        case OP_BITNOT: return "~";
        case OP_PRE_INC: return "PRE_INC";
        case OP_PRE_DEC: return "PRE_DEC";
        case OP_POST_INC: return "POST_INC";
        case OP_POST_DEC: return "POST_DEC";
        default: return "?";
    }
}

static void indent(FILE *out, int depth) {
    for (int i = 0; i < depth; ++i) fprintf(out, "  ");
}

static void print_type_line(FILE *out, const TypeRef *type) {
    type_ref_print(out, type);
}

static void print_literal(FILE *out, Value v) {
    value_print(out, v);
}

static void dump_ast_node(FILE *out, ASTNode *node, int depth) {
    for (; node; node = node->next) {
        indent(out, depth);
        switch (node->kind) {
            case NODE_PROGRAM:
                fprintf(out, "PROGRAM\n");
                dump_ast_node(out, node->as.program.items, depth + 1);
                break;
            case NODE_STRUCT_DECL:
                fprintf(out, "STRUCT name=%s\n", node->as.struct_decl.name);
                dump_ast_node(out, node->as.struct_decl.fields, depth + 1);
                break;
            case NODE_ENUM_DECL:
                fprintf(out, "ENUM name=%s\n", node->as.enum_decl.name);
                dump_ast_node(out, node->as.enum_decl.members, depth + 1);
                break;
            case NODE_TYPE_ALIAS:
                fprintf(out, "TYPE_ALIAS name=%s target=", node->as.type_alias.name);
                print_type_line(out, node->as.type_alias.target_type);
                fprintf(out, "\n");
                break;
            case NODE_FIELD_DECL:
                fprintf(out, "FIELD name=%s type=", node->as.field_decl.name);
                print_type_line(out, node->as.field_decl.type);
                fprintf(out, "\n");
                break;
            case NODE_ENUM_MEMBER:
                fprintf(out, "ENUM_MEMBER name=%s value=%lld\n", node->as.enum_member.name, node->as.enum_member.value);
                break;
            case NODE_FUNCTION:
                fprintf(out, "FUNCTION name=%s return=", node->as.function.name);
                print_type_line(out, node->as.function.return_type);
                fprintf(out, "\n");
                if (node->as.function.params) {
                    indent(out, depth + 1);
                    fprintf(out, "PARAMS\n");
                    dump_ast_node(out, node->as.function.params, depth + 2);
                }
                if (node->as.function.body) {
                    indent(out, depth + 1);
                    fprintf(out, "BODY\n");
                    dump_ast_node(out, node->as.function.body, depth + 2);
                }
                break;
            case NODE_PARAM:
                fprintf(out, "PARAM name=%s type=", node->as.param.name);
                print_type_line(out, node->as.param.type);
                fprintf(out, "\n");
                break;
            case NODE_BLOCK:
                fprintf(out, "BLOCK\n");
                dump_ast_node(out, node->as.block.statements, depth + 1);
                break;
            case NODE_VAR_DECL:
                fprintf(out, "VAR_DECL name=%s type=", node->as.var_decl.name);
                print_type_line(out, node->as.var_decl.declared_type);
                fprintf(out, " const=%d\n", node->as.var_decl.is_const);
                if (node->as.var_decl.initializer) {
                    indent(out, depth + 1);
                    fprintf(out, "INIT\n");
                    dump_ast_node(out, node->as.var_decl.initializer, depth + 2);
                }
                break;
            case NODE_ASSIGN:
                fprintf(out, "ASSIGN\n");
                indent(out, depth + 1);
                fprintf(out, "TARGET\n");
                dump_ast_node(out, node->as.assign.target, depth + 2);
                indent(out, depth + 1);
                fprintf(out, "VALUE\n");
                dump_ast_node(out, node->as.assign.value, depth + 2);
                break;
            case NODE_IF:
                fprintf(out, "IF\n");
                indent(out, depth + 1); fprintf(out, "COND\n");
                dump_ast_node(out, node->as.if_stmt.condition, depth + 2);
                indent(out, depth + 1); fprintf(out, "THEN\n");
                dump_ast_node(out, node->as.if_stmt.then_branch, depth + 2);
                if (node->as.if_stmt.else_branch) {
                    indent(out, depth + 1); fprintf(out, "ELSE\n");
                    dump_ast_node(out, node->as.if_stmt.else_branch, depth + 2);
                }
                break;
            case NODE_WHILE:
                fprintf(out, "WHILE\n");
                indent(out, depth + 1); fprintf(out, "COND\n");
                dump_ast_node(out, node->as.while_stmt.condition, depth + 2);
                indent(out, depth + 1); fprintf(out, "BODY\n");
                dump_ast_node(out, node->as.while_stmt.body, depth + 2);
                break;
            case NODE_DO_WHILE:
                fprintf(out, "DO_WHILE\n");
                indent(out, depth + 1); fprintf(out, "BODY\n");
                dump_ast_node(out, node->as.do_while_stmt.body, depth + 2);
                indent(out, depth + 1); fprintf(out, "COND\n");
                dump_ast_node(out, node->as.do_while_stmt.condition, depth + 2);
                break;
            case NODE_FOR:
                fprintf(out, "FOR\n");
                if (node->as.for_stmt.init) { indent(out, depth + 1); fprintf(out, "INIT\n"); dump_ast_node(out, node->as.for_stmt.init, depth + 2); }
                if (node->as.for_stmt.condition) { indent(out, depth + 1); fprintf(out, "COND\n"); dump_ast_node(out, node->as.for_stmt.condition, depth + 2); }
                if (node->as.for_stmt.update) { indent(out, depth + 1); fprintf(out, "UPDATE\n"); dump_ast_node(out, node->as.for_stmt.update, depth + 2); }
                indent(out, depth + 1); fprintf(out, "BODY\n");
                dump_ast_node(out, node->as.for_stmt.body, depth + 2);
                break;
            case NODE_RETURN:
                fprintf(out, "RETURN\n");
                if (node->as.return_stmt.expr) dump_ast_node(out, node->as.return_stmt.expr, depth + 1);
                break;
            case NODE_BREAK: fprintf(out, "BREAK\n"); break;
            case NODE_CONTINUE: fprintf(out, "CONTINUE\n"); break;
            case NODE_DEFER:
                fprintf(out, "DEFER\n");
                dump_ast_node(out, node->as.defer_stmt.stmt, depth + 1);
                break;
            case NODE_REVEAL:
                fprintf(out, "REVEAL\n");
                dump_ast_node(out, node->as.reveal_stmt.expr, depth + 1);
                break;
            case NODE_EXPR_STMT:
                fprintf(out, "EXPR_STMT\n");
                dump_ast_node(out, node->as.expr_stmt.expr, depth + 1);
                break;
            case NODE_BINARY:
                fprintf(out, "BINARY op=%s\n", op_name(node->as.binary.op));
                dump_ast_node(out, node->as.binary.left, depth + 1);
                dump_ast_node(out, node->as.binary.right, depth + 1);
                break;
            case NODE_UNARY:
                fprintf(out, "UNARY op=%s\n", op_name(node->as.unary.op));
                dump_ast_node(out, node->as.unary.operand, depth + 1);
                break;
            case NODE_CALL:
                fprintf(out, "CALL name=%s\n", node->as.call.name);
                dump_ast_node(out, node->as.call.args, depth + 1);
                break;
            case NODE_IDENTIFIER:
                fprintf(out, "IDENTIFIER name=%s\n", node->as.identifier.name);
                break;
            case NODE_FIELD_ACCESS:
                fprintf(out, "FIELD_ACCESS field=%s\n", node->as.field_access.field);
                dump_ast_node(out, node->as.field_access.object, depth + 1);
                break;
            case NODE_INDEX_ACCESS:
                fprintf(out, "INDEX_ACCESS\n");
                dump_ast_node(out, node->as.index_access.array, depth + 1);
                dump_ast_node(out, node->as.index_access.index, depth + 1);
                break;
            case NODE_ARRAY_LITERAL:
                fprintf(out, "ARRAY_LITERAL\n");
                dump_ast_node(out, node->as.array_literal.elements, depth + 1);
                break;
            case NODE_LITERAL:
                fprintf(out, "LITERAL value=");
                print_literal(out, node->as.literal.value);
                fprintf(out, "\n");
                break;
            default:
                fprintf(out, "UNKNOWN_NODE\n");
                break;
        }
    }
}

void dump_ast_file(ASTNode *root, FILE *out) {
    if (!root || !out) return;
    dump_ast_node(out, root, 0);
}

typedef struct {
    int temp_count;
    int label_count;
} TACContext;

static int new_temp(TACContext *ctx) { return ++ctx->temp_count; }
static int new_label(TACContext *ctx) { return ++ctx->label_count; }

static void print_expr_inline(FILE *out, ASTNode *expr) {
    if (!expr) { fprintf(out, "<null>"); return; }
    switch (expr->kind) {
        case NODE_IDENTIFIER:
            fprintf(out, "%s", expr->as.identifier.name);
            break;
        case NODE_FIELD_ACCESS:
            print_expr_inline(out, expr->as.field_access.object);
            fprintf(out, ".%s", expr->as.field_access.field);
            break;
        case NODE_INDEX_ACCESS:
            print_expr_inline(out, expr->as.index_access.array);
            fprintf(out, "[");
            print_expr_inline(out, expr->as.index_access.index);
            fprintf(out, "]");
            break;
        case NODE_LITERAL:
            print_literal(out, expr->as.literal.value);
            break;
        default:
            fprintf(out, "t?");
            break;
    }
}

static int emit_expr_tac(FILE *out, ASTNode *expr, TACContext *ctx);
static void emit_stmt_tac(FILE *out, ASTNode *stmt, TACContext *ctx);

static int emit_expr_tac(FILE *out, ASTNode *expr, TACContext *ctx) {
    int t, l, r;
    switch (expr->kind) {
        case NODE_LITERAL:
            t = new_temp(ctx);
            fprintf(out, "t%d = ", t);
            print_literal(out, expr->as.literal.value);
            fprintf(out, "\n");
            return t;
        case NODE_IDENTIFIER:
            t = new_temp(ctx);
            fprintf(out, "t%d = %s\n", t, expr->as.identifier.name);
            return t;
        case NODE_FIELD_ACCESS:
            t = new_temp(ctx);
            fprintf(out, "t%d = ", t);
            print_expr_inline(out, expr);
            fprintf(out, "\n");
            return t;
        case NODE_INDEX_ACCESS:
            t = new_temp(ctx);
            fprintf(out, "t%d = ", t);
            print_expr_inline(out, expr);
            fprintf(out, "\n");
            return t;
        case NODE_ARRAY_LITERAL:
            t = new_temp(ctx);
            fprintf(out, "t%d = [", t);
            for (ASTNode *el = expr->as.array_literal.elements; el; el = el->next) {
                print_expr_inline(out, el);
                if (el->next) fprintf(out, ", ");
            }
            fprintf(out, "]\n");
            return t;
        case NODE_UNARY:
            l = emit_expr_tac(out, expr->as.unary.operand, ctx);
            t = new_temp(ctx);
            fprintf(out, "t%d = %s t%d\n", t, op_name(expr->as.unary.op), l);
            return t;
        case NODE_BINARY:
            l = emit_expr_tac(out, expr->as.binary.left, ctx);
            r = emit_expr_tac(out, expr->as.binary.right, ctx);
            t = new_temp(ctx);
            fprintf(out, "t%d = t%d %s t%d\n", t, l, op_name(expr->as.binary.op), r);
            return t;
        case NODE_CALL: {
            for (ASTNode *arg = expr->as.call.args; arg; arg = arg->next) {
                int at = emit_expr_tac(out, arg, ctx);
                fprintf(out, "param t%d\n", at);
            }
            t = new_temp(ctx);
            fprintf(out, "t%d = call %s, %d\n", t, expr->as.call.name, expr->as.call.args ? 1 : 0);
            return t;
        }
        default:
            t = new_temp(ctx);
            fprintf(out, "t%d = <expr>\n", t);
            return t;
    }
}

static void emit_stmt_tac(FILE *out, ASTNode *stmt, TACContext *ctx) {
    for (; stmt; stmt = stmt->next) {
        int t, l1, l2, l3;
        switch (stmt->kind) {
            case NODE_BLOCK:
                emit_stmt_tac(out, stmt->as.block.statements, ctx);
                break;
            case NODE_VAR_DECL:
                if (stmt->as.var_decl.initializer) {
                    t = emit_expr_tac(out, stmt->as.var_decl.initializer, ctx);
                    fprintf(out, "%s = t%d\n", stmt->as.var_decl.name, t);
                } else {
                    fprintf(out, "%s = <default>\n", stmt->as.var_decl.name);
                }
                break;
            case NODE_ASSIGN:
                t = emit_expr_tac(out, stmt->as.assign.value, ctx);
                print_expr_inline(out, stmt->as.assign.target);
                fprintf(out, " = t%d\n", t);
                break;
            case NODE_REVEAL:
                t = emit_expr_tac(out, stmt->as.reveal_stmt.expr, ctx);
                fprintf(out, "print t%d\n", t);
                break;
            case NODE_EXPR_STMT:
                (void)emit_expr_tac(out, stmt->as.expr_stmt.expr, ctx);
                break;
            case NODE_RETURN:
                if (stmt->as.return_stmt.expr) {
                    t = emit_expr_tac(out, stmt->as.return_stmt.expr, ctx);
                    fprintf(out, "return t%d\n", t);
                } else {
                    fprintf(out, "return\n");
                }
                break;
            case NODE_IF:
                l1 = new_label(ctx); l2 = new_label(ctx); l3 = stmt->as.if_stmt.else_branch ? new_label(ctx) : 0;
                t = emit_expr_tac(out, stmt->as.if_stmt.condition, ctx);
                if (stmt->as.if_stmt.else_branch) {
                    fprintf(out, "if t%d goto L%d\n", t, l1);
                    fprintf(out, "goto L%d\n", l2);
                    fprintf(out, "L%d:\n", l1);
                    emit_stmt_tac(out, stmt->as.if_stmt.then_branch, ctx);
                    fprintf(out, "goto L%d\n", l3);
                    fprintf(out, "L%d:\n", l2);
                    emit_stmt_tac(out, stmt->as.if_stmt.else_branch, ctx);
                    fprintf(out, "L%d:\n", l3);
                } else {
                    fprintf(out, "if t%d goto L%d\n", t, l1);
                    fprintf(out, "goto L%d\n", l2);
                    fprintf(out, "L%d:\n", l1);
                    emit_stmt_tac(out, stmt->as.if_stmt.then_branch, ctx);
                    fprintf(out, "L%d:\n", l2);
                }
                break;
            case NODE_WHILE:
                l1 = new_label(ctx); l2 = new_label(ctx); l3 = new_label(ctx);
                fprintf(out, "L%d:\n", l1);
                t = emit_expr_tac(out, stmt->as.while_stmt.condition, ctx);
                fprintf(out, "if t%d goto L%d\n", t, l2);
                fprintf(out, "goto L%d\n", l3);
                fprintf(out, "L%d:\n", l2);
                emit_stmt_tac(out, stmt->as.while_stmt.body, ctx);
                fprintf(out, "goto L%d\n", l1);
                fprintf(out, "L%d:\n", l3);
                break;
            case NODE_DO_WHILE:
                l1 = new_label(ctx);
                fprintf(out, "L%d:\n", l1);
                emit_stmt_tac(out, stmt->as.do_while_stmt.body, ctx);
                t = emit_expr_tac(out, stmt->as.do_while_stmt.condition, ctx);
                fprintf(out, "if t%d goto L%d\n", t, l1);
                break;
            case NODE_FOR:
                l1 = new_label(ctx); l2 = new_label(ctx); l3 = new_label(ctx);
                if (stmt->as.for_stmt.init) emit_stmt_tac(out, stmt->as.for_stmt.init, ctx);
                fprintf(out, "L%d:\n", l1);
                if (stmt->as.for_stmt.condition) {
                    t = emit_expr_tac(out, stmt->as.for_stmt.condition, ctx);
                    fprintf(out, "if t%d goto L%d\n", t, l2);
                    fprintf(out, "goto L%d\n", l3);
                } else {
                    fprintf(out, "goto L%d\n", l2);
                }
                fprintf(out, "L%d:\n", l2);
                emit_stmt_tac(out, stmt->as.for_stmt.body, ctx);
                if (stmt->as.for_stmt.update) emit_stmt_tac(out, stmt->as.for_stmt.update, ctx);
                fprintf(out, "goto L%d\n", l1);
                fprintf(out, "L%d:\n", l3);
                break;
            case NODE_DEFER:
                fprintf(out, "# defer\n");
                emit_stmt_tac(out, stmt->as.defer_stmt.stmt, ctx);
                break;
            case NODE_BREAK:
                fprintf(out, "break\n");
                break;
            case NODE_CONTINUE:
                fprintf(out, "continue\n");
                break;
            default:
                fprintf(out, "# unsupported stmt kind %d\n", stmt->kind);
                break;
        }
    }
}

void dump_tac_file(ASTNode *root, FILE *out) {
    if (!root || !out || root->kind != NODE_PROGRAM) return;
    TACContext ctx = {0, 0};
    for (ASTNode *item = root->as.program.items; item; item = item->next) {
        if (item->kind == NODE_STRUCT_DECL) {
            fprintf(out, "type %s:\n", item->as.struct_decl.name);
            for (ASTNode *field = item->as.struct_decl.fields; field; field = field->next) {
                fprintf(out, "field %s : ", field->as.field_decl.name);
                print_type_line(out, field->as.field_decl.type);
                fprintf(out, "\n");
            }
            fprintf(out, "end type %s\n\n", item->as.struct_decl.name);
        } else if (item->kind == NODE_ENUM_DECL) {
            fprintf(out, "enum %s:\n", item->as.enum_decl.name);
            for (ASTNode *m = item->as.enum_decl.members; m; m = m->next) {
                fprintf(out, "%s = %lld\n", m->as.enum_member.name, m->as.enum_member.value);
            }
            fprintf(out, "end enum %s\n\n", item->as.enum_decl.name);
        } else if (item->kind == NODE_TYPE_ALIAS) {
            fprintf(out, "alias %s = ", item->as.type_alias.name);
            print_type_line(out, item->as.type_alias.target_type);
            fprintf(out, "\n\n");
        } else if (item->kind == NODE_FUNCTION) {
            fprintf(out, "func %s:\n", item->as.function.name);
            for (ASTNode *p = item->as.function.params; p; p = p->next) {
                fprintf(out, "param_def %s : ", p->as.param.name);
                print_type_line(out, p->as.param.type);
                fprintf(out, "\n");
            }
            emit_stmt_tac(out, item->as.function.body, &ctx);
            fprintf(out, "end func %s\n\n", item->as.function.name);
        }
    }
}

static void dump_symbols_from_stmt(FILE *out, ASTNode *stmt, const char *scope);

static void dump_symbols_from_stmt(FILE *out, ASTNode *stmt, const char *scope) {
    for (; stmt; stmt = stmt->next) {
        switch (stmt->kind) {
            case NODE_BLOCK:
                dump_symbols_from_stmt(out, stmt->as.block.statements, scope);
                break;
            case NODE_VAR_DECL:
                fprintf(out, "name=%s\tkind=variable\ttype=", stmt->as.var_decl.name);
                print_type_line(out, stmt->as.var_decl.declared_type);
                fprintf(out, "\tconst=%d\tscope=%s\n", stmt->as.var_decl.is_const, scope);
                break;
            case NODE_IF:
                dump_symbols_from_stmt(out, stmt->as.if_stmt.then_branch, scope);
                dump_symbols_from_stmt(out, stmt->as.if_stmt.else_branch, scope);
                break;
            case NODE_WHILE:
                dump_symbols_from_stmt(out, stmt->as.while_stmt.body, scope);
                break;
            case NODE_DO_WHILE:
                dump_symbols_from_stmt(out, stmt->as.do_while_stmt.body, scope);
                break;
            case NODE_FOR:
                dump_symbols_from_stmt(out, stmt->as.for_stmt.init, scope);
                dump_symbols_from_stmt(out, stmt->as.for_stmt.body, scope);
                break;
            case NODE_DEFER:
                dump_symbols_from_stmt(out, stmt->as.defer_stmt.stmt, scope);
                break;
            default:
                break;
        }
    }
}

void dump_symbol_table_file(ASTNode *root, FILE *out) {
    if (!root || !out || root->kind != NODE_PROGRAM) return;
    fprintf(out, "=== Symbol Table ===\n");
    for (ASTNode *item = root->as.program.items; item; item = item->next) {
        if (item->kind == NODE_STRUCT_DECL) {
            fprintf(out, "name=%s\tkind=struct\ttype=%s\tscope=global\n", item->as.struct_decl.name, item->as.struct_decl.name);
            for (ASTNode *field = item->as.struct_decl.fields; field; field = field->next) {
                fprintf(out, "name=%s\tkind=field\ttype=", field->as.field_decl.name);
                print_type_line(out, field->as.field_decl.type);
                fprintf(out, "\tscope=%s\n", item->as.struct_decl.name);
            }
        } else if (item->kind == NODE_ENUM_DECL) {
            fprintf(out, "name=%s\tkind=enum\ttype=Metal\tscope=global\n", item->as.enum_decl.name);
            for (ASTNode *m = item->as.enum_decl.members; m; m = m->next) {
                fprintf(out, "name=%s\tkind=enum_member\ttype=Metal\tscope=%s\n", m->as.enum_member.name, item->as.enum_decl.name);
            }
        } else if (item->kind == NODE_TYPE_ALIAS) {
            fprintf(out, "name=%s\tkind=alias\ttype=", item->as.type_alias.name);
            print_type_line(out, item->as.type_alias.target_type);
            fprintf(out, "\tscope=global\n");
        } else if (item->kind == NODE_FUNCTION) {
            fprintf(out, "name=%s\tkind=function\ttype=", item->as.function.name);
            print_type_line(out, item->as.function.return_type);
            fprintf(out, "\tscope=global\n");
            for (ASTNode *p = item->as.function.params; p; p = p->next) {
                fprintf(out, "name=%s\tkind=parameter\ttype=", p->as.param.name);
                print_type_line(out, p->as.param.type);
                fprintf(out, "\tscope=%s\n", item->as.function.name);
            }
            dump_symbols_from_stmt(out, item->as.function.body, item->as.function.name);
        }
    }
}
