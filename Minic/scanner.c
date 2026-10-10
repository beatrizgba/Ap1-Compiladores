/* Entrada portátil da etapa 1; preserva o scanner original gerado pelo Flex. */
#define _POSIX_C_SOURCE 200809L
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#define main lexer_stage1_main
#include "src/lexer/lexer.c"
#undef main

int main(int argc,char **argv) {
#ifdef _WIN32
    _setmode(_fileno(stdout),_O_BINARY);
    _setmode(_fileno(stderr),_O_BINARY);
#endif
    return lexer_stage1_main(argc,argv);
}
