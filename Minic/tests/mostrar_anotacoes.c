/* Teste de contrato: expõe as anotações da AST, sem alterar as entradas oficiais. */
#include "../src/common.h"

static const char *kinds[]={"program","function","param","decl","block","if","while",
    "return","expr","empty","literal","id","call","index","assign","binary","unary","break","continue","for"};

static void dump(Node *n) {
    if(!n)return;
    printf("%s\t%d\t%d\t%s\t%s\t%s\t%s\t",kinds[n->kind],n->token->line,n->token->column,
        n->semantic_type,n->category,n->conversion_to,n->expected_type);
    if(n->has_array_size)printf("%lld",n->array_size);else fputs("-",stdout);
    printf("\t%d\t%d\t%d\t",n->assignable,n->loop_depth,n->flow);
    if(n->symbol)printf("%s:%d:%d",n->symbol->node->value,n->symbol->node->token->line,n->symbol->node->token->column);
    else fputs("-",stdout);
    fputc('\n',stdout);
    for(Node *child=n->children;child;child=child->next)dump(child);
    for(Node *param=n->params;param;param=param->next)dump(param);
    dump(n->left);dump(n->right);dump(n->body);dump(n->otherwise);dump(n->size);dump(n->initializer);
}

int main(int argc,char **argv) {
    configure_output();
    if(argc!=2)return 2;
    Context ctx={0};
    if(!read_source(&ctx,argv[1])){context_free(&ctx);return 2;}
    Node *program=scan_source(&ctx)?parse_program(&ctx):NULL;
    if(!program){print_failure(&ctx);context_free(&ctx);return 1;}
    analyze_semantics(&ctx,program);
    dump(program);
    context_free(&ctx);
    return 0;
}
