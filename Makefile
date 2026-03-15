CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude -Isrc
FLEX = flex
BISON = bison

SRC = src/ast.c src/value.c src/symbol_table.c src/semantic.c src/interpreter.c src/debug_output.c src/main.c src/parser.tab.c src/lex.yy.c
OBJ = $(SRC:.c=.o)
TARGET = cosmerelang.exe

all: $(TARGET)

src/parser.tab.c src/parser.tab.h: src/parser.y
	$(BISON) -d -o src/parser.tab.c src/parser.y

src/lex.yy.c: src/lexer.l src/parser.tab.h
	$(FLEX) src/lexer.l
	powershell -NoProfile -Command "Move-Item -Force 'lex.yy.c' 'src\\lex.yy.c'"

$(TARGET): src/parser.tab.c src/lex.yy.c $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	.\$(TARGET) tests/sample.cos

clean:
	powershell -NoProfile -Command "$$paths = @('$(TARGET)','cosmerelang','src/parser.tab.c','src/parser.tab.h','src/lex.yy.c','lex.yy.c'); foreach ($$p in $$paths) { Remove-Item -Force -ErrorAction SilentlyContinue $$p }; Remove-Item -Force -ErrorAction SilentlyContinue 'src/*.o'; Remove-Item -Force -ErrorAction SilentlyContinue '*.o'; exit 0"

.PHONY: all clean run
