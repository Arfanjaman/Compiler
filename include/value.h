#ifndef COSMERE_VALUE_H
#define COSMERE_VALUE_H

#include <stdio.h>
#include <stddef.h>

typedef enum {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_CHAR,
    TYPE_BOOL,
    TYPE_STRING,
    TYPE_VOID,
    TYPE_NULL,
    TYPE_STRUCT,
    TYPE_ARRAY,
    TYPE_NAMED,
    TYPE_ERROR
} ValueType;

typedef struct TypeRef TypeRef;
typedef struct Value Value;
typedef struct StructFieldValue StructFieldValue;
typedef struct StructValue StructValue;
typedef struct ArrayValue ArrayValue;

struct TypeRef {
    ValueType kind;
    char *name;
    TypeRef *element_type;
};

struct Value {
    ValueType type;
    char *type_name;
    union {
        long long i;
        double d;
        char c;
        int b;
        char *s;
        StructValue *record;
        ArrayValue *array;
    } as;
};

struct StructFieldValue {
    char *name;
    TypeRef *type;
    Value value;
    StructFieldValue *next;
};

struct StructValue {
    StructFieldValue *fields;
};

struct ArrayValue {
    TypeRef *element_type;
    size_t length;
    Value *items;
};

TypeRef *type_ref_make(ValueType kind, const char *name, TypeRef *element_type);
TypeRef *type_ref_make_primitive(ValueType kind);
TypeRef *type_ref_make_named(const char *name);
TypeRef *type_ref_make_array(TypeRef *element_type);
TypeRef *type_ref_make_struct(const char *name);
TypeRef *type_ref_copy(const TypeRef *type);
void type_ref_free(TypeRef *type);
int type_ref_equal(const TypeRef *lhs, const TypeRef *rhs);
int type_ref_assignable(const TypeRef *dst, const TypeRef *src);
int type_ref_is_numeric(const TypeRef *type);
int type_ref_is_integral(const TypeRef *type);
void type_ref_print(FILE *out, const TypeRef *type);
const char *value_type_name(ValueType t);

Value value_make_int(long long x);
Value value_make_float(double x);
Value value_make_double(double x);
Value value_make_char(char x);
Value value_make_bool(int x);
Value value_make_string(const char *x);
Value value_make_void(void);
Value value_make_null(void);
Value value_make_struct(const char *type_name, StructValue *record);
Value value_make_array(const TypeRef *element_type, size_t length);
Value value_make_error(void);
Value value_copy(Value v);
void value_free(Value *v);
int value_is_truthy(Value v);
Value value_cast(Value v, const TypeRef *dest);
void value_print(FILE *out, Value v);

StructValue *struct_value_create(void);
StructFieldValue *struct_value_find_field(StructValue *record, const char *name);
const StructFieldValue *struct_value_find_field_const(const StructValue *record, const char *name);
void struct_value_add_field(StructValue *record, const char *name, const TypeRef *type, Value value);
StructValue *struct_value_copy(const StructValue *record);
void struct_value_free(StructValue *record);

ArrayValue *array_value_create(const TypeRef *element_type, size_t length);
ArrayValue *array_value_copy(const ArrayValue *array);
void array_value_free(ArrayValue *array);

#endif
