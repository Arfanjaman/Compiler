%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "parser.tab.h"

int line_num = 1;
int lexer_error_count = 0;
static char *xstrdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (!p) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    memcpy(p, s, n);
    return p;
}
static char *unescape_string(const char *src) {
    size_t n = strlen(src);
    char *out = (char *)malloc(n + 1);
    size_t j = 0;
    for (size_t i = 0; i < n; ++i) {
        if (src[i] == '\\' && i + 1 < n) {
            ++i;
            switch (src[i]) {
                case 'n': out[j++] = '\n'; break;
                case 't': out[j++] = '\t'; break;
                case 'r': out[j++] = '\r'; break;
                case '\\': out[j++] = '\\'; break;
                case '"': out[j++] = '"'; break;
                case '\'': out[j++] = '\''; break;
                default: out[j++] = src[i]; break;
            }
        } else {
            out[j++] = src[i];
        }
    }
    out[j] = '\0';
    return out;
}
%}

%x COMMENT

%%

[ \t\r]+                   ;
\n                         { line_num++; }
"//"[^\n]*                 ;
"/*"                       { BEGIN(COMMENT); }
<COMMENT>"*/"              { BEGIN(INITIAL); }
<COMMENT>\n                { line_num++; }
<COMMENT>.                 ;

"SoulCast"                 { return SOULCAST; }
"Manifest"                 { return MANIFEST; }
"Conceal"                  { return CONCEAL; }
"Intent"                   { return INTENT; }
"Otherwise"                { return OTHERWISE; }
"AsLong"                   { return ASLONG; }
"Run"                      { return RUN; }
"Cycle"                    { return CYCLE; }
"Shatter"                  { return SHATTER; }
"Persist"                  { return PERSIST; }
"Ascend"                   { return ASCEND; }
"Reveal"                   { return REVEAL; }
"Bind"                     { return BIND; }
"defer"                    { return DEFER; }
"Undead"                   { return UNDEAD; }
"Metal"                    { return METAL; }
"Essence"                  { return ESSENCE; }
"DoubleEssence"            { return DOUBLEESSENCE; }
"Glyph"                    { return GLYPH; }
"Oath"                     { return OATH; }
"Abyss"                    { return ABYSS; }
"Scroll"                   { return SCROLL; }
"Null"                     { return NULLKW; }
"Surge"                    { return SURGE; }
"Order"                    { return ORDER; }
"spren"                    { return SPREN; }
"true"                     { return BOOL_TRUE; }
"false"                    { return BOOL_FALSE; }
"Together"                 { return AND; }
"Alone"                    { return OR; }
"Not"                      { return '!'; }

"=="                       { return EQ; }
"!="                       { return NE; }
"<="                       { return LE; }
">="                       { return GE; }
"&&"                       { return AND; }
"||"                       { return OR; }
"<<"                       { return SHL; }
">>"                       { return SHR; }
"??"                       { return COALESCE; }
"++"                       { return INCREMENT; }
"--"                       { return DECREMENT; }

[0-9]+\.[0-9]+             { yylval.dval = atof(yytext); return FLOAT_LITERAL; }
[0-9]+                     { yylval.ival = atoll(yytext); return INT_LITERAL; }
\'([^\\\']|\\[ntr\\\'])\'  {
                              char c = yytext[1];
                              if (c == '\\') {
                                  switch (yytext[2]) {
                                      case 'n': c = '\n'; break;
                                      case 't': c = '\t'; break;
                                      case 'r': c = '\r'; break;
                                      case '\\': c = '\\'; break;
                                      case '\'': c = '\''; break;
                                  }
                              }
                              yylval.cval = c;
                              return CHAR_LITERAL;
                            }
\"([^\\\n\"]|\\.)*\"        {
                              size_t len = strlen(yytext);
                              char *raw = (char *)malloc(len - 1);
                              if (!raw) {
                                fprintf(stderr, "Out of memory\n");
                                exit(EXIT_FAILURE);
                              }
                              memcpy(raw, yytext + 1, len - 2);
                              raw[len - 2] = '\0';
                              yylval.sval = unescape_string(raw);
                              free(raw);
                              return STRING_LITERAL;
                            }

[a-zA-Z_][a-zA-Z0-9_]*      { yylval.sval = xstrdup(yytext); return IDENTIFIER; }

[(){}[\],;:+\-*/%<>=!~^&|?.] { return yytext[0]; }

.                           {
                              fprintf(stderr, "Lexical error at line %d: invalid token '%s'\n", line_num, yytext);
                              lexer_error_count++;
                              return INVALID_TOKEN;
                            }

%%

int yywrap(void) { return 1; }
