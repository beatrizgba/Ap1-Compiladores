/* Entrada da etapa 2: usa exatamente o mesmo scanner e parser da etapa 3. */
#include "src/common.h"
#ifndef MINIC_SEPARATE_COMPILATION
#include "src/common.c"
#include "src/scanner/scanner.c"
#include "src/parser/parser.c"
#endif

int main(int argc,char **argv) {
    if(argc!=2){fputs("Uso: ./parser codigo.c\n",stderr);return 2;}
    Context ctx={0};
    if(!read_source(&ctx,argv[1])){fputs("ERRO ENTRADA: não foi possível ler o arquivo\n",stderr);context_free(&ctx);return 2;}
    int ok=scan_source(&ctx);Node *program=ok?parse_program(&ctx):NULL;
    if(!ok||!program){print_failure(&ctx);context_free(&ctx);return 1;}
    print_ast(program,stdout);fputc('\n',stdout);context_free(&ctx);return 0;
}
