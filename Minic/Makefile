CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2
SOURCES = src/common.c src/common.h src/scanner/scanner.c src/parser/parser.c
MODULES = src/common.c src/scanner/scanner.c src/parser/parser.c

.PHONY: all test scanner clean
all: parser minic

parser: parser.c $(SOURCES)
	$(CC) $(CFLAGS) -DMINIC_SEPARATE_COMPILATION parser.c $(MODULES) -o parser

minic: minic.c $(SOURCES) src/semantico/analisador_semantico.c
	$(CC) $(CFLAGS) -DMINIC_SEPARATE_COMPILATION minic.c $(MODULES) src/semantico/analisador_semantico.c -o minic

test: all
	python3 tests/validar_compilador.py

scanner:
	python3 tools/gerar_scanner.py

clean:
	rm -f parser minic
	rm -rf build
