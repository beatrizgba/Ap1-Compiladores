/* Entrada da etapa 3: compile com gcc minic.c -o minic. */
#include "src/common.h"
#ifndef MINIC_SEPARATE_COMPILATION
#include "src/common.c"
#include "src/scanner/scanner.c"
#include "src/parser/parser.c"
#include "src/semantico/analisador_semantico.c"
#include "src/ir/gerador.c"
#include "src/ir/atributos.c"
#include "src/optimizer/otimizador.c"
#include "src/codegen/maquina.c"
#include "src/driver.c"
#endif

int minic_main(int argc,char **argv);
int main(int argc,char **argv) {return minic_main(argc,argv);}
