#include "common.h"
#include "ir/ir.h"
#include <string.h>

static const char *driver_help=
"Uso: minic [opções] arquivo.c\n"
"  --tokens       mostrar tokens\n"
"  --ast          mostrar AST construída pelo parser SLR\n"
"  --symbols      mostrar símbolos e slots dos frames\n"
"  --check        verificar semântica sem gerar código\n"
"  --ir           gerar quádruplas 3AC\n"
"  --target=asm   gerar assembly da MINIC-VM\n"
"  --run          compilar e executar na MINIC-VM\n"
"  --exec         executar arquivo de assembly .mvm\n"
"  --sdd          mostrar atributos e ordem topológica\n"
"  --deps         mostrar grafo de dependências (DOT)\n"
"  --cfg          mostrar grafo de fluxo de controle (DOT)\n"
"  --trace-parser registrar shift/reduce em stderr\n"
"  -O0 / -O1      desativar/ativar otimizações\n"
"  -o arquivo     salvar a saída gerada\n"
"Sem opções: interface compatível com os scripts semânticos da etapa 3.\n";

int minic_main(int argc,char **argv) {
    configure_output();
    if(argc==2&&!strcmp(argv[1],"--help")){fputs(driver_help,stdout);return 0;}
    enum {D_NONE,D_LEGACY,D_TOKENS,D_AST,D_SYMBOLS,D_CHECK,D_IR,D_ASM,D_RUN,D_EXEC,D_SDD,D_DEPS,D_CFG};
    const char *flags[]={"--tokens","--ast","--symbols","--check","--ir","--target=asm","--run","--exec","--sdd","--deps","--cfg"};
    int modes[]={D_TOKENS,D_AST,D_SYMBOLS,D_CHECK,D_IR,D_ASM,D_RUN,D_EXEC,D_SDD,D_DEPS,D_CFG};
    int mode=D_NONE,opt=0,trace=0,legacy=argc==2&&argv[1][0]!='-';const char *path=NULL,*output=NULL;
    for(int k=1;k<argc;k++) {
        const char *arg=argv[k];int recognized=0;
        for(size_t j=0;j<11;j++)if(!strcmp(arg,flags[j])){if(mode)goto usage;mode=modes[j];recognized=1;break;}
        if(recognized)continue;
        if(!strcmp(arg,"-O0")||!strcmp(arg,"-O1"))opt=!strcmp(arg,"-O1");
        else if(!strcmp(arg,"--trace-parser"))trace=1;
        else if(!strcmp(arg,"-o")&&k+1<argc)output=argv[++k];
        else if(*arg=='-'||path)goto usage;
        else path=arg;
    }
    if(!path || (output&&(mode==D_RUN||mode==D_EXEC||mode==D_CHECK)))goto usage;
    if(!mode)mode=legacy?D_LEGACY:D_IR;
    Context ctx={0};ctx.final_mode=!legacy;ctx.trace_parser=trace;
    if(!read_source(&ctx,path)){fputs("ERRO ENTRADA: não foi possível ler o arquivo\n",stderr);context_free(&ctx);return legacy?2:1;}
    IR *ir=NULL;Node *program=NULL;int status=0;
    if(mode==D_EXEC) {
        ir=read_assembly(&ctx);if(!ir||!validate_ir(ir)){fputs("ERRO GERACAO: Assembly MINIC-VM inválido.\n",stderr);status=5;goto done;}
    } else {
        if(!scan_source(&ctx)) {
            if(legacy)print_failure(&ctx);
            else {
                size_t p=source_position(&ctx,ctx.failure_line,ctx.failure_column),length=1;
                while(p+length<ctx.source_length&&((unsigned char)ctx.source[p+length]&0xc0)==0x80)length++;
                fprintf(stderr,"ERRO %s linha %d, coluna %d: %s; símbolo “%s”.\n",ctx.stage,ctx.failure_line,ctx.failure_column,ctx.failure,copy_text(&ctx,ctx.source+p,length));
            }
            status=legacy?1:2;goto done;
        }
        if(mode!=D_TOKENS) {
            program=parse_program(&ctx);if(!program){print_failure(&ctx);status=legacy?1:3;goto done;}
            if(mode!=D_AST) {
                analyze_semantics(&ctx,program);
                if(mode==D_LEGACY||mode==D_CHECK||ctx.error_count) {
                    print_semantics(&ctx);status=ctx.error_count?(legacy?1:4):0;goto done;
                }
                ir=generate_ir(&ctx,program);
                if(!validate_ir(ir)){fputs("ERRO GERACAO: IR inválida.\n",stderr);status=5;goto done;}
                if(opt && mode!=D_SDD&&mode!=D_DEPS&&mode!=D_SYMBOLS) {
                    optimize_ir(ir);if(!validate_ir(ir)){fputs("ERRO GERACAO: IR otimizada inválida.\n",stderr);status=5;goto done;}
                }
            }
        }
    }
    if(mode==D_RUN||mode==D_EXEC){status=mode==D_RUN?execute_ir(ir):execute_assembly(ir);goto done;}
    FILE *out=output?fopen(output,"wb"):stdout;
    if(!out){fputs("ERRO GERACAO: não foi possível salvar a saída.\n",stderr);status=5;goto done;}
    if(mode==D_TOKENS)for(size_t k=0;k<ctx.count;k++){Token t=ctx.tokens[k];fprintf(out,"%s\t%s\t%d\t%d\n",t.kind,t.text,t.line,t.column);}
    else if(mode==D_AST){print_ast(program,out);fputc('\n',out);}
    else if(mode==D_IR)print_ir(ir,out);
    else if(mode==D_ASM)print_assembly(ir,out);
    else if(mode==D_SYMBOLS)print_symbols(&ctx,program,out);
    else if(mode==D_SDD)print_attributes(&ctx,program,out);
    else if(mode==D_DEPS)print_dependencies(&ctx,program,out);
    else if(mode==D_CFG)print_cfg(ir,out);
    if(ferror(out)){fputs("ERRO GERACAO: falha ao salvar a saída.\n",stderr);status=5;}
    if(output&&fclose(out))status=5;
done:context_free(&ctx);return status;
usage:fputs(driver_help,stderr);return 1;
}
