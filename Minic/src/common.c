#include "common.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static void memory_failure(void) { fputs("ERRO ENTRADA: memória insuficiente\n", stderr); exit(2); }
void *allocate(Context *ctx, size_t size) {
    void *data = calloc(1, size ? size : 1);
    Allocation *item = malloc(sizeof(*item));
    if (!data || !item) memory_failure();
    item->data = data; item->next = ctx->allocations; ctx->allocations = item;
    return data;
}
char *copy_text(Context *ctx, const char *text, size_t length) {
    char *result = allocate(ctx, length + 1);
    memcpy(result, text, length);
    return result;
}
char *format_text(Context *ctx, const char *format, ...) {
    va_list args, copy;
    va_start(args, format); va_copy(copy, args);
    int length = vsnprintf(NULL, 0, format, copy); va_end(copy);
    if (length < 0) { va_end(args); memory_failure(); }
    char *result = allocate(ctx, (size_t)length + 1);
    vsnprintf(result, (size_t)length + 1, format, args); va_end(args);
    return result;
}
char *source_text(Context *ctx, Node *n) {
    size_t start = n->token->start, end = n->end;
    while (start < end && (ctx->source[start]==' ' || ctx->source[start]=='\n' || ctx->source[start]=='\t')) start++;
    while (end > start && (ctx->source[end-1]==' ' || ctx->source[end-1]=='\n' || ctx->source[end-1]=='\t')) end--;
    return copy_text(ctx, ctx->source + start, end-start);
}
size_t source_position(Context *ctx, int line, int column) {
    size_t i = 0; int current_line = 1, current_column = 1;
    while (ctx->source[i] && (current_line < line || current_column < column)) {
        unsigned char c = (unsigned char)ctx->source[i++];
        if (c=='\n') { current_line++; current_column=1; }
        else if ((c & 0xc0)!=0x80) current_column++;
    }
    return i;
}
void context_free(Context *ctx) {
    Allocation *item = ctx->allocations;
    while (item) { Allocation *next=item->next; free(item->data); free(item); item=next; }
    free(ctx->tokens); free(ctx->errors);
    memset(ctx, 0, sizeof(*ctx));
}
int read_source(Context *ctx, const char *path) {
    FILE *file = fopen(path,"rb");
    if (!file) return 0;
    if (fseek(file,0,SEEK_END)) { fclose(file); return 0; }
    long length=ftell(file);
    if (length<0 || fseek(file,0,SEEK_SET)) { fclose(file); return 0; }
    char *raw = allocate(ctx,(size_t)length+1);
    size_t got=fread(raw,1,(size_t)length,file);
    int ok=!ferror(file) && got==(size_t)length; fclose(file);
    if (!ok) return 0;
    ctx->source=allocate(ctx,got+1);
    size_t out=0;
    for(size_t i=0;i<got;i++) {
        if(raw[i]=='\r') { if(i+1<got && raw[i+1]=='\n') i++; ctx->source[out++]='\n'; }
        else ctx->source[out++]=raw[i];
    }
    ctx->source_length=out;
    return 1;
}
void print_failure(Context *ctx) {
    fprintf(stderr,"ERRO %s linha %d, coluna %d: %s\n",ctx->stage,ctx->failure_line,ctx->failure_column,ctx->failure);
}
void add_error(Context *ctx, const char *code, Node *n, const char *message) {
    if(ctx->error_count==ctx->error_capacity) {
        size_t capacity=ctx->error_capacity ? 2*ctx->error_capacity : 16;
        Diagnostic *data=realloc(ctx->errors,capacity*sizeof(*data));
        if(!data) memory_failure();
        ctx->errors=data;ctx->error_capacity=capacity;
    }
    Diagnostic d={copy_text(ctx,code,strlen(code)),copy_text(ctx,message,strlen(message)),n->token->line,n->token->column,ctx->error_count};
    ctx->errors[ctx->error_count++]=d;
}
static int compare_diagnostics(const void *a,const void *b) {
    const Diagnostic *x=a,*y=b;
    if(x->line!=y->line) return x->line<y->line?-1:1;
    if(x->column!=y->column) return x->column<y->column?-1:1;
    return x->order<y->order?-1:x->order>y->order;
}
void print_semantics(Context *ctx) {
    if(ctx->error_count) qsort(ctx->errors,ctx->error_count,sizeof(*ctx->errors),compare_diagnostics);
    for(size_t i=0;i<ctx->error_count;i++) {
        Diagnostic *d=&ctx->errors[i];
        printf("%s — linha %d, coluna %d: %s\r\n",d->code,d->line,d->column,d->message);
    }
    printf("Análise semântica concluída: %zu %s; programa %s.",ctx->error_count,ctx->error_count==1?"erro":"erros",ctx->error_count?"rejeitado":"aceito");
}
