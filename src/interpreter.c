#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "interpreter.h"
#include "symbol_table.h"

typedef struct FunctionEntry
{
    char *name;
    ASTNode *node;
    struct FunctionEntry *next;
} FunctionEntry;

typedef enum
{
    FLOW_NORMAL,
    FLOW_RETURN,
    FLOW_BREAK,
    FLOW_CONTINUE,
    FLOW_ERROR
} FlowTag;

typedef struct
{
    FlowTag tag;
    Value value;
} Flow;

typedef struct DeferItem
{
    ASTNode *stmt;
    struct DeferItem *next;
} DeferItem;

typedef struct Frame
{
    SymbolTable *scope;
    DeferItem *defers;
    struct Frame *parent;
} Frame;

typedef struct
{
    Value *slot;
    TypeRef *type;
    int is_const;
    int ok;
} Location;

typedef struct
{
    ASTNode *program;
    FunctionEntry *functions;
    SymbolTable *global_scope;
    FILE *out;
    FILE *err;
    Frame *frame;
} Runtime;

static char *xstrdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    memcpy(p, s, n);
    return p;
}

static void runtime_error(Runtime *rt, int line, const char *msg) { fprintf(rt->err, "Runtime error at line %d: %s\n", line, msg); }

static FunctionEntry *find_function(Runtime *rt, const char *name)
{
    for (FunctionEntry *f = rt->functions; f; f = f->next)
        if (strcmp(f->name, name) == 0)
            return f;
    return NULL;
}

static ASTNode *find_struct_decl(Runtime *rt, const char *name)
{
    for (ASTNode *item = rt->program->as.program.items; item; item = item->next) {
        if (item->kind == NODE_STRUCT_DECL && strcmp(item->as.struct_decl.name, name) == 0) return item;
    }
    return NULL;
}

static void push_frame(Runtime *rt, SymbolTable *scope)
{
    Frame *f = calloc(1, sizeof(Frame));
    f->scope = scope;
    f->parent = rt->frame;
    rt->frame = f;
}

static void run_defers(Runtime *rt);
static void pop_frame(Runtime *rt)
{
    if (!rt->frame)
        return;
    run_defers(rt);
    Frame *old = rt->frame;
    rt->frame = old->parent;
    while (old->defers)
    {
        DeferItem *n = old->defers->next;
        free(old->defers);
        old->defers = n;
    }
    symtab_destroy(old->scope);
    free(old);
}

static void register_defer(Runtime *rt, ASTNode *stmt)
{
    DeferItem *d = calloc(1, sizeof(DeferItem));
    d->stmt = stmt;
    d->next = rt->frame->defers;
    rt->frame->defers = d;
}

static Value eval_expr(Runtime *rt, ASTNode *expr);
static Flow exec_stmt(Runtime *rt, ASTNode *stmt);

static void run_defers(Runtime *rt)
{
    for (DeferItem *d = rt->frame->defers; d; d = d->next)
    {
        Flow f = exec_stmt(rt, d->stmt);
        if (f.tag == FLOW_ERROR)
            break;
    }
}

static long long as_int(Value v)
{
    if (v.type == TYPE_INT)
        return v.as.i;
    if (v.type == TYPE_BOOL)
        return v.as.b;
    if (v.type == TYPE_CHAR)
        return (unsigned char)v.as.c;
    return (long long)v.as.d;
}

static double as_double(Value v)
{
    if (v.type == TYPE_DOUBLE || v.type == TYPE_FLOAT)
        return v.as.d;
    if (v.type == TYPE_INT)
        return (double)v.as.i;
    if (v.type == TYPE_BOOL)
        return (double)v.as.b;
    if (v.type == TYPE_CHAR)
        return (unsigned char)v.as.c;
    return 0.0;
}

static int is_zero_value(Value v)
{
    if (v.type == TYPE_INT) return v.as.i == 0;
    if (v.type == TYPE_FLOAT || v.type == TYPE_DOUBLE) return v.as.d == 0.0;
    if (v.type == TYPE_BOOL) return v.as.b == 0;
    if (v.type == TYPE_CHAR) return v.as.c == '\0';
    return 0;
}

static TypeRef *type_ref_from_value(const Value *value)
{
    if (!value) return type_ref_make_primitive(TYPE_ERROR);
    switch (value->type) {
        case TYPE_ARRAY:
            return type_ref_make_array(type_ref_copy(value->as.array ? value->as.array->element_type : type_ref_make_primitive(TYPE_ERROR)));
        case TYPE_STRUCT:
            return type_ref_make_struct(value->type_name ? value->type_name : "");
        default:
            return type_ref_make_primitive(value->type);
    }
}

static Value default_value_for_type(Runtime *rt, const TypeRef *type)
{
    if (!type) return value_make_void();
    switch (type->kind) {
        case TYPE_INT: return value_make_int(0);
        case TYPE_FLOAT: return value_make_float(0.0);
        case TYPE_DOUBLE: return value_make_double(0.0);
        case TYPE_CHAR: return value_make_char('\0');
        case TYPE_BOOL: return value_make_bool(0);
        case TYPE_STRING: return value_make_string("");
        case TYPE_ARRAY: return value_make_array(type->element_type, 0);
        case TYPE_STRUCT: {
            ASTNode *decl = find_struct_decl(rt, type->name);
            StructValue *record = struct_value_create();
            if (decl) {
                for (ASTNode *field = decl->as.struct_decl.fields; field; field = field->next) {
                    Value field_default = default_value_for_type(rt, field->as.field_decl.type);
                    struct_value_add_field(record, field->as.field_decl.name, field->as.field_decl.type, field_default);
                    value_free(&field_default);
                }
            }
            return value_make_struct(type->name, record);
        }
        case TYPE_VOID: return value_make_void();
        default: return value_make_null();
    }
}

static Value coerce_value(Runtime *rt, Value value, const TypeRef *dest, int line)
{
    if (dest->kind == TYPE_STRUCT || dest->kind == TYPE_ARRAY) {
        TypeRef *src = type_ref_from_value(&value);
        int ok = type_ref_assignable(dest, src);
        type_ref_free(src);
        if (!ok) {
            runtime_error(rt, line, "type mismatch");
            return value_make_error();
        }
        return value_copy(value);
    }
    Value casted = value_cast(value, dest);
    if (casted.type == TYPE_ERROR) runtime_error(rt, line, "type mismatch");
    return casted;
}

static Location resolve_location(Runtime *rt, ASTNode *expr)
{
    Location loc = {0};
    if (!expr) return loc;
    if (expr->kind == NODE_IDENTIFIER) {
        Symbol *s = symtab_lookup(rt->frame->scope, expr->as.identifier.name);
        if (!s) {
            runtime_error(rt, expr->line, "undeclared identifier");
            return loc;
        }
        loc.slot = &s->value;
        loc.type = s->declared_type;
        loc.is_const = s->is_const;
        loc.ok = 1;
        return loc;
    }
    if (expr->kind == NODE_FIELD_ACCESS) {
        Location base = resolve_location(rt, expr->as.field_access.object);
        if (!base.ok || !base.slot || base.slot->type != TYPE_STRUCT || !base.slot->as.record) {
            runtime_error(rt, expr->line, "field access requires struct value");
            return loc;
        }
        StructFieldValue *field = struct_value_find_field(base.slot->as.record, expr->as.field_access.field);
        if (!field) {
            runtime_error(rt, expr->line, "unknown struct field");
            return loc;
        }
        loc.slot = &field->value;
        loc.type = field->type;
        loc.is_const = base.is_const;
        loc.ok = 1;
        return loc;
    }
    if (expr->kind == NODE_INDEX_ACCESS) {
        Location base = resolve_location(rt, expr->as.index_access.array);
        if (!base.ok || !base.slot || base.slot->type != TYPE_ARRAY || !base.slot->as.array) {
            runtime_error(rt, expr->line, "indexing requires array value");
            return loc;
        }
        Value idx = eval_expr(rt, expr->as.index_access.index);
        if (idx.type == TYPE_ERROR) return loc;
        long long index = as_int(idx);
        if (index < 0 || (size_t)index >= base.slot->as.array->length) {
            runtime_error(rt, expr->line, "array index out of bounds");
            return loc;
        }
        loc.slot = &base.slot->as.array->items[index];
        loc.type = base.slot->as.array->element_type;
        loc.is_const = base.is_const;
        loc.ok = 1;
        return loc;
    }
    runtime_error(rt, expr->line, "invalid assignment target");
    return loc;
}

static Value eval_call(Runtime *rt, ASTNode *expr)
{
    if (strcmp(expr->as.call.name, "Bind") == 0)
    {
        char buf[128];
        char *end = NULL;
        long long x = 0;
        if (!fgets(buf, sizeof(buf), stdin))
            return value_make_null();
        x = strtoll(buf, &end, 10);
        if (end == buf)
            return value_make_null();
        return value_make_int(x);
    }

    ASTNode *struct_decl = find_struct_decl(rt, expr->as.call.name);
    if (struct_decl) {
        StructValue *record = struct_value_create();
        ASTNode *field = struct_decl->as.struct_decl.fields;
        ASTNode *arg = expr->as.call.args;
        while (field) {
            Value field_value = default_value_for_type(rt, field->as.field_decl.type);
            if (arg) {
                Value raw = eval_expr(rt, arg);
                if (raw.type == TYPE_ERROR) {
                    struct_value_free(record);
                    return value_make_error();
                }
                Value coerced = coerce_value(rt, raw, field->as.field_decl.type, arg->line);
                if (coerced.type == TYPE_ERROR) {
                    value_free(&raw);
                    struct_value_free(record);
                    return value_make_error();
                }
                value_free(&field_value);
                field_value = coerced;
                value_free(&raw);
                arg = arg->next;
            }
            struct_value_add_field(record, field->as.field_decl.name, field->as.field_decl.type, field_value);
            value_free(&field_value);
            field = field->next;
        }
        return value_make_struct(expr->as.call.name, record);
    }

    FunctionEntry *f = find_function(rt, expr->as.call.name);
    if (!f)
    {
        runtime_error(rt, expr->line, "unknown function");
        return value_make_error();
    }
    SymbolTable *scope = symtab_create(rt->global_scope);
    ASTNode *param = f->node->as.function.params;
    ASTNode *arg = expr->as.call.args;
    while (param && arg)
    {
        Value v = eval_expr(rt, arg);
        if (v.type == TYPE_ERROR)
        {
            symtab_destroy(scope);
            return v;
        }
        Value casted = coerce_value(rt, v, param->as.param.type, arg->line);
        value_free(&v);
        if (casted.type == TYPE_ERROR)
        {
            symtab_destroy(scope);
            return value_make_error();
        }
        symtab_define(scope, param->as.param.name, param->as.param.type, 0, casted);
        value_free(&casted);
        arg = arg->next;
        param = param->next;
    }
    push_frame(rt, scope);
    Flow flow = exec_stmt(rt, f->node->as.function.body);
    pop_frame(rt);
    if (flow.tag == FLOW_RETURN)
        return flow.value;
    if (f->node->as.function.return_type->kind == TYPE_VOID)
        return value_make_void();
    return value_make_null();
}

static Value eval_unary(Runtime *rt, ASTNode *expr)
{
    ASTNode *opnd = expr->as.unary.operand;
    if (expr->as.unary.op == OP_PRE_INC || expr->as.unary.op == OP_PRE_DEC ||
        expr->as.unary.op == OP_POST_INC || expr->as.unary.op == OP_POST_DEC)
    {
        Location loc = resolve_location(rt, opnd);
        if (!loc.ok || loc.is_const || !loc.slot || (loc.slot->type != TYPE_INT && loc.slot->type != TYPE_FLOAT &&
            loc.slot->type != TYPE_DOUBLE && loc.slot->type != TYPE_CHAR && loc.slot->type != TYPE_BOOL))
        {
            runtime_error(rt, expr->line, "increment/decrement target must be a numeric variable");
            return value_make_error();
        }
        Value oldv = value_copy(*loc.slot);
        int delta = (expr->as.unary.op == OP_PRE_INC || expr->as.unary.op == OP_POST_INC) ? 1 : -1;
        if (loc.slot->type == TYPE_INT)
            loc.slot->as.i += delta;
        else if (loc.slot->type == TYPE_FLOAT || loc.slot->type == TYPE_DOUBLE)
            loc.slot->as.d += (double)delta;
        else if (loc.slot->type == TYPE_CHAR)
            loc.slot->as.c = (char)(loc.slot->as.c + delta);
        else
            loc.slot->as.b = !!(loc.slot->as.b + delta);
        if (expr->as.unary.op == OP_PRE_INC || expr->as.unary.op == OP_PRE_DEC)
            return value_copy(*loc.slot);
        return oldv;
    }
    Value v = eval_expr(rt, opnd);
    if (v.type == TYPE_ERROR)
        return v;
    switch (expr->as.unary.op)
    {
    case OP_NEG:
        return (v.type == TYPE_INT) ? value_make_int(-v.as.i) : value_make_double(-as_double(v));
    case OP_NOT:
        return value_make_bool(!value_is_truthy(v));
    case OP_BITNOT:
        return value_make_int(~as_int(v));
    default:
        return value_make_error();
    }
}

static Value eval_binary(Runtime *rt, ASTNode *expr)
{
    if (expr->as.binary.op == OP_COALESCE)
    {
        Value l = eval_expr(rt, expr->as.binary.left);
        if (l.type != TYPE_NULL)
            return l;
        return eval_expr(rt, expr->as.binary.right);
    }
    Value l = eval_expr(rt, expr->as.binary.left);
    Value r = eval_expr(rt, expr->as.binary.right);
    if (l.type == TYPE_ERROR || r.type == TYPE_ERROR)
        return value_make_error();
    switch (expr->as.binary.op)
    {
    case OP_ADD:
        if (l.type == TYPE_STRING && r.type == TYPE_STRING)
        {
            char buf[1024];
            snprintf(buf, sizeof(buf), "%s%s", l.as.s ? l.as.s : "", r.as.s ? r.as.s : "");
            return value_make_string(buf);
        }
        if (l.type == TYPE_STRING || r.type == TYPE_STRING)
        {
            runtime_error(rt, expr->line, "string concatenation requires two strings");
            return value_make_error();
        }
        return (l.type == TYPE_INT && r.type == TYPE_INT) ? value_make_int(l.as.i + r.as.i) : value_make_double(as_double(l) + as_double(r));
    case OP_SUB:
        return (l.type == TYPE_INT && r.type == TYPE_INT) ? value_make_int(l.as.i - r.as.i) : value_make_double(as_double(l) - as_double(r));
    case OP_MUL:
        return (l.type == TYPE_INT && r.type == TYPE_INT) ? value_make_int(l.as.i * r.as.i) : value_make_double(as_double(l) * as_double(r));
    case OP_DIV:
        if (is_zero_value(r))
        {
            runtime_error(rt, expr->line, "division by zero");
            return value_make_error();
        }
        if (l.type == TYPE_INT && r.type == TYPE_INT) return value_make_int(l.as.i / r.as.i);
        return value_make_double(as_double(l) / as_double(r));
    case OP_MOD:
        if (is_zero_value(r))
        {
            runtime_error(rt, expr->line, "modulo by zero");
            return value_make_error();
        }
        return value_make_int(as_int(l) % as_int(r));
    case OP_LT: return value_make_bool(as_double(l) < as_double(r));
    case OP_GT: return value_make_bool(as_double(l) > as_double(r));
    case OP_LE: return value_make_bool(as_double(l) <= as_double(r));
    case OP_GE: return value_make_bool(as_double(l) >= as_double(r));
    case OP_EQ:
        if (l.type == TYPE_NULL || r.type == TYPE_NULL) return value_make_bool(l.type == r.type);
        if (l.type == TYPE_STRING && r.type == TYPE_STRING) return value_make_bool(strcmp(l.as.s, r.as.s) == 0);
        return value_make_bool(as_double(l) == as_double(r));
    case OP_NE:
        if (l.type == TYPE_NULL || r.type == TYPE_NULL) return value_make_bool(l.type != r.type);
        if (l.type == TYPE_STRING && r.type == TYPE_STRING) return value_make_bool(strcmp(l.as.s, r.as.s) != 0);
        return value_make_bool(as_double(l) != as_double(r));
    case OP_AND: return value_make_bool(value_is_truthy(l) && value_is_truthy(r));
    case OP_OR: return value_make_bool(value_is_truthy(l) || value_is_truthy(r));
    case OP_BITAND: return value_make_int(as_int(l) & as_int(r));
    case OP_BITOR: return value_make_int(as_int(l) | as_int(r));
    case OP_BITXOR: return value_make_int(as_int(l) ^ as_int(r));
    case OP_SHL: return value_make_int(as_int(l) << as_int(r));
    case OP_SHR: return value_make_int(as_int(l) >> as_int(r));
    default:
        return value_make_error();
    }
}

static Value eval_expr(Runtime *rt, ASTNode *expr)
{
    if (!expr)
        return value_make_void();
    switch (expr->kind)
    {
    case NODE_LITERAL:
        return value_copy(expr->as.literal.value);
    case NODE_IDENTIFIER:
    case NODE_FIELD_ACCESS:
    case NODE_INDEX_ACCESS: {
        Location loc = resolve_location(rt, expr);
        if (!loc.ok) return value_make_error();
        return value_copy(*loc.slot);
    }
    case NODE_ARRAY_LITERAL: {
        ASTNode *el = expr->as.array_literal.elements;
        if (!el) return value_make_error();
        Value first = eval_expr(rt, el);
        if (first.type == TYPE_ERROR) return first;
        TypeRef *elem_type = type_ref_from_value(&first);
        int count = 1;
        for (ASTNode *cur = el->next; cur; cur = cur->next) count++;
        Value arr = value_make_array(elem_type, (size_t)count);
        arr.as.array->items[0] = value_copy(first);
        value_free(&first);
        int idx = 1;
        for (ASTNode *cur = el->next; cur; cur = cur->next, ++idx) {
            Value raw = eval_expr(rt, cur);
            Value coerced = coerce_value(rt, raw, elem_type, cur->line);
            value_free(&raw);
            if (coerced.type == TYPE_ERROR) {
                value_free(&arr);
                type_ref_free(elem_type);
                return value_make_error();
            }
            arr.as.array->items[idx] = coerced;
        }
        type_ref_free(elem_type);
        return arr;
    }
    case NODE_CALL:
        return eval_call(rt, expr);
    case NODE_UNARY:
        return eval_unary(rt, expr);
    case NODE_BINARY:
        return eval_binary(rt, expr);
    default:
        return value_make_error();
    }
}

static Flow flow_normal(void)
{
    Flow f = {FLOW_NORMAL, value_make_void()};
    return f;
}
static Flow flow_error(void)
{
    Flow f = {FLOW_ERROR, value_make_error()};
    return f;
}
static Flow flow_return(Value v)
{
    Flow f = {FLOW_RETURN, v};
    return f;
}

static Flow exec_block(Runtime *rt, ASTNode *block)
{
    SymbolTable *scope = symtab_create(rt->frame ? rt->frame->scope : rt->global_scope);
    push_frame(rt, scope);
    for (ASTNode *stmt = block->as.block.statements; stmt; stmt = stmt->next)
    {
        Flow f = exec_stmt(rt, stmt);
        if (f.tag != FLOW_NORMAL)
        {
            pop_frame(rt);
            return f;
        }
    }
    pop_frame(rt);
    return flow_normal();
}

static Flow exec_stmt(Runtime *rt, ASTNode *stmt)
{
    if (!stmt)
        return flow_normal();
    switch (stmt->kind)
    {
    case NODE_BLOCK:
        return exec_block(rt, stmt);
    case NODE_VAR_DECL:
    {
        Value initial = default_value_for_type(rt, stmt->as.var_decl.declared_type);
        if (stmt->as.var_decl.initializer)
        {
            Value raw = eval_expr(rt, stmt->as.var_decl.initializer);
            if (raw.type == TYPE_ERROR) return flow_error();
            value_free(&initial);
            initial = coerce_value(rt, raw, stmt->as.var_decl.declared_type, stmt->line);
            value_free(&raw);
            if (initial.type == TYPE_ERROR) return flow_error();
        }
        if (!symtab_define(rt->frame->scope, stmt->as.var_decl.name, stmt->as.var_decl.declared_type, stmt->as.var_decl.is_const, initial))
        {
            runtime_error(rt, stmt->line, "duplicate declaration");
            value_free(&initial);
            return flow_error();
        }
        value_free(&initial);
        return flow_normal();
    }
    case NODE_ASSIGN:
    {
        Location loc = resolve_location(rt, stmt->as.assign.target);
        if (!loc.ok || loc.is_const) {
            runtime_error(rt, stmt->line, "invalid assignment");
            return flow_error();
        }
        Value raw = eval_expr(rt, stmt->as.assign.value);
        if (raw.type == TYPE_ERROR) return flow_error();
        Value coerced = coerce_value(rt, raw, loc.type, stmt->line);
        value_free(&raw);
        if (coerced.type == TYPE_ERROR) return flow_error();
        value_free(loc.slot);
        *loc.slot = value_copy(coerced);
        value_free(&coerced);
        return flow_normal();
    }
    case NODE_IF:
    {
        Value cond = eval_expr(rt, stmt->as.if_stmt.condition);
        if (cond.type == TYPE_ERROR)
            return flow_error();
        if (value_is_truthy(cond))
            return exec_stmt(rt, stmt->as.if_stmt.then_branch);
        if (stmt->as.if_stmt.else_branch)
            return exec_stmt(rt, stmt->as.if_stmt.else_branch);
        return flow_normal();
    }
    case NODE_WHILE:
        while (value_is_truthy(eval_expr(rt, stmt->as.while_stmt.condition)))
        {
            Flow f = exec_stmt(rt, stmt->as.while_stmt.body);
            if (f.tag == FLOW_RETURN || f.tag == FLOW_ERROR)
                return f;
            if (f.tag == FLOW_BREAK)
                break;
            if (f.tag == FLOW_CONTINUE)
                continue;
        }
        return flow_normal();
    case NODE_DO_WHILE:
        do
        {
            Flow f = exec_stmt(rt, stmt->as.do_while_stmt.body);
            if (f.tag == FLOW_RETURN || f.tag == FLOW_ERROR)
                return f;
            if (f.tag == FLOW_BREAK)
                break;
            if (f.tag == FLOW_CONTINUE)
                continue;
        } while (value_is_truthy(eval_expr(rt, stmt->as.do_while_stmt.condition)));
        return flow_normal();
    case NODE_FOR:
        if (stmt->as.for_stmt.init)
        {
            if (stmt->as.for_stmt.init->kind == NODE_VAR_DECL || stmt->as.for_stmt.init->kind == NODE_ASSIGN)
            {
                Flow initf = exec_stmt(rt, stmt->as.for_stmt.init);
                if (initf.tag != FLOW_NORMAL)
                    return initf;
            }
            else if (eval_expr(rt, stmt->as.for_stmt.init).type == TYPE_ERROR)
                return flow_error();
        }
        while (!stmt->as.for_stmt.condition || value_is_truthy(eval_expr(rt, stmt->as.for_stmt.condition)))
        {
            Flow f = exec_stmt(rt, stmt->as.for_stmt.body);
            if (f.tag == FLOW_RETURN || f.tag == FLOW_ERROR)
                return f;
            if (f.tag == FLOW_BREAK)
                break;
            if (stmt->as.for_stmt.update)
            {
                if (stmt->as.for_stmt.update->kind == NODE_ASSIGN)
                {
                    Flow uf = exec_stmt(rt, stmt->as.for_stmt.update);
                    if (uf.tag == FLOW_ERROR)
                        return uf;
                }
                else if (eval_expr(rt, stmt->as.for_stmt.update).type == TYPE_ERROR)
                {
                    return flow_error();
                }
            }
            if (f.tag == FLOW_CONTINUE)
                continue;
        }
        return flow_normal();
    case NODE_RETURN:
    {
        Value v = stmt->as.return_stmt.expr ? eval_expr(rt, stmt->as.return_stmt.expr) : value_make_void();
        return flow_return(v);
    }
    case NODE_BREAK:
    {
        Flow f = {FLOW_BREAK, value_make_void()};
        return f;
    }
    case NODE_CONTINUE:
    {
        Flow f = {FLOW_CONTINUE, value_make_void()};
        return f;
    }
    case NODE_DEFER:
        register_defer(rt, stmt->as.defer_stmt.stmt);
        return flow_normal();
    case NODE_REVEAL:
    {
        Value v = eval_expr(rt, stmt->as.reveal_stmt.expr);
        if (v.type == TYPE_ERROR)
            return flow_error();
        value_print(rt->out, v);
        fprintf(rt->out, "\n");
        value_free(&v);
        return flow_normal();
    }
    case NODE_EXPR_STMT: {
        Value v = eval_expr(rt, stmt->as.expr_stmt.expr);
        if (v.type == TYPE_ERROR) return flow_error();
        value_free(&v);
        return flow_normal();
    }
    default:
        return flow_error();
    }
}

int interpret_program(ASTNode *program, FILE *out, FILE *err)
{
    Runtime rt = {0};
    rt.program = program;
    rt.out = out;
    rt.err = err;

    for (ASTNode *item = program->as.program.items; item; item = item->next)
    {
        if (item->kind != NODE_FUNCTION) continue;
        FunctionEntry *e = calloc(1, sizeof(FunctionEntry));
        e->name = xstrdup(item->as.function.name);
        e->node = item;
        e->next = rt.functions;
        rt.functions = e;
    }

    rt.global_scope = symtab_create(NULL);
    for (ASTNode *item = program->as.program.items; item; item = item->next) {
        if (item->kind != NODE_ENUM_DECL) continue;
        for (ASTNode *member = item->as.enum_decl.members; member; member = member->next) {
            Value v = value_make_int(member->as.enum_member.value);
            symtab_define(rt.global_scope, member->as.enum_member.name, type_ref_make_primitive(TYPE_INT), 1, v);
            value_free(&v);
        }
    }

    push_frame(&rt, rt.global_scope);
    FunctionEntry *main_fn = find_function(&rt, "main");
    if (!main_fn)
    {
        runtime_error(&rt, 0, "main function not found");
        return 1;
    }
    Flow f = exec_stmt(&rt, main_fn->node->as.function.body);
    pop_frame(&rt);
    while (rt.functions)
    {
        FunctionEntry *n = rt.functions->next;
        free(rt.functions->name);
        free(rt.functions);
        rt.functions = n;
    }
    return f.tag == FLOW_ERROR ? 1 : 0;
}
