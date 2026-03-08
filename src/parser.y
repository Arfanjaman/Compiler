%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

extern int yylex(void);
extern int line_num;
void yyerror(const char *msg);
ASTNode *root = NULL;

static ASTNode *make_bool_node(int b, int line) {
    return ast_make_literal(value_make_bool(b), line);
}

static long long enum_member_count(ASTNode *members) {
    long long count = 0;
    for (; members; members = members->next) count++;
    return count;
}
%}

%code requires {
    #include "ast.h"
}

%union {
    long long ival;
    double dval;
    char cval;
    char *sval;
    ASTNode *node;
    TypeRef *type_ref;
}

%token SOULCAST MANIFEST CONCEAL INTENT OTHERWISE ASLONG RUN CYCLE SHATTER PERSIST ASCEND REVEAL BIND DEFER
%token METAL ESSENCE DOUBLEESSENCE GLYPH OATH ABYSS SCROLL UNDEAD NULLKW
%token SURGE ORDER SPREN
%token BOOL_TRUE BOOL_FALSE
%token <ival> INT_LITERAL
%token <dval> FLOAT_LITERAL
%token <cval> CHAR_LITERAL
%token <sval> STRING_LITERAL IDENTIFIER
%token EQ NE LE GE AND OR SHL SHR COALESCE INCREMENT DECREMENT INVALID_TOKEN

%type <node> program top_list top_item function struct_decl enum_decl alias_decl
%type <node> struct_field_list_opt struct_field_list struct_field enum_member_list_opt enum_member_list
%type <node> param_list_opt param_list param block stmt_list stmt declaration declaration_core assign_stmt assign_core
%type <node> if_stmt while_stmt do_while_stmt for_stmt return_stmt reveal_stmt bind_stmt defer_stmt defer_target expr_stmt
%type <node> expression lvalue primary arg_list_opt arg_list for_init_opt for_update_opt expression_opt array_literal
%type <type_ref> type_spec base_type

%right '='
%right COALESCE
%left OR
%left AND
%left '|'
%left '^'
%left '&'
%left EQ NE
%left '<' '>' LE GE
%left SHL SHR
%left '+' '-'
%left '*' '/' '%'
%right UMINUS '!' '~'
%right INCREMENT DECREMENT
%nonassoc LOWER_THAN_ELSE
%nonassoc OTHERWISE

%%

program
    : top_list { root = ast_make_program($1, line_num); }
    ;

top_list
    : top_list top_item { $$ = ast_append($1, $2); }
    | top_item { $$ = $1; }
    ;

top_item
    : function { $$ = $1; }
    | struct_decl { $$ = $1; }
    | enum_decl { $$ = $1; }
    | alias_decl { $$ = $1; }
    ;

function
    : SOULCAST type_spec IDENTIFIER '(' param_list_opt ')' block
      { $$ = ast_make_function($3, $2, $5, $7, line_num); free($3); }
    ;

struct_decl
    : SURGE IDENTIFIER MANIFEST struct_field_list_opt CONCEAL
      { $$ = ast_make_struct_decl($2, $4, line_num); free($2); }
    ;

struct_field_list_opt
    : struct_field_list { $$ = $1; }
    | /* empty */ { $$ = NULL; }
    ;

struct_field_list
    : struct_field_list struct_field ';' { $$ = ast_append($1, $2); }
    | struct_field ';' { $$ = $1; }
    ;

struct_field
    : type_spec IDENTIFIER { $$ = ast_make_field_decl($2, $1, line_num); free($2); }
    ;

enum_decl
    : ORDER IDENTIFIER MANIFEST enum_member_list_opt CONCEAL
      { $$ = ast_make_enum_decl($2, $4, line_num); free($2); }
    ;

enum_member_list_opt
    : enum_member_list { $$ = $1; }
    | /* empty */ { $$ = NULL; }
    ;

enum_member_list
    : enum_member_list ',' IDENTIFIER
      { $$ = ast_append($1, ast_make_enum_member($3, enum_member_count($1), line_num)); free($3); }
    | IDENTIFIER
      { $$ = ast_make_enum_member($1, 0, line_num); free($1); }
    ;

alias_decl
    : SPREN type_spec IDENTIFIER ';'
      { $$ = ast_make_type_alias($3, $2, line_num); free($3); }
    ;

param_list_opt
    : param_list { $$ = $1; }
    | /* empty */ { $$ = NULL; }
    ;

param_list
    : param_list ',' param { $$ = ast_append($1, $3); }
    | param { $$ = $1; }
    ;

param
    : type_spec IDENTIFIER { $$ = ast_make_param($2, $1, line_num); free($2); }
    ;

block
    : MANIFEST stmt_list CONCEAL { $$ = ast_make_block($2, line_num); }
    ;

stmt_list
    : stmt_list stmt { $$ = ast_append($1, $2); }
    | /* empty */ { $$ = NULL; }
    ;

stmt
    : declaration ';' { $$ = $1; }
    | assign_stmt ';' { $$ = $1; }
    | bind_stmt ';' { $$ = $1; }
    | expr_stmt ';' { $$ = $1; }
    | reveal_stmt ';' { $$ = $1; }
    | return_stmt ';' { $$ = $1; }
    | defer_stmt ';' { $$ = $1; }
    | if_stmt { $$ = $1; }
    | while_stmt { $$ = $1; }
    | do_while_stmt { $$ = $1; }
    | for_stmt { $$ = $1; }
    | block { $$ = $1; }
    | SHATTER ';' { $$ = ast_make_break(line_num); }
    | PERSIST ';' { $$ = ast_make_continue(line_num); }
    ;

declaration
    : declaration_core { $$ = $1; }
    ;

declaration_core
    : type_spec IDENTIFIER
      { $$ = ast_make_var_decl($2, $1, 0, NULL, line_num); free($2); }
    | type_spec IDENTIFIER '=' expression
      { $$ = ast_make_var_decl($2, $1, 0, $4, line_num); free($2); }
    | UNDEAD type_spec IDENTIFIER '=' expression
      { $$ = ast_make_var_decl($3, $2, 1, $5, line_num); free($3); }
    ;

assign_stmt
    : assign_core { $$ = $1; }
    ;

assign_core
    : lvalue '=' expression
      { $$ = ast_make_assign($1, $3, line_num); }
    ;

bind_stmt
    : BIND lvalue
      { $$ = ast_make_assign($2, ast_make_call("Bind", NULL, line_num), line_num); }
    ;

expr_stmt
    : expression { $$ = ast_make_expr_stmt($1, line_num); }
    ;

reveal_stmt
    : REVEAL '(' expression ')' { $$ = ast_make_reveal($3, line_num); }
    ;

return_stmt
    : ASCEND expression_opt { $$ = ast_make_return($2, line_num); }
    ;

defer_stmt
    : DEFER defer_target { $$ = ast_make_defer($2, line_num); }
    ;

defer_target
    : block { $$ = $1; }
    | reveal_stmt { $$ = $1; }
    | assign_stmt { $$ = $1; }
    | bind_stmt { $$ = $1; }
    | expr_stmt { $$ = $1; }
    ;

if_stmt
    : INTENT '(' expression ')' stmt %prec LOWER_THAN_ELSE
      { $$ = ast_make_if($3, $5, NULL, line_num); }
    | INTENT '(' expression ')' stmt OTHERWISE stmt
      { $$ = ast_make_if($3, $5, $7, line_num); }
    ;

while_stmt
    : ASLONG '(' expression ')' stmt
      { $$ = ast_make_while($3, $5, line_num); }
    ;

do_while_stmt
    : RUN stmt ASLONG '(' expression ')' ';'
      { $$ = ast_make_do_while($2, $5, line_num); }
    ;

for_stmt
    : CYCLE '(' for_init_opt ';' expression_opt ';' for_update_opt ')' stmt
      { $$ = ast_make_for($3, $5, $7, $9, line_num); }
    ;

for_init_opt
    : declaration_core { $$ = $1; }
    | assign_core { $$ = $1; }
    | expression { $$ = $1; }
    | /* empty */ { $$ = NULL; }
    ;

for_update_opt
    : assign_core { $$ = $1; }
    | expression { $$ = $1; }
    | /* empty */ { $$ = NULL; }
    ;

expression_opt
    : expression { $$ = $1; }
    | /* empty */ { $$ = NULL; }
    ;

lvalue
    : IDENTIFIER { $$ = ast_make_identifier($1, line_num); free($1); }
    | lvalue '.' IDENTIFIER { $$ = ast_make_field_access($1, $3, line_num); free($3); }
    | lvalue '[' expression ']' { $$ = ast_make_index_access($1, $3, line_num); }
    ;

primary
    : IDENTIFIER '(' arg_list_opt ')' { $$ = ast_make_call($1, $3, line_num); free($1); }
    | BIND '(' ')' { $$ = ast_make_call("Bind", NULL, line_num); }
    | lvalue { $$ = $1; }
    | INT_LITERAL { $$ = ast_make_literal(value_make_int($1), line_num); }
    | FLOAT_LITERAL { $$ = ast_make_literal(value_make_float($1), line_num); }
    | CHAR_LITERAL { $$ = ast_make_literal(value_make_char($1), line_num); }
    | STRING_LITERAL { $$ = ast_make_literal(value_make_string($1), line_num); free($1); }
    | BOOL_TRUE { $$ = make_bool_node(1, line_num); }
    | BOOL_FALSE { $$ = make_bool_node(0, line_num); }
    | NULLKW { $$ = ast_make_literal(value_make_null(), line_num); }
    | array_literal { $$ = $1; }
    | '(' expression ')' { $$ = $2; }
    ;

array_literal
    : '[' arg_list_opt ']' { $$ = ast_make_array_literal($2, line_num); }
    ;

expression
    : expression '+' expression { $$ = ast_make_binary(OP_ADD, $1, $3, line_num); }
    | expression '-' expression { $$ = ast_make_binary(OP_SUB, $1, $3, line_num); }
    | expression '*' expression { $$ = ast_make_binary(OP_MUL, $1, $3, line_num); }
    | expression '/' expression { $$ = ast_make_binary(OP_DIV, $1, $3, line_num); }
    | expression '%' expression { $$ = ast_make_binary(OP_MOD, $1, $3, line_num); }
    | expression '<' expression { $$ = ast_make_binary(OP_LT, $1, $3, line_num); }
    | expression '>' expression { $$ = ast_make_binary(OP_GT, $1, $3, line_num); }
    | expression LE expression { $$ = ast_make_binary(OP_LE, $1, $3, line_num); }
    | expression GE expression { $$ = ast_make_binary(OP_GE, $1, $3, line_num); }
    | expression EQ expression { $$ = ast_make_binary(OP_EQ, $1, $3, line_num); }
    | expression NE expression { $$ = ast_make_binary(OP_NE, $1, $3, line_num); }
    | expression AND expression { $$ = ast_make_binary(OP_AND, $1, $3, line_num); }
    | expression OR expression { $$ = ast_make_binary(OP_OR, $1, $3, line_num); }
    | expression '&' expression { $$ = ast_make_binary(OP_BITAND, $1, $3, line_num); }
    | expression '|' expression { $$ = ast_make_binary(OP_BITOR, $1, $3, line_num); }
    | expression '^' expression { $$ = ast_make_binary(OP_BITXOR, $1, $3, line_num); }
    | expression SHL expression { $$ = ast_make_binary(OP_SHL, $1, $3, line_num); }
    | expression SHR expression { $$ = ast_make_binary(OP_SHR, $1, $3, line_num); }
    | expression COALESCE expression { $$ = ast_make_binary(OP_COALESCE, $1, $3, line_num); }
    | '-' expression %prec UMINUS { $$ = ast_make_unary(OP_NEG, $2, line_num); }
    | '!' expression { $$ = ast_make_unary(OP_NOT, $2, line_num); }
    | '~' expression { $$ = ast_make_unary(OP_BITNOT, $2, line_num); }
    | INCREMENT lvalue { $$ = ast_make_unary(OP_PRE_INC, $2, line_num); }
    | DECREMENT lvalue { $$ = ast_make_unary(OP_PRE_DEC, $2, line_num); }
    | lvalue INCREMENT { $$ = ast_make_unary(OP_POST_INC, $1, line_num); }
    | lvalue DECREMENT { $$ = ast_make_unary(OP_POST_DEC, $1, line_num); }
    | primary { $$ = $1; }
    ;

arg_list_opt
    : arg_list { $$ = $1; }
    | /* empty */ { $$ = NULL; }
    ;

arg_list
    : arg_list ',' expression { $$ = ast_append($1, $3); }
    | expression { $$ = $1; }
    ;

type_spec
    : base_type { $$ = $1; }
    | type_spec '[' ']' { $$ = type_ref_make_array($1); }
    ;

base_type
    : METAL { $$ = type_ref_make_primitive(TYPE_INT); }
    | ESSENCE { $$ = type_ref_make_primitive(TYPE_FLOAT); }
    | DOUBLEESSENCE { $$ = type_ref_make_primitive(TYPE_DOUBLE); }
    | GLYPH { $$ = type_ref_make_primitive(TYPE_CHAR); }
    | OATH { $$ = type_ref_make_primitive(TYPE_BOOL); }
    | ABYSS { $$ = type_ref_make_primitive(TYPE_VOID); }
    | SCROLL { $$ = type_ref_make_primitive(TYPE_STRING); }
    | IDENTIFIER { $$ = type_ref_make_named($1); free($1); }
    ;

%%

void yyerror(const char *msg) {
    fprintf(stderr, "Syntax error at line %d: %s\n", line_num, msg);
}
