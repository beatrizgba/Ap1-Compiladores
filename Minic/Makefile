CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2
SOURCES = src/common.c src/common.h src/scanner/scanner.c src/parser/parser.c
MODULES = src/common.c src/scanner/scanner.c src/parser/parser.c
PARSER_TABLES = src/parser/ascendente.c.inc src/parser/tabelas_lr.h
FINAL_MODULES = src/ir/gerador.c src/ir/atributos.c src/optimizer/otimizador.c src/codegen/maquina.c src/driver.c

.PHONY: all test scanner-fontes parser-tabelas benchmark clean
all: scanner parser minic

scanner: scanner.c src/lexer/lexer.c
	$(CC) $(CFLAGS) scanner.c -o scanner

parser: parser.c $(SOURCES) $(PARSER_TABLES)
	$(CC) $(CFLAGS) -DMINIC_SEPARATE_COMPILATION parser.c $(MODULES) -o parser

minic: minic.c $(SOURCES) $(PARSER_TABLES) src/semantico/analisador_semantico.c $(FINAL_MODULES) src/ir/ir.h
	$(CC) $(CFLAGS) -DMINIC_SEPARATE_COMPILATION minic.c $(MODULES) src/semantico/analisador_semantico.c $(FINAL_MODULES) -o minic

test: all
	python3 tests/validar_compilador.py

scanner-fontes:
	python3 tools/gerar_scanner.py

parser-tabelas:
	python3 tools/gerar_parser_lr.py

benchmark: all
	python3 tools/benchmark.py

clean:
	rm -f scanner parser minic scanner.exe parser.exe minic.exe
	rm -rf build
