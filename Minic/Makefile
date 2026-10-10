CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2
SOURCES = src/common.c src/common.h src/scanner/scanner.c src/parser/parser.c
MODULES = src/common.c src/scanner/scanner.c src/parser/parser.c

.PHONY: all test scanner-fontes clean
all: scanner parser minic

scanner: scanner.c src/lexer/lexer.c
	$(CC) $(CFLAGS) scanner.c -o scanner

parser: parser.c $(SOURCES)
	$(CC) $(CFLAGS) -DMINIC_SEPARATE_COMPILATION parser.c $(MODULES) -o parser

minic: minic.c $(SOURCES) src/semantico/analisador_semantico.c
	$(CC) $(CFLAGS) -DMINIC_SEPARATE_COMPILATION minic.c $(MODULES) src/semantico/analisador_semantico.c -o minic

test: all
	python3 tests/validar_compilador.py

scanner-fontes:
	python3 tools/gerar_scanner.py

clean:
	rm -f scanner parser minic scanner.exe parser.exe minic.exe
	rm -rf build
