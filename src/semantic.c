#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "semantic.h"
#include "symbol_table.h"

typedef struct FunctionSig {
    char *name;
    TypeRef *return_type;
    ASTNode *params;
    struct FunctionSig *next;
} FunctionSig;

typedef struct {
    ASTNode *program;
    FILE *err;
    int errors;
    FunctionSig *functions;
    TypeRef *current_return_type;
    int loop_depth;
    SymbolTable *global_scope;
} Context;

static char *xstrdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    memcpy(p, s, n);
    return p;
}

static TypeRef *primitive_type(ValueType kind) {
    return type_ref_make_primitive(kind);
}

static void report(Context *ctx, int line, const char *msg, const char *name) {
    fprintf(ctx->err, "Semantic error at line %d: %s%s%s\n", line, msg, name ? " " : "", name ? name : "");
    ctx->errors++;
}

static ASTNode *find_type_item(Context *ctx, const char *name) {
    for (ASTNode *item = ctx->program->as.program.items; item; item = item->next) {
        switch (item->kind) {
            case NODE_STRUCT_DECL:
                if (strcmp(item->as.struct_decl.name, name) == 0) return item;
                break;
            case NODE_ENUM_DECL:
                if (strcmp(item->as.enum_decl.name, name) == 0) return item;
                break;
            case NODE_TYPE_ALIAS:
                if (strcmp(item->as.type_alias.name, name) == 0) return item;
                break;
            default:
                break;
        }
    }
    return NULL;
}

static ASTNode *find_struct_decl(Context *ctx, const char *name) {
    ASTNode *item = find_type_item(ctx, name);
    return item && item->kind == NODE_STRUCT_DECL ? item : NULL;
}

static TypeRef *resolve_type(Context *ctx, const TypeRef *type, int line);
static TypeRef *infer_expr(Context *ctx, SymbolTable *scope, ASTNode *expr);
static void check_stmt(Context *ctx, SymbolTable *scope, ASTNode *stmt);

static TypeRef *resolve_type(Context *ctx, const TypeRef *type, int line) {
    if (!type) return primitive_type(TYPE_VOID);
    if (type->kind == TYPE_ARRAY) {
        return type_ref_make_array(resolve_type(ctx, type->element_type, line));
    }
    if (type->kind != TYPE_NAMED) return type_ref_copy(type);

    ASTNode *item = find_type_item(ctx, type->name);
    if (!item) {
        report(ctx, line, "unknown type", type->name);
        return primitive_type(TYPE_ERROR);
    }
    if (item->kind == NODE_TYPE_ALIAS) {
        return resolve_type(ctx, item->as.type_alias.target_type, line);
    }
    if (item->kind == NODE_ENUM_DECL) {
        return primitive_type(TYPE_INT);
    }
    if (item->kind == NODE_STRUCT_DECL) {
        return type_ref_make_struct(item->as.struct_decl.name);
    }
    return primitive_type(TYPE_ERROR);
}

static int count_list(ASTNode *node) {
    int n = 0;
    for (; node; node = node->next) n++;
    return n;
}

static FunctionSig *find_func(Context *ctx, const char *name) {
    for (FunctionSig *f = ctx->functions; f; f = f->next) {
        if (strcmp(f->name, name) == 0) return f;
    }
    return NULL;
}

static void register_types(Context *ctx) {
    for (ASTNode *item = ctx->program->as.program.items; item; item = item->next) {
        if (item->kind == NODE_STRUCT_DECL || item->kind == NODE_ENUM_DECL || item->kind == NODE_TYPE_ALIAS) {
            ASTNode *existing = find_type_item(ctx, item->kind == NODE_TYPE_ALIAS ? item->as.type_alias.name :
                                                     item->kind == NODE_STRUCT_DECL ? item->as.struct_decl.name :
                                                     item->as.enum_decl.name);
            if (existing != item) {
                report(ctx, item->line, "duplicate type declaration", item->kind == NODE_TYPE_ALIAS ? item->as.type_alias.name :
                                                                 item->kind == NODE_STRUCT_DECL ? item->as.struct_decl.name :
                                                                 item->as.enum_decl.name);
            }
        }
    }
}

static void resolve_type_declarations(Context *ctx) {
    for (ASTNode *item = ctx->program->as.program.items; item; item = item->next) {
        if (item->kind == NODE_TYPE_ALIAS) {
            TypeRef *resolved = resolve_type(ctx, item->as.type_alias.target_type, item->line);
            type_ref_free(item->as.type_alias.target_type);
            item->as.type_alias.target_type = resolved;
        } else if (item->kind == NODE_STRUCT_DECL) {
            for (ASTNode *field = item->as.struct_decl.fields; field; field = field->next) {
                TypeRef *resolved = resolve_type(ctx, field->as.field_decl.type, field->line);
                type_ref_free(field->as.field_decl.type);
                field->as.field_decl.type = resolved;
            }
        }
    }
}

static void register_functions(Context *ctx) {
    for (ASTNode *item = ctx->program->as.program.items; item; item = item->next) {
        if (item->kind != NODE_FUNCTION) continue;
        if (find_func(ctx, item->as.function.name)) {
            report(ctx, item->line, "duplicate function", item->as.function.name);
            continue;
        }
        item->as.function.return_type = resolve_type(ctx, item->as.function.return_type, item->line);
        for (ASTNode *p = item->as.function.params; p; p = p->next) {
            TypeRef *resolved = resolve_type(ctx, p->as.param.type, p->line);
            type_ref_free(p->as.param.type);
            p->as.param.type = resolved;
        }
        FunctionSig *sig = (FunctionSig *)calloc(1, sizeof(FunctionSig));
        sig->name = xstrdup(item->as.function.name);
        sig->return_type = type_ref_copy(item->as.function.return_type);
        sig->params = item->as.function.params;
        sig->next = ctx->functions;
        ctx->functions = sig;
    }
}

static void register_enum_constants(Context *ctx) {
    for (ASTNode *item = ctx->program->as.program.items; item; item = item->next) {
        if (item->kind != NODE_ENUM_DECL) continue;
        for (ASTNode *member = item->as.enum_decl.members; member; member = member->next) {
            if (!symtab_define(ctx->global_scope, member->as.enum_member.name, primitive_type(TYPE_INT), 1, value_make_int(member->as.enum_member.value))) {
                report(ctx, member->line, "duplicate enum member", member->as.enum_member.name);
            }
        }
    }
}

static ASTNode *field_in_struct(ASTNode *struct_decl, const char *field_name) {
    for (ASTNode *field = struct_decl ? struct_decl->as.struct_decl.fields : NULL; field; field = field->next) {
        if (strcmp(field->as.field_decl.name, field_name) == 0) return field;
    }
    return NULL;
}

static Symbol *base_symbol(SymbolTable *scope, ASTNode *target) {
    if (!target) return NULL;
    if (target->kind == NODE_IDENTIFIER) return symtab_lookup(scope, target->as.identifier.name);
    if (target->kind == NODE_FIELD_ACCESS) return base_symbol(scope, target->as.field_access.object);
    if (target->kind == NODE_INDEX_ACCESS) return base_symbol(scope, target->as.index_access.array);
    return NULL;
}

static TypeRef *infer_lvalue(Context *ctx, SymbolTable *scope, ASTNode *expr) {
    if (expr->kind == NODE_IDENTIFIER) {
        Symbol *s = symtab_lookup(scope, expr->as.identifier.name);
        if (!s) {
            report(ctx, expr->line, "use of undeclared identifier", expr->as.identifier.name);
            return primitive_type(TYPE_ERROR);
        }
        return type_ref_copy(s->declared_type);
    }
    if (expr->kind == NODE_FIELD_ACCESS) {
        TypeRef *base = infer_lvalue(ctx, scope, expr->as.field_access.object);
        if (!base || base->kind != TYPE_STRUCT) {
            report(ctx, expr->line, "field access requires a struct value", expr->as.field_access.field);
            type_ref_free(base);
            return primitive_type(TYPE_ERROR);
        }
        ASTNode *decl = find_struct_decl(ctx, base->name);
        ASTNode *field = field_in_struct(decl, expr->as.field_access.field);
        if (!field) {
            report(ctx, expr->line, "unknown struct field", expr->as.field_access.field);
            type_ref_free(base);
            return primitive_type(TYPE_ERROR);
        }
        type_ref_free(base);
        return type_ref_copy(field->as.field_decl.type);
    }
    if (expr->kind == NODE_INDEX_ACCESS) {
        TypeRef *array_type = infer_lvalue(ctx, scope, expr->as.index_access.array);
        TypeRef *index_type = infer_expr(ctx, scope, expr->as.index_access.index);
        if (!type_ref_is_integral(index_type)) {
            report(ctx, expr->line, "array index must be integral", NULL);
        }
        type_ref_free(index_type);
        if (!array_type || array_type->kind != TYPE_ARRAY) {
            report(ctx, expr->line, "indexing requires an array value", NULL);
            type_ref_free(array_type);
            return primitive_type(TYPE_ERROR);
        }
        TypeRef *element = type_ref_copy(array_type->element_type);
        type_ref_free(array_type);
        return element;
    }
    report(ctx, expr->line, "invalid assignment target", NULL);
    return primitive_type(TYPE_ERROR);
}

static TypeRef *infer_expr(Context *ctx, SymbolTable *scope, ASTNode *expr) {
    if (!expr) return primitive_type(TYPE_VOID);
    switch (expr->kind) {
        case NODE_LITERAL:
            if (expr->as.literal.value.type == TYPE_STRUCT) return type_ref_make_struct(expr->as.literal.value.type_name);
            if (expr->as.literal.value.type == TYPE_ARRAY) return type_ref_make_array(type_ref_copy(expr->as.literal.value.as.array->element_type));
            return primitive_type(expr->as.literal.value.type);
        case NODE_IDENTIFIER:
        case NODE_FIELD_ACCESS:
        case NODE_INDEX_ACCESS:
            return infer_lvalue(ctx, scope, expr);
        case NODE_ARRAY_LITERAL: {
            if (!expr->as.array_literal.elements) {
                report(ctx, expr->line, "empty array literals are not supported", NULL);
                return primitive_type(TYPE_ERROR);
            }
            TypeRef *first = infer_expr(ctx, scope, expr->as.array_literal.elements);
            for (ASTNode *el = expr->as.array_literal.elements->next; el; el = el->next) {
                TypeRef *cur = infer_expr(ctx, scope, el);
                if (!type_ref_assignable(first, cur)) {
                    report(ctx, el->line, "array literal element type mismatch", NULL);
                }
                type_ref_free(cur);
            }
            return type_ref_make_array(first);
        }
        case NODE_CALL: {
            if (strcmp(expr->as.call.name, "Bind") == 0) return primitive_type(TYPE_INT);
            FunctionSig *f = find_func(ctx, expr->as.call.name);
            if (f) {
                if (count_list(expr->as.call.args) != count_list(f->params)) {
                    report(ctx, expr->line, "wrong number of arguments in call to", expr->as.call.name);
                }
                ASTNode *arg = expr->as.call.args;
                ASTNode *param = f->params;
                while (arg && param) {
                    TypeRef *at = infer_expr(ctx, scope, arg);
                    if (!type_ref_assignable(param->as.param.type, at)) {
                        report(ctx, arg->line, "argument type mismatch for parameter", param->as.param.name);
                    }
                    type_ref_free(at);
                    arg = arg->next;
                    param = param->next;
                }
                return type_ref_copy(f->return_type);
            }
            ASTNode *struct_decl = find_struct_decl(ctx, expr->as.call.name);
            if (struct_decl) {
                if (count_list(expr->as.call.args) != count_list(struct_decl->as.struct_decl.fields)) {
                    report(ctx, expr->line, "wrong number of constructor arguments for", expr->as.call.name);
                }
                ASTNode *arg = expr->as.call.args;
                ASTNode *field = struct_decl->as.struct_decl.fields;
                while (arg && field) {
                    TypeRef *at = infer_expr(ctx, scope, arg);
                    if (!type_ref_assignable(field->as.field_decl.type, at)) {
                        report(ctx, arg->line, "constructor argument type mismatch for field", field->as.field_decl.name);
                    }
                    type_ref_free(at);
                    arg = arg->next;
                    field = field->next;
                }
                return type_ref_make_struct(expr->as.call.name);
            }
            report(ctx, expr->line, "call to unknown function or constructor", expr->as.call.name);
            return primitive_type(TYPE_ERROR);
        }
        case NODE_UNARY: {
            TypeRef *t = infer_expr(ctx, scope, expr->as.unary.operand);
            switch (expr->as.unary.op) {
                case OP_NEG:
                    if (!type_ref_is_numeric(t) && !type_ref_is_integral(t)) {
                        report(ctx, expr->line, "invalid operand for unary '-'", NULL);
                        type_ref_free(t);
                        return primitive_type(TYPE_ERROR);
                    }
                    return t;
                case OP_NOT:
                    type_ref_free(t);
                    return primitive_type(TYPE_BOOL);
                case OP_BITNOT:
                    if (!type_ref_is_integral(t)) {
                        report(ctx, expr->line, "invalid operand for bitwise not", NULL);
                    }
                    type_ref_free(t);
                    return primitive_type(TYPE_INT);
                case OP_PRE_INC:
                case OP_PRE_DEC:
                case OP_POST_INC:
                case OP_POST_DEC: {
                    Symbol *sym = base_symbol(scope, expr->as.unary.operand);
                    if (!sym) {
                        report(ctx, expr->line, "increment/decrement target must be assignable", NULL);
                    } else if (sym->is_const) {
                        report(ctx, expr->line, "cannot modify const variable", sym->name);
                    } else if (!type_ref_is_numeric(t) && !type_ref_is_integral(t)) {
                        report(ctx, expr->line, "increment/decrement requires numeric target", NULL);
                    }
                    return t;
                }
                default:
                    type_ref_free(t);
                    return primitive_type(TYPE_ERROR);
            }
        }
        case NODE_BINARY: {
            TypeRef *lt = infer_expr(ctx, scope, expr->as.binary.left);
            TypeRef *rt = infer_expr(ctx, scope, expr->as.binary.right);
            switch (expr->as.binary.op) {
                case OP_ADD:
                    if (lt->kind == TYPE_STRING && rt->kind == TYPE_STRING) {
                        type_ref_free(rt);
                        return lt;
                    }
                    if (!(type_ref_is_numeric(lt) || type_ref_is_integral(lt)) ||
                        !(type_ref_is_numeric(rt) || type_ref_is_integral(rt))) {
                        report(ctx, expr->line, "invalid operands for '+'", NULL);
                        type_ref_free(lt);
                        type_ref_free(rt);
                        return primitive_type(TYPE_ERROR);
                    }
                    if (lt->kind == TYPE_DOUBLE || rt->kind == TYPE_DOUBLE) {
                        type_ref_free(lt);
                        type_ref_free(rt);
                        return primitive_type(TYPE_DOUBLE);
                    }
                    if (lt->kind == TYPE_FLOAT || rt->kind == TYPE_FLOAT) {
                        type_ref_free(lt);
                        type_ref_free(rt);
                        return primitive_type(TYPE_FLOAT);
                    }
                    type_ref_free(lt);
                    type_ref_free(rt);
                    return primitive_type(TYPE_INT);
                case OP_SUB:
                case OP_MUL:
                case OP_DIV:
                    if (!(type_ref_is_numeric(lt) || type_ref_is_integral(lt)) ||
                        !(type_ref_is_numeric(rt) || type_ref_is_integral(rt))) {
                        report(ctx, expr->line, "invalid arithmetic operands", NULL);
                        type_ref_free(lt);
                        type_ref_free(rt);
                        return primitive_type(TYPE_ERROR);
                    }
                    if (expr->as.binary.op == OP_DIV && expr->as.binary.right->kind == NODE_LITERAL) {
                        Value rv = expr->as.binary.right->as.literal.value;
                        if ((rv.type == TYPE_INT && rv.as.i == 0) ||
                            ((rv.type == TYPE_FLOAT || rv.type == TYPE_DOUBLE) && rv.as.d == 0.0)) {
                            report(ctx, expr->line, "division or modulo by zero", NULL);
                        }
                    }
                    if (lt->kind == TYPE_DOUBLE || rt->kind == TYPE_DOUBLE) {
                        type_ref_free(lt); type_ref_free(rt); return primitive_type(TYPE_DOUBLE);
                    }
                    if (lt->kind == TYPE_FLOAT || rt->kind == TYPE_FLOAT) {
                        type_ref_free(lt); type_ref_free(rt); return primitive_type(TYPE_FLOAT);
                    }
                    type_ref_free(lt); type_ref_free(rt); return primitive_type(TYPE_INT);
                case OP_MOD:
                case OP_BITAND:
                case OP_BITOR:
                case OP_BITXOR:
                case OP_SHL:
                case OP_SHR:
                    if (!type_ref_is_integral(lt) || !type_ref_is_integral(rt)) {
                        report(ctx, expr->line, "invalid integral operands", NULL);
                    }
                    if (expr->as.binary.op == OP_MOD && expr->as.binary.right->kind == NODE_LITERAL) {
                        Value rv = expr->as.binary.right->as.literal.value;
                        if (rv.type == TYPE_INT && rv.as.i == 0) report(ctx, expr->line, "division or modulo by zero", NULL);
                    }
                    type_ref_free(lt); type_ref_free(rt); return primitive_type(TYPE_INT);
                case OP_LT:
                case OP_GT:
                case OP_LE:
                case OP_GE:
                    if (!(type_ref_is_numeric(lt) || type_ref_is_integral(lt)) ||
                        !(type_ref_is_numeric(rt) || type_ref_is_integral(rt))) {
                        report(ctx, expr->line, "invalid operands for comparison", NULL);
                    }
                    type_ref_free(lt); type_ref_free(rt); return primitive_type(TYPE_BOOL);
                case OP_EQ:
                case OP_NE:
                    if (!type_ref_assignable(lt, rt) && !type_ref_assignable(rt, lt)) {
                        report(ctx, expr->line, "incompatible operands for equality comparison", NULL);
                    }
                    type_ref_free(lt); type_ref_free(rt); return primitive_type(TYPE_BOOL);
                case OP_AND:
                case OP_OR:
                    type_ref_free(lt); type_ref_free(rt); return primitive_type(TYPE_BOOL);
                case OP_COALESCE:
                    if (lt->kind == TYPE_NULL) {
                        type_ref_free(lt);
                        return rt;
                    }
                    if (!type_ref_assignable(lt, rt)) {
                        report(ctx, expr->line, "right side of coalesce is not compatible with left side", NULL);
                    }
                    type_ref_free(rt);
                    return lt;
                default:
                    type_ref_free(lt);
                    type_ref_free(rt);
                    return primitive_type(TYPE_ERROR);
            }
        }
        default:
            return primitive_type(TYPE_ERROR);
    }
}

static void check_block(Context *ctx, SymbolTable *parent, ASTNode *block) {
    SymbolTable *scope = symtab_create(parent);
    for (ASTNode *stmt = block->as.block.statements; stmt; stmt = stmt->next) check_stmt(ctx, scope, stmt);
    symtab_destroy(scope);
}

static int stmt_guarantees_return(ASTNode *stmt) {
    if (!stmt) return 0;
    switch (stmt->kind) {
        case NODE_RETURN:
            return 1;
        case NODE_BLOCK:
            for (ASTNode *s = stmt->as.block.statements; s; s = s->next) {
                if (stmt_guarantees_return(s)) return 1;
            }
            return 0;
        case NODE_IF:
            return stmt->as.if_stmt.else_branch &&
                   stmt_guarantees_return(stmt->as.if_stmt.then_branch) &&
                   stmt_guarantees_return(stmt->as.if_stmt.else_branch);
        default:
            return 0;
    }
}

static void check_stmt(Context *ctx, SymbolTable *scope, ASTNode *stmt) {
    if (!stmt) return;
    switch (stmt->kind) {
        case NODE_BLOCK:
            check_block(ctx, scope, stmt);
            break;
        case NODE_VAR_DECL: {
            if (symtab_lookup_current(scope, stmt->as.var_decl.name)) {
                report(ctx, stmt->line, "duplicate declaration of", stmt->as.var_decl.name);
                break;
            }
            TypeRef *resolved = resolve_type(ctx, stmt->as.var_decl.declared_type, stmt->line);
            type_ref_free(stmt->as.var_decl.declared_type);
            stmt->as.var_decl.declared_type = resolved;
            if (stmt->as.var_decl.initializer) {
                TypeRef *rhs = infer_expr(ctx, scope, stmt->as.var_decl.initializer);
                if (!type_ref_assignable(stmt->as.var_decl.declared_type, rhs)) {
                    report(ctx, stmt->line, "initializer type mismatch for", stmt->as.var_decl.name);
                }
                type_ref_free(rhs);
            }
            symtab_define(scope, stmt->as.var_decl.name, stmt->as.var_decl.declared_type, stmt->as.var_decl.is_const, value_make_void());
            break;
        }
        case NODE_ASSIGN: {
            Symbol *sym = base_symbol(scope, stmt->as.assign.target);
            if (!sym) {
                report(ctx, stmt->line, "assignment target is not declared", NULL);
                break;
            }
            if (sym->is_const) {
                report(ctx, stmt->line, "assignment to const variable", sym->name);
                break;
            }
            TypeRef *dst = infer_lvalue(ctx, scope, stmt->as.assign.target);
            TypeRef *rhs = infer_expr(ctx, scope, stmt->as.assign.value);
            if (!type_ref_assignable(dst, rhs)) {
                report(ctx, stmt->line, "assignment type mismatch", NULL);
            }
            type_ref_free(dst);
            type_ref_free(rhs);
            break;
        }
        case NODE_IF: {
            TypeRef *cond = infer_expr(ctx, scope, stmt->as.if_stmt.condition);
            if (cond->kind == TYPE_VOID) report(ctx, stmt->line, "if condition cannot be void", NULL);
            type_ref_free(cond);
            check_stmt(ctx, scope, stmt->as.if_stmt.then_branch);
            check_stmt(ctx, scope, stmt->as.if_stmt.else_branch);
            break;
        }
        case NODE_WHILE: {
            ctx->loop_depth++;
            TypeRef *cond = infer_expr(ctx, scope, stmt->as.while_stmt.condition);
            if (cond->kind == TYPE_VOID) report(ctx, stmt->line, "while condition cannot be void", NULL);
            type_ref_free(cond);
            check_stmt(ctx, scope, stmt->as.while_stmt.body);
            ctx->loop_depth--;
            break;
        }
        case NODE_DO_WHILE: {
            ctx->loop_depth++;
            check_stmt(ctx, scope, stmt->as.do_while_stmt.body);
            TypeRef *cond = infer_expr(ctx, scope, stmt->as.do_while_stmt.condition);
            if (cond->kind == TYPE_VOID) report(ctx, stmt->line, "do-while condition cannot be void", NULL);
            type_ref_free(cond);
            ctx->loop_depth--;
            break;
        }
        case NODE_FOR:
            ctx->loop_depth++;
            if (stmt->as.for_stmt.init) check_stmt(ctx, scope, stmt->as.for_stmt.init);
            if (stmt->as.for_stmt.condition) {
                TypeRef *cond = infer_expr(ctx, scope, stmt->as.for_stmt.condition);
                if (cond->kind == TYPE_VOID) report(ctx, stmt->line, "for condition cannot be void", NULL);
                type_ref_free(cond);
            }
            if (stmt->as.for_stmt.update) infer_expr(ctx, scope, stmt->as.for_stmt.update);
            check_stmt(ctx, scope, stmt->as.for_stmt.body);
            ctx->loop_depth--;
            break;
        case NODE_RETURN: {
            TypeRef *t = stmt->as.return_stmt.expr ? infer_expr(ctx, scope, stmt->as.return_stmt.expr) : primitive_type(TYPE_VOID);
            if (!type_ref_assignable(ctx->current_return_type, t) &&
                !(ctx->current_return_type->kind == TYPE_VOID && t->kind == TYPE_VOID)) {
                report(ctx, stmt->line, "return type mismatch", NULL);
            }
            type_ref_free(t);
            break;
        }
        case NODE_BREAK:
        case NODE_CONTINUE:
            if (ctx->loop_depth == 0) report(ctx, stmt->line, "break/continue used outside loop", NULL);
            break;
        case NODE_DEFER:
            check_stmt(ctx, scope, stmt->as.defer_stmt.stmt);
            break;
        case NODE_REVEAL: {
            TypeRef *t = infer_expr(ctx, scope, stmt->as.reveal_stmt.expr);
            type_ref_free(t);
            break;
        }
        case NODE_EXPR_STMT: {
            TypeRef *t = infer_expr(ctx, scope, stmt->as.expr_stmt.expr);
            type_ref_free(t);
            break;
        }
        default:
            break;
    }
}

SemanticResult semantic_check(ASTNode *program, FILE *err) {
    Context ctx = {0};
    ctx.program = program;
    ctx.err = err;
    ctx.global_scope = symtab_create(NULL);

    register_types(&ctx);
    resolve_type_declarations(&ctx);
    register_functions(&ctx);
    register_enum_constants(&ctx);

    if (!find_func(&ctx, "main")) report(&ctx, program ? program->line : 1, "missing main function", NULL);

    for (ASTNode *item = program->as.program.items; item; item = item->next) {
        if (item->kind != NODE_FUNCTION) continue;
        SymbolTable *scope = symtab_create(ctx.global_scope);
        ctx.current_return_type = item->as.function.return_type;
        for (ASTNode *p = item->as.function.params; p; p = p->next) {
            if (!symtab_define(scope, p->as.param.name, p->as.param.type, 0, value_make_void())) {
                report(&ctx, p->line, "duplicate parameter", p->as.param.name);
            }
        }
        check_stmt(&ctx, scope, item->as.function.body);
        if (item->as.function.return_type->kind != TYPE_VOID && !stmt_guarantees_return(item->as.function.body)) {
            report(&ctx, item->line, "missing return in function", item->as.function.name);
        }
        symtab_destroy(scope);
    }

    while (ctx.functions) {
        FunctionSig *next = ctx.functions->next;
        free(ctx.functions->name);
        type_ref_free(ctx.functions->return_type);
        free(ctx.functions);
        ctx.functions = next;
    }
    symtab_destroy(ctx.global_scope);

    SemanticResult r;
    r.ok = ctx.errors == 0;
    r.error_count = ctx.errors;
    return r;
}
