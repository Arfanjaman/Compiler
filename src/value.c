#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "value.h"

static char *xstrdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (!p) {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }
    memcpy(p, s, n);
    return p;
}

TypeRef *type_ref_make(ValueType kind, const char *name, TypeRef *element_type) {
    TypeRef *type = (TypeRef *)calloc(1, sizeof(TypeRef));
    type->kind = kind;
    type->name = name ? xstrdup(name) : NULL;
    type->element_type = element_type;
    return type;
}

TypeRef *type_ref_make_primitive(ValueType kind) {
    return type_ref_make(kind, NULL, NULL);
}

TypeRef *type_ref_make_named(const char *name) {
    return type_ref_make(TYPE_NAMED, name, NULL);
}

TypeRef *type_ref_make_array(TypeRef *element_type) {
    return type_ref_make(TYPE_ARRAY, NULL, element_type);
}

TypeRef *type_ref_make_struct(const char *name) {
    return type_ref_make(TYPE_STRUCT, name, NULL);
}

TypeRef *type_ref_copy(const TypeRef *type) {
    if (!type) return NULL;
    return type_ref_make(type->kind, type->name, type_ref_copy(type->element_type));
}

void type_ref_free(TypeRef *type) {
    if (!type) return;
    free(type->name);
    type_ref_free(type->element_type);
    free(type);
}

int type_ref_equal(const TypeRef *lhs, const TypeRef *rhs) {
    if (lhs == rhs) return 1;
    if (!lhs || !rhs) return 0;
    if (lhs->kind != rhs->kind) return 0;
    switch (lhs->kind) {
        case TYPE_STRUCT:
        case TYPE_NAMED:
            return lhs->name && rhs->name && strcmp(lhs->name, rhs->name) == 0;
        case TYPE_ARRAY:
            return type_ref_equal(lhs->element_type, rhs->element_type);
        default:
            return 1;
    }
}

int type_ref_is_numeric(const TypeRef *type) {
    return type && (type->kind == TYPE_INT || type->kind == TYPE_FLOAT || type->kind == TYPE_DOUBLE);
}

int type_ref_is_integral(const TypeRef *type) {
    return type && (type->kind == TYPE_INT || type->kind == TYPE_CHAR || type->kind == TYPE_BOOL);
}

int type_ref_assignable(const TypeRef *dst, const TypeRef *src) {
    if (!dst || !src) return 0;
    if (type_ref_equal(dst, src)) return 1;

    if (dst->kind == TYPE_DOUBLE) {
        return src->kind == TYPE_INT || src->kind == TYPE_FLOAT || src->kind == TYPE_DOUBLE ||
               src->kind == TYPE_CHAR || src->kind == TYPE_BOOL;
    }
    if (dst->kind == TYPE_FLOAT) {
        return src->kind == TYPE_INT || src->kind == TYPE_FLOAT ||
               src->kind == TYPE_CHAR || src->kind == TYPE_BOOL;
    }
    if (dst->kind == TYPE_INT) {
        return src->kind == TYPE_INT || src->kind == TYPE_CHAR || src->kind == TYPE_BOOL;
    }
    if (dst->kind == TYPE_CHAR) {
        return src->kind == TYPE_CHAR || src->kind == TYPE_INT;
    }
    if (dst->kind == TYPE_BOOL) {
        return src->kind == TYPE_BOOL || src->kind == TYPE_INT || src->kind == TYPE_CHAR;
    }
    if (dst->kind == TYPE_STRING) {
        return src->kind == TYPE_STRING || src->kind == TYPE_NULL;
    }
    if (dst->kind == TYPE_ARRAY) {
        return src->kind == TYPE_ARRAY && type_ref_equal(dst->element_type, src->element_type);
    }
    if (dst->kind == TYPE_STRUCT) {
        return src->kind == TYPE_STRUCT && dst->name && src->name && strcmp(dst->name, src->name) == 0;
    }
    if (dst->kind == TYPE_VOID) return src->kind == TYPE_VOID;
    if (dst->kind == TYPE_NULL) return src->kind == TYPE_NULL;
    return 0;
}

void type_ref_print(FILE *out, const TypeRef *type) {
    if (!out) out = stdout;
    if (!type) {
        fprintf(out, "<null-type>");
        return;
    }
    switch (type->kind) {
        case TYPE_INT: fprintf(out, "Metal"); break;
        case TYPE_FLOAT: fprintf(out, "Essence"); break;
        case TYPE_DOUBLE: fprintf(out, "DoubleEssence"); break;
        case TYPE_CHAR: fprintf(out, "Glyph"); break;
        case TYPE_BOOL: fprintf(out, "Oath"); break;
        case TYPE_STRING: fprintf(out, "Scroll"); break;
        case TYPE_VOID: fprintf(out, "Abyss"); break;
        case TYPE_NULL: fprintf(out, "Null"); break;
        case TYPE_STRUCT:
        case TYPE_NAMED:
            fprintf(out, "%s", type->name ? type->name : "<unnamed>");
            break;
        case TYPE_ARRAY:
            type_ref_print(out, type->element_type);
            fprintf(out, "[]");
            break;
        case TYPE_ERROR:
        default:
            fprintf(out, "Error");
            break;
    }
}

const char *value_type_name(ValueType t) {
    switch (t) {
        case TYPE_INT: return "Metal";
        case TYPE_FLOAT: return "Essence";
        case TYPE_DOUBLE: return "DoubleEssence";
        case TYPE_CHAR: return "Glyph";
        case TYPE_BOOL: return "Oath";
        case TYPE_STRING: return "Scroll";
        case TYPE_VOID: return "Abyss";
        case TYPE_NULL: return "Null";
        case TYPE_STRUCT: return "Surge";
        case TYPE_ARRAY: return "Array";
        case TYPE_NAMED: return "Named";
        case TYPE_ERROR:
        default:
            return "Error";
    }
}

Value value_make_int(long long x) {
    Value v = (Value){0};
    v.type = TYPE_INT;
    v.as.i = x;
    return v;
}

Value value_make_float(double x) {
    Value v = (Value){0};
    v.type = TYPE_FLOAT;
    v.as.d = x;
    return v;
}

Value value_make_double(double x) {
    Value v = (Value){0};
    v.type = TYPE_DOUBLE;
    v.as.d = x;
    return v;
}

Value value_make_char(char x) {
    Value v = (Value){0};
    v.type = TYPE_CHAR;
    v.as.c = x;
    return v;
}

Value value_make_bool(int x) {
    Value v = (Value){0};
    v.type = TYPE_BOOL;
    v.as.b = !!x;
    return v;
}

Value value_make_string(const char *x) {
    Value v = (Value){0};
    v.type = TYPE_STRING;
    v.as.s = xstrdup(x ? x : "");
    return v;
}

Value value_make_void(void) {
    Value v = (Value){0};
    v.type = TYPE_VOID;
    return v;
}

Value value_make_null(void) {
    Value v = (Value){0};
    v.type = TYPE_NULL;
    return v;
}

Value value_make_struct(const char *type_name, StructValue *record) {
    Value v = (Value){0};
    v.type = TYPE_STRUCT;
    v.type_name = xstrdup(type_name ? type_name : "");
    v.as.record = record;
    return v;
}

Value value_make_array(const TypeRef *element_type, size_t length) {
    Value v = (Value){0};
    v.type = TYPE_ARRAY;
    v.as.array = array_value_create(element_type, length);
    return v;
}

Value value_make_error(void) {
    Value v = (Value){0};
    v.type = TYPE_ERROR;
    return v;
}

StructValue *struct_value_create(void) {
    return (StructValue *)calloc(1, sizeof(StructValue));
}

StructFieldValue *struct_value_find_field(StructValue *record, const char *name) {
    for (StructFieldValue *field = record ? record->fields : NULL; field; field = field->next) {
        if (strcmp(field->name, name) == 0) return field;
    }
    return NULL;
}

const StructFieldValue *struct_value_find_field_const(const StructValue *record, const char *name) {
    for (const StructFieldValue *field = record ? record->fields : NULL; field; field = field->next) {
        if (strcmp(field->name, name) == 0) return field;
    }
    return NULL;
}

void struct_value_add_field(StructValue *record, const char *name, const TypeRef *type, Value value) {
    StructFieldValue *field = (StructFieldValue *)calloc(1, sizeof(StructFieldValue));
    field->name = xstrdup(name);
    field->type = type_ref_copy(type);
    field->value = value_copy(value);
    if (!record->fields) {
        record->fields = field;
        return;
    }
    StructFieldValue *tail = record->fields;
    while (tail->next) tail = tail->next;
    tail->next = field;
}

StructValue *struct_value_copy(const StructValue *record) {
    if (!record) return NULL;
    StructValue *copy = struct_value_create();
    for (const StructFieldValue *field = record->fields; field; field = field->next) {
        struct_value_add_field(copy, field->name, field->type, field->value);
    }
    return copy;
}

void struct_value_free(StructValue *record) {
    if (!record) return;
    StructFieldValue *field = record->fields;
    while (field) {
        StructFieldValue *next = field->next;
        free(field->name);
        type_ref_free(field->type);
        value_free(&field->value);
        free(field);
        field = next;
    }
    free(record);
}

ArrayValue *array_value_create(const TypeRef *element_type, size_t length) {
    ArrayValue *array = (ArrayValue *)calloc(1, sizeof(ArrayValue));
    array->element_type = type_ref_copy(element_type);
    array->length = length;
    if (length > 0) {
        array->items = (Value *)calloc(length, sizeof(Value));
    }
    return array;
}

ArrayValue *array_value_copy(const ArrayValue *array) {
    if (!array) return NULL;
    ArrayValue *copy = array_value_create(array->element_type, array->length);
    for (size_t i = 0; i < array->length; ++i) copy->items[i] = value_copy(array->items[i]);
    return copy;
}

void array_value_free(ArrayValue *array) {
    if (!array) return;
    for (size_t i = 0; i < array->length; ++i) value_free(&array->items[i]);
    free(array->items);
    type_ref_free(array->element_type);
    free(array);
}

Value value_copy(Value v) {
    Value out = (Value){0};
    out.type = v.type;
    out.type_name = v.type_name ? xstrdup(v.type_name) : NULL;
    switch (v.type) {
        case TYPE_STRING:
            out.as.s = xstrdup(v.as.s ? v.as.s : "");
            break;
        case TYPE_INT:
            out.as.i = v.as.i;
            break;
        case TYPE_FLOAT:
        case TYPE_DOUBLE:
            out.as.d = v.as.d;
            break;
        case TYPE_CHAR:
            out.as.c = v.as.c;
            break;
        case TYPE_BOOL:
            out.as.b = v.as.b;
            break;
        case TYPE_STRUCT:
            out.as.record = struct_value_copy(v.as.record);
            break;
        case TYPE_ARRAY:
            out.as.array = array_value_copy(v.as.array);
            break;
        case TYPE_VOID:
        case TYPE_NULL:
        case TYPE_ERROR:
        default:
            break;
    }
    return out;
}

void value_free(Value *v) {
    if (!v) return;
    free(v->type_name);
    v->type_name = NULL;
    if (v->type == TYPE_STRING && v->as.s) {
        free(v->as.s);
        v->as.s = NULL;
    } else if (v->type == TYPE_STRUCT) {
        struct_value_free(v->as.record);
        v->as.record = NULL;
    } else if (v->type == TYPE_ARRAY) {
        array_value_free(v->as.array);
        v->as.array = NULL;
    }
    v->type = TYPE_NULL;
}

static double value_to_double(Value v) {
    switch (v.type) {
        case TYPE_INT: return (double)v.as.i;
        case TYPE_FLOAT:
        case TYPE_DOUBLE: return v.as.d;
        case TYPE_BOOL: return (double)v.as.b;
        case TYPE_CHAR: return (double)v.as.c;
        default: return 0.0;
    }
}

static long long value_to_int(Value v) {
    switch (v.type) {
        case TYPE_INT: return v.as.i;
        case TYPE_FLOAT:
        case TYPE_DOUBLE: return (long long)v.as.d;
        case TYPE_BOOL: return (long long)v.as.b;
        case TYPE_CHAR: return (long long)v.as.c;
        default: return 0;
    }
}

int value_is_truthy(Value v) {
    switch (v.type) {
        case TYPE_BOOL: return v.as.b != 0;
        case TYPE_INT: return v.as.i != 0;
        case TYPE_FLOAT:
        case TYPE_DOUBLE: return v.as.d != 0.0;
        case TYPE_CHAR: return v.as.c != '\0';
        case TYPE_STRING: return v.as.s && v.as.s[0] != '\0';
        case TYPE_STRUCT: return v.as.record != NULL;
        case TYPE_ARRAY: return v.as.array && v.as.array->length != 0;
        case TYPE_NULL:
        case TYPE_VOID:
        case TYPE_ERROR:
        default:
            return 0;
    }
}

Value value_cast(Value v, const TypeRef *dest) {
    if (!dest) return value_make_error();
    if (dest->kind == TYPE_STRUCT || dest->kind == TYPE_ARRAY) {
        if (type_ref_assignable(dest, type_ref_make(v.type, v.type_name, v.type == TYPE_ARRAY && v.as.array ? type_ref_copy(v.as.array->element_type) : NULL))) {
            return value_copy(v);
        }
        return value_make_error();
    }

    if (dest->kind == TYPE_INT) return value_make_int(value_to_int(v));
    if (dest->kind == TYPE_FLOAT) return value_make_float(value_to_double(v));
    if (dest->kind == TYPE_DOUBLE) return value_make_double(value_to_double(v));
    if (dest->kind == TYPE_CHAR) return value_make_char((char)value_to_int(v));
    if (dest->kind == TYPE_BOOL) return value_make_bool(value_is_truthy(v));
    if (dest->kind == TYPE_STRING) {
        if (v.type == TYPE_NULL) return value_make_string("");
        if (v.type == TYPE_STRING) return value_make_string(v.as.s ? v.as.s : "");
        return value_make_error();
    }
    if (dest->kind == TYPE_VOID) return value_make_void();
    if (dest->kind == TYPE_NULL) return value_make_null();
    return value_make_error();
}

void value_print(FILE *out, Value v) {
    if (!out) out = stdout;
    switch (v.type) {
        case TYPE_INT:
            fprintf(out, "%lld", v.as.i);
            break;
        case TYPE_FLOAT:
        case TYPE_DOUBLE:
            fprintf(out, "%g", v.as.d);
            break;
        case TYPE_CHAR:
            fprintf(out, "%c", v.as.c);
            break;
        case TYPE_BOOL:
            fprintf(out, "%s", v.as.b ? "true" : "false");
            break;
        case TYPE_STRING:
            fprintf(out, "%s", v.as.s ? v.as.s : "");
            break;
        case TYPE_NULL:
            fprintf(out, "null");
            break;
        case TYPE_VOID:
            fprintf(out, "<void>");
            break;
        case TYPE_STRUCT: {
            fprintf(out, "%s{", v.type_name ? v.type_name : "Surge");
            const StructFieldValue *field = v.as.record ? v.as.record->fields : NULL;
            while (field) {
                fprintf(out, "%s=", field->name);
                value_print(out, field->value);
                if (field->next) fprintf(out, ", ");
                field = field->next;
            }
            fprintf(out, "}");
            break;
        }
        case TYPE_ARRAY:
            fprintf(out, "[");
            if (v.as.array) {
                for (size_t i = 0; i < v.as.array->length; ++i) {
                    value_print(out, v.as.array->items[i]);
                    if (i + 1 < v.as.array->length) fprintf(out, ", ");
                }
            }
            fprintf(out, "]");
            break;
        case TYPE_ERROR:
        default:
            fprintf(out, "<error>");
            break;
    }
}
