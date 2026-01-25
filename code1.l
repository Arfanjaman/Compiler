%option noyywrap yylineno

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *tokout = NULL;
static FILE *cout = NULL;

static void emit(const char *name) {
    fprintf(tokout, "<LINE:%d> <%s>\n", yylineno, name);
}
static void emitv(const char *name, const char *val) {
    fprintf(tokout, "<LINE:%d> <%s:%s>\n", yylineno, name, val);
}
static void cwrite(const char *s) {
    fputs(s, cout);
}
static void ctext(void) {
    fputs(yytext, cout);
}
%}

%x COMMENT
%x STR

DIGIT       [0-9]
LETTER      [A-Za-z_]
ID          {LETTER}({LETTER}|{DIGIT})*
INT         {DIGIT}+
FLOAT       {DIGIT}+"."{DIGIT}+

%%

[ \t\r]+            { ctext(); }
"\r\n"              { ctext(); }
"\n"                { ctext(); }

"//".*              { ctext(); }

"/*"                { BEGIN(COMMENT); ctext(); }
<COMMENT>"*/"       { BEGIN(INITIAL); ctext(); }
<COMMENT>"\r\n"      { ctext();}
<COMMENT>\n         { ctext();}
<COMMENT>.          { ctext();}

\"                  { BEGIN(STR); yymore(); }
<STR>\\[\\\"ntr0]    { yymore(); }
<STR>\"             { BEGIN(INITIAL); emitv("STRING", yytext); cwrite(yytext); }
<STR>\r\n           { BEGIN(INITIAL); emit("LEX_ERROR_UNTERMINATED_STRING"); }
<STR>\n             { BEGIN(INITIAL); emit("LEX_ERROR_UNTERMINATED_STRING"); }
<STR>.              { yymore(); }


"Manifest"          { emit("LBRACE"); cwrite("{"); }
"Conceal"           { emit("RBRACE"); cwrite("}"); }

"Metal"             { emit("TYPE_METAL"); cwrite("int"); }
"Essence"           { emit("TYPE_ESSENCE"); cwrite("float"); }
"DoubleEssence"     { emit("TYPE_DOUBLEESSENCE"); cwrite("double"); }
"Glyph"             { emit("TYPE_GLYPH"); cwrite("char"); }

"Intent"            { emit("IF"); cwrite("if"); }
"Otherwise"         { emit("ELSE"); cwrite("else"); }
"AsLong"            { emit("WHILE"); cwrite("while"); }
"Run"               { emit("DO"); cwrite("do"); }
"Cycle"             { emit("FOR"); cwrite("for"); }
"Shatter"           { emit("BREAK"); cwrite("break"); }
"Persist"           { emit("CONTINUE"); cwrite("continue"); }
"Ascend"            { emit("RETURN"); cwrite("return"); }

"SoulCast"          { emit("FUNCDEF"); }
"Reveal"            { emit("PRINT"); cwrite("printf"); }
"Bind"              { emit("SCAN"); cwrite("scanf"); }

"Together"          { emit("AND"); cwrite("&&"); }
"Alone"             { emit("OR"); cwrite("||"); }
"Not"               { emit("NOT"); cwrite("!"); }

"??"                { emit("NULL_COALESCE"); cwrite("COALESCE"); }

"=="                { emit("EQ"); ctext(); }
"!="                { emit("NE"); ctext(); }
"<="                { emit("LE"); ctext(); }
">="                { emit("GE"); ctext(); }
"++"                { emit("INC"); ctext(); }
"--"                { emit("DEC"); ctext(); }
"<<"                { emit("SHL"); ctext(); }
">>"                { emit("SHR"); ctext(); }

"="                 { emit("ASSIGN"); ctext(); }
"<"                 { emit("LT"); ctext(); }
">"                 { emit("GT"); ctext(); }
"+"                 { emit("PLUS"); ctext(); }
"-"                 { emit("MINUS"); ctext(); }
"*"                 { emit("MUL"); ctext(); }
"/"                 { emit("DIV"); ctext(); }
"%"                 { emit("MOD"); ctext(); }

"("                 { emit("LPAREN"); ctext(); }
")"                 { emit("RPAREN"); ctext(); }
"["                 { emit("LBRACKET"); ctext(); }
"]"                 { emit("RBRACKET"); ctext(); }
"{"                 { emit("LBRACE"); ctext(); }
"}"                 { emit("RBRACE"); ctext(); }
";"                 { emit("SEMI"); ctext(); }
","                 { emit("COMMA"); ctext(); }

{FLOAT}             { emitv("FLOAT", yytext); ctext(); }
{INT}               { emitv("INT", yytext); ctext(); }
\'([^\\\']|\\.)\'    { emitv("CHAR", yytext); ctext(); }

{ID}                { emitv("ID", yytext); ctext(); }

.                   { emitv("UNKNOWN", yytext); ctext(); }

<<EOF>>             { return 0; }

%%

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input.cos> [out.c] [tokens.txt]\n", argv[0]);
        return 1;
    }

    FILE *in = fopen(argv[1], "rb");
    if (!in) { perror("open input"); return 1; }
    yyin = in;

    const char *cname = (argc >= 3) ? argv[2] : "out.c";
    cout = fopen(cname, "wb");
    if (!cout) { perror("open out.c"); fclose(in); return 1; }

    const char *tname = (argc >= 4) ? argv[3] : "tokens.txt";
    tokout = fopen(tname, "wb");
    if (!tokout) { perror("open tokens"); fclose(cout); fclose(in); return 1; }

    fputs("#include <stdio.h>\n", cout);
    fputs("#define COALESCE(a,b) ((a)!=0 ? (a) : (b))\n\n", cout);

    yylex();

    fclose(tokout);
    fclose(cout);
    fclose(in);
    return 0;
}
