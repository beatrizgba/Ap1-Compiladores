/* Entrada da etapa 3: compile com gcc minic.c -o minic. */
#include "src/common.h"
#ifndef MINIC_SEPARATE_COMPILATION
#include "src/common.c"
#include "src/scanner/scanner.c"
#include "src/parser/parser.c"
#include "src/semantico/analisador_semantico.c"
#endif

int main(int argc,char **argv) {
    configure_output();
    if(argc!=2){fputs("Uso: ./minic codigo.c\n",stderr);return 2;}
    Context ctx={0};
    if(!read_source(&ctx,argv[1])){fputs("ERRO ENTRADA: não foi possível ler o arquivo\n",stderr);context_free(&ctx);return 2;}
    int ok=scan_source(&ctx);Node *program=ok?parse_program(&ctx):NULL;
    if(!ok||!program){print_failure(&ctx);context_free(&ctx);return 1;}
    analyze_semantics(&ctx,program);print_semantics(&ctx);
    int status=ctx.error_count?1:0;context_free(&ctx);return status;
}
