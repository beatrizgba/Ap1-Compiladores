#include "ir.h"
#include <string.h>
#include <stdlib.h>

static const char *sdd_kinds[]={"program","function","param","decl","block","if","while","return","expr","empty","literal","id","call","index","assign","binary","unary","break","continue","for"};
static void sdd_collect(Node *n,Node **nodes,size_t *count) {
    if(!n)return;
    nodes[(*count)++]=n;
    for(Node *c=n->children;c;c=c->next)sdd_collect(c,nodes,count);
    for(Node *c=n->params;c;c=c->next)sdd_collect(c,nodes,count);
    sdd_collect(n->left,nodes,count);sdd_collect(n->right,nodes,count);sdd_collect(n->body,nodes,count);
    sdd_collect(n->otherwise,nodes,count);sdd_collect(n->size,nodes,count);sdd_collect(n->initializer,nodes,count);
}
static void sdd_quote(FILE *out,const char *text) {
    fputc('"',out);
    for(const unsigned char *p=(const unsigned char *)text;*p;p++) {
        if(*p=='"'||*p=='\\'){fputc('\\',out);fputc(*p,out);}
        else if(*p=='\n')fputs("\\n",out);else if(*p=='\r')fputs("\\r",out);else if(*p=='\t')fputs("\\t",out);
        else if(*p<32)fprintf(out,"\\u%04x",*p);else fputc(*p,out);
    }
    fputc('"',out);
}
typedef struct SDDEdge {const char *a,*b;} SDDEdge;
typedef struct SDDGraph {Context *ctx;Node **nodes;size_t count,edges;SDDEdge *edge;const char **names,**values;} SDDGraph;
static const char *sdd_attrs[]={"type","symbol","loop","place","code"};
static size_t sdd_id(SDDGraph *g,Node *n){for(size_t k=0;k<g->count;k++)if(g->nodes[k]==n)return k;return 0;}
static void sdd_edge(SDDGraph *g,size_t a,int aa,size_t b,int ba) {
    const char *from=g->names[a*5+(size_t)aa],*to=g->names[b*5+(size_t)ba];
    for(size_t k=0;k<g->edges;k++)if(!strcmp(g->edge[k].a,from)&&!strcmp(g->edge[k].b,to))return;
    g->edge[g->edges++]=(SDDEdge){from,to};
}
static void sdd_child(SDDGraph *g,size_t k,Node *child) {
    if(!child)return;
    size_t c=sdd_id(g,child);Node *n=g->nodes[k];sdd_edge(g,c,4,k,4);
    if(child->kind!=N_FUNCTION)sdd_edge(g,k,2,c,2);
    if(n->kind==N_BINARY||n->kind==N_UNARY||n->kind==N_INDEX||n->kind==N_ASSIGN||n->kind==N_CALL)sdd_edge(g,c,0,k,0);
}
static SDDGraph sdd_graph(Context *ctx,Node *program) {
    SDDGraph g={0};g.ctx=ctx;g.nodes=allocate(ctx,(ctx->count*2+16)*sizeof(*g.nodes));sdd_collect(program,g.nodes,&g.count);
    g.names=allocate(ctx,g.count*5*sizeof(*g.names));g.values=allocate(ctx,g.count*5*sizeof(*g.values));g.edge=allocate(ctx,(g.count*16+16)*sizeof(*g.edge));
    for(size_t k=0;k<g.count;k++) {
        Node *n=g.nodes[k];const char *values[]={*n->semantic_type?n->semantic_type:"-",n->symbol?n->symbol->node->value:"-",format_text(ctx,"%d",n->loop_depth),n->ir_place?n->ir_place:"-",format_text(ctx,"%zu:%zu",n->ir_begin,n->ir_end)};
        for(int j=0;j<5;j++){g.names[k*5+(size_t)j]=format_text(ctx,"n%zu_%s",k,sdd_attrs[j]);g.values[k*5+(size_t)j]=format_text(ctx,"n%zu.%s=%s",k,sdd_attrs[j],values[j]);}
    }
    for(size_t k=0;k<g.count;k++) {
        Node *n=g.nodes[k];sdd_edge(&g,k,1,k,0);sdd_edge(&g,k,0,k,3);sdd_edge(&g,k,3,k,4);sdd_edge(&g,k,2,k,4);
        if(n->symbol&&n->symbol->node!=n)sdd_edge(&g,sdd_id(&g,n->symbol->node),1,k,1);
        for(Node *c=n->children;c;c=c->next)sdd_child(&g,k,c);
        for(Node *c=n->params;c;c=c->next)sdd_child(&g,k,c);
        sdd_child(&g,k,n->left);sdd_child(&g,k,n->right);sdd_child(&g,k,n->body);sdd_child(&g,k,n->otherwise);sdd_child(&g,k,n->size);sdd_child(&g,k,n->initializer);
    }
    return g;
}
static int sdd_compare_edge(const void *a,const void *b){const SDDEdge *x=a,*y=b;int c=strcmp(x->a,y->a);return c?c:strcmp(x->b,y->b);}
static int sdd_compare_name(const void *a,const void *b){return strcmp(*(const char *const *)a,*(const char *const *)b);}
void print_dependencies(Context *ctx,Node *program,FILE *out) {
    SDDGraph g=sdd_graph(ctx,program);fputs("digraph SDD {\n",out);
    for(size_t k=0;k<g.count*5;k++){fprintf(out,"  %s [label=",g.names[k]);sdd_quote(out,g.values[k]);fputs("];\n",out);}
    qsort(g.edge,g.edges,sizeof(*g.edge),sdd_compare_edge);
    for(size_t k=0;k<g.edges;k++)fprintf(out,"  %s -> %s;\n",g.edge[k].a,g.edge[k].b);
    fputs("}\n",out);
}
void print_attributes(Context *ctx,Node *program,FILE *out) {
    SDDGraph g=sdd_graph(ctx,program);fputs("no\tclasse\tlinha\tcoluna\ttipo\tsimbolo\tplace\tcode\tloop\n",out);
    for(size_t k=0;k<g.count;k++) {
        Node *n=g.nodes[k];fprintf(out,"%zu\t%s\t%d\t%d\t%s\t%s\t%s\t%zu:%zu\t%d\n",k,sdd_kinds[n->kind],n->token->line,n->token->column,n->semantic_type,n->symbol?n->symbol->node->value:"-",n->ir_place?n->ir_place:"-",n->ir_begin,n->ir_end,n->loop_depth);
    }
    unsigned char *done=allocate(ctx,g.count*5);const char **ready=allocate(ctx,g.count*5*sizeof(*ready));size_t remaining=g.count*5;int first=1;
    fputs("ordem_topologica\t",out);
    while(remaining) {
        size_t count=0;
        for(size_t k=0;k<g.count*5;k++)if(!done[k]) {
            int blocked=0;
            for(size_t e=0;e<g.edges&&!blocked;e++)if(!strcmp(g.edge[e].b,g.names[k]))
                for(size_t j=0;j<g.count*5;j++)if(!done[j]&&!strcmp(g.names[j],g.edge[e].a)){blocked=1;break;}
            if(!blocked)ready[count++]=g.names[k];
        }
        if(!count){fputs("CICLO\n",out);return;}
        qsort(ready,count,sizeof(*ready),sdd_compare_name);
        for(size_t k=0;k<count;k++) {
            fprintf(out,"%s%s",first?"":",",ready[k]);first=0;
            for(size_t j=0;j<g.count*5;j++)if(!strcmp(g.names[j],ready[k]))done[j]=1;
        }
        remaining-=count;
    }
    fputc('\n',out);
}
static void symbols_visit(Context *ctx,Node *n,int level,unsigned long long *slot,Node **nodes,size_t count,FILE *out) {
    if(!n)return;
    if(n->kind==N_DECL||n->kind==N_PARAM||n->kind==N_FUNCTION) {
        size_t uses=0;for(size_t k=0;k<count;k++)if(nodes[k]!=n&&nodes[k]->symbol&&nodes[k]->symbol->node==n)uses++;
        const char *state=n->kind==N_PARAM?"parametro":n->initializer?"inicializador":n->global_scope?"zero":"declarada";
        fprintf(out,"%s\t%s\t%s\t%d\t%d\t%d\t%s\t",n->value,n->category,n->semantic_type,level,n->token->line,n->token->column,n->kind==N_FUNCTION?n->value:format_text(ctx,"%s%d_%d_%s",n->global_scope?"g":"v",n->token->line,n->token->column,n->value));
        if(n->kind==N_FUNCTION)fputc('-',out);else fprintf(out,"%llu",*slot);
        fprintf(out,"\t%s\t%zu\n",state,uses);
        if(n->kind!=N_FUNCTION)*slot+=(unsigned long long)(n->array&&n->kind!=N_PARAM?n->array_size:1);
    }
    if(n->kind==N_FUNCTION) {
        unsigned long long local=0;
        for(Node *c=n->params;c;c=c->next)symbols_visit(ctx,c,1,&local,nodes,count,out);
        for(Node *c=n->body->children;c;c=c->next)symbols_visit(ctx,c,1,&local,nodes,count,out);
    } else if(n->kind==N_PROGRAM||n->kind==N_BLOCK) {
        for(Node *c=n->children;c;c=c->next)symbols_visit(ctx,c,level+(n->kind==N_BLOCK),slot,nodes,count,out);
    } else {
        for(Node *c=n->children;c;c=c->next)symbols_visit(ctx,c,level,slot,nodes,count,out);
        symbols_visit(ctx,n->left,level,slot,nodes,count,out);symbols_visit(ctx,n->right,level,slot,nodes,count,out);symbols_visit(ctx,n->body,level,slot,nodes,count,out);symbols_visit(ctx,n->otherwise,level,slot,nodes,count,out);symbols_visit(ctx,n->size,level,slot,nodes,count,out);symbols_visit(ctx,n->initializer,level,slot,nodes,count,out);
    }
}
void print_symbols(Context *ctx,Node *program,FILE *out) {
    Node **nodes=allocate(ctx,(ctx->count*2+16)*sizeof(*nodes));size_t count=0;unsigned long long slot=0;sdd_collect(program,nodes,&count);
    fputs("nome\tcategoria\ttipo\tnivel\tlinha\tcoluna\tendereco\tslot\tinicializacao\tusos\n",out);symbols_visit(ctx,program,0,&slot,nodes,count,out);
}
