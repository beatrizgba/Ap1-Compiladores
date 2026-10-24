#include "ir.h"
#include <string.h>
#include <stdlib.h>

typedef struct IRGenerator { IR *ir; int temp,label,depth;const char **breaks,**continues; } IRGenerator;
static const char *ir_storage(Context *ctx,Node *n){return format_text(ctx,"%s%d_%d_%s",n->global_scope?"g":"v",n->token->line,n->token->column,n->value);}
const char *ir_emit(IR *ir,const char *op,const char *d,const char *a,const char *b,const char *typ) {
    if(ir->count==ir->capacity) {
        size_t capacity=ir->capacity?ir->capacity*2:128;
        IRInstruction *code=allocate(ir->ctx,capacity*sizeof(*code));
        if(ir->count)memcpy(code,ir->code,ir->count*sizeof(*code));
        ir->code=code;ir->capacity=capacity;
    }
    ir->code[ir->count++]=(IRInstruction){op,d,a,b,typ};return d;
}
static const char *ir_temp(IRGenerator *g){return format_text(g->ir->ctx,"t%d",++g->temp);}
static const char *ir_label(IRGenerator *g){return format_text(g->ir->ctx,"L%d",++g->label);}
static const char *ir_value(IRGenerator *g,Node *n);
typedef struct IRAddress {const char *base,*index;} IRAddress;
static IRAddress ir_address(IRGenerator *g,Node *n) {
    n->ir_begin=g->ir->count;IRAddress target;
    if(n->kind==N_ID)target=(IRAddress){ir_storage(g->ir->ctx,n->symbol->node),NULL};
    else {const char *base=ir_value(g,n->left),*index=ir_value(g,n->right);target=(IRAddress){base,index};}
    n->ir_place=target.index?format_text(g->ir->ctx,"%s[%s]",target.base,target.index):target.base;n->ir_end=g->ir->count;return target;
}
static void ir_store(IRGenerator *g,IRAddress target,const char *value,const char *typ) {
    if(target.index)ir_emit(g->ir,"STORE",target.base,target.index,value,typ);
    else ir_emit(g->ir,"MOV",target.base,value,"-",typ);
}
static const char *ir_expression(IRGenerator *g,Node *n) {
    IR *ir=g->ir;Context *ctx=ir->ctx;
    switch(n->kind) {
        case N_LITERAL: {
            if(!strchr(n->value,'\t'))return n->value;
            size_t length=strlen(n->value),pos=0;char *text=allocate(ctx,length*2+1);
            for(size_t k=0;k<length;k++){if(n->value[k]=='\t'){text[pos++]='\\';text[pos++]='t';}else text[pos++]=n->value[k];}
            return text;
        }
        case N_ID:return ir_emit(ir,"MOV",ir_temp(g),ir_storage(ctx,n->symbol->node),"-",n->semantic_type);
        case N_INDEX: {
            const char *base=ir_value(g,n->left),*index=ir_value(g,n->right);
            return ir_emit(ir,"LOAD",ir_temp(g),base,index,n->semantic_type);
        }
        case N_ASSIGN: {
            IRAddress target=ir_address(g,n->left);const char *value=ir_value(g,n->right);
            ir_store(g,target,value,n->left->semantic_type);return value;
        }
        case N_UNARY: {
            const char *value=ir_value(g,n->right);
            if(!strcmp(n->value,"+"))return value;
            return ir_emit(ir,!strcmp(n->value,"!")?"NOT":"NEG",ir_temp(g),value,"-",n->semantic_type);
        }
        case N_BINARY: {
            const char *left=ir_value(g,n->left),*op=n->value;
            if(!strcmp(op,"&&")||!strcmp(op,"||")) {
                const char *dest=ir_temp(g),*end=ir_label(g);
                ir_emit(ir,"MOV",dest,left,"-","bool");ir_emit(ir,!strcmp(op,"&&")?"JFALSE":"JTRUE",end,left,"-","void");
                const char *right=ir_value(g,n->right);ir_emit(ir,"MOV",dest,right,"-","bool");ir_emit(ir,"LABEL",end,"-","-","void");return dest;
            }
            const char *right=ir_value(g,n->right),*operation="";
            const char *ops[]={"+","-","*","/","%","<","<=",">",">=","==","!="};
            const char *names[]={"ADD","SUB","MUL","DIV","MOD","LT","LE","GT","GE","EQ","NE"};
            for(size_t i=0;i<11;i++)if(!strcmp(op,ops[i]))operation=names[i];
            return ir_emit(ir,operation,ir_temp(g),left,right,n->semantic_type);
        }
        case N_CALL: {
            if(!strcmp(n->value,"print")) {
                const char *value=ir_value(g,n->children);ir_emit(ir,"PRINT","-",value,"-",n->children->semantic_type);return "-";
            }
            if(!strcmp(n->value,"read")) {
                IRAddress target=ir_address(g,n->children);const char *typ=n->children->semantic_type;
                const char *value=ir_emit(ir,"READ",ir_temp(g),"-","-",typ);ir_store(g,target,value,typ);return "-";
            }
            size_t count=0,index=0;for(Node *a=n->children;a;a=a->next)count++;
            const char **args=allocate(ctx,(count?count:1)*sizeof(*args));
            for(Node *a=n->children;a;a=a->next)args[index++]=ir_value(g,a);
            index=0;for(Node *a=n->children;a;a=a->next)ir_emit(ir,"PARAM","-",args[index++],"-",*a->conversion_to?a->conversion_to:a->semantic_type);
            return ir_emit(ir,"CALL",strcmp(n->semantic_type,"void")?ir_temp(g):"-",n->value,format_text(ctx,"%zu",count),n->semantic_type);
        }
        default:return "-";
    }
}
static const char *ir_value(IRGenerator *g,Node *n) {
    n->ir_begin=g->ir->count;
    const char *result=ir_expression(g,n);
    if(*n->conversion_to)result=ir_emit(g->ir,"CAST",ir_temp(g),result,"-",n->conversion_to);
    n->ir_place=result;n->ir_end=g->ir->count;return result;
}
static void ir_statement_impl(IRGenerator *g,Node *n);
static void ir_statement(IRGenerator *g,Node *n) {
    n->ir_begin=g->ir->count;ir_statement_impl(g,n);n->ir_end=g->ir->count;
}
static void ir_statement_impl(IRGenerator *g,Node *n) {
    IR *ir=g->ir;
    switch(n->kind) {
        case N_BLOCK:for(Node *child=n->children;child;child=child->next)ir_statement(g,child);break;
        case N_DECL: {
            const char *dest=ir_storage(ir->ctx,n);
            ir_emit(ir,n->global_scope?"GLOBAL":"LOCAL",dest,n->array?format_text(ir->ctx,"%lld",n->array_size):"-","-",n->semantic_type);
            if(n->initializer){const char *value=ir_value(g,n->initializer);ir_emit(ir,"MOV",dest,value,"-",n->type_name);}break;
        }
        case N_EXPR:ir_value(g,n->left);break;
        case N_RETURN: {
            const char *value=n->left?ir_value(g,n->left):"-";ir_emit(ir,"RETURN","-",value,"-",n->expected_type);break;
        }
        case N_IF: {
            const char *other=ir_label(g),*end=ir_label(g),*value=ir_value(g,n->left);
            ir_emit(ir,"JFALSE",other,value,"-","void");ir_statement(g,n->body);ir_emit(ir,"JUMP",end,"-","-","void");
            ir_emit(ir,"LABEL",other,"-","-","void");if(n->otherwise)ir_statement(g,n->otherwise);ir_emit(ir,"LABEL",end,"-","-","void");break;
        }
        case N_WHILE:case N_FOR: {
            const char *test=ir_label(g),*step=ir_label(g),*end=ir_label(g);
            if(n->initializer)ir_value(g,n->initializer);
            ir_emit(ir,"LABEL",test,"-","-","void");
            if(n->left){const char *value=ir_value(g,n->left);ir_emit(ir,"JFALSE",end,value,"-","void");}
            g->breaks[g->depth]=end;g->continues[g->depth++]=n->kind==N_FOR?step:test;ir_statement(g,n->body);g->depth--;
            ir_emit(ir,"LABEL",step,"-","-","void");if(n->kind==N_FOR && n->right)ir_value(g,n->right);
            ir_emit(ir,"JUMP",test,"-","-","void");ir_emit(ir,"LABEL",end,"-","-","void");break;
        }
        case N_BREAK:case N_CONTINUE:ir_emit(ir,"JUMP",n->kind==N_BREAK?g->breaks[g->depth-1]:g->continues[g->depth-1],"-","-","void");break;
        default:break;
    }
}
IR *generate_ir(Context *ctx,Node *program) {
    IR *ir=allocate(ctx,sizeof(*ir));ir->ctx=ctx;IRGenerator g={ir,0,0,0,NULL,NULL};
    g.breaks=allocate(ctx,(ctx->count+1)*sizeof(*g.breaks));g.continues=allocate(ctx,(ctx->count+1)*sizeof(*g.continues));
    ir_emit(ir,"FUNC","$init","-","-","void");
    for(Node *n=program->children;n;n=n->next)if(n->kind==N_DECL)n->global_scope=1;
    for(Node *n=program->children;n;n=n->next)if(n->kind==N_DECL)
        ir_emit(ir,"GLOBAL",ir_storage(ctx,n),n->array?format_text(ctx,"%lld",n->array_size):"-","-",n->semantic_type);
    for(Node *n=program->children;n;n=n->next)if(n->kind==N_DECL && n->initializer) {
        const char *value=ir_value(&g,n->initializer);ir_emit(ir,"MOV",ir_storage(ctx,n),value,"-",n->type_name);
    }
    ir_emit(ir,"RETURN","-","-","-","void");ir_emit(ir,"END","$init","-","-","void");
    for(Node *n=program->children;n;n=n->next)if(n->kind==N_FUNCTION) {
        n->ir_begin=ir->count;
        g.temp=g.label=0;ir_emit(ir,"FUNC",n->value,"-","-",n->type_name);
        for(Node *p=n->params;p;p=p->next){p->ir_begin=ir->count;ir_emit(ir,"ARG",ir_storage(ctx,p),"-","-",p->semantic_type);p->ir_end=ir->count;}
        ir_statement(&g,n->body);ir_emit(ir,"RETURN","-",!strcmp(n->type_name,"void")?"-":"0","-",n->type_name);ir_emit(ir,"END",n->value,"-","-","void");
        n->ir_end=ir->count;
    }
    program->ir_end=ir->count;
    return ir;
}
void print_ir(IR *ir,FILE *out) {
    for(size_t k=0;k<ir->count;k++){IRInstruction i=ir->code[k];fprintf(out,"%s\t%s\t%s\t%s\t%s\n",i.op,i.d,i.a,i.b,i.typ);}
}
int validate_ir(IR *ir) {
    int stack_code=0;for(size_t k=0;k<ir->count;k++)if(!strcmp(ir->code[k].op,"PUSH")||!strcmp(ir->code[k].op,"POP"))stack_code=1;
    size_t start=0,pending=0;const char *function=NULL;
    for(size_t k=0;k<ir->count;k++) {
        IRInstruction i=ir->code[k];
        if(!strcmp(i.op,"FUNC")){if(function)return 0;function=i.d;start=k;}
        else if(!strcmp(i.op,"END")){if(!function || strcmp(function,i.d)||pending)return 0;function=NULL;}
        else if(!function)return 0;
        else if(!strcmp(i.op,"JUMP")||!strcmp(i.op,"JFALSE")||!strcmp(i.op,"JTRUE")) {
            int found=0;for(size_t j=start+1;j<ir->count && strcmp(ir->code[j].op,"END");j++)
                if(!strcmp(ir->code[j].op,"LABEL")&&!strcmp(ir->code[j].d,i.d))found=1;
            if(!found)return 0;
        }else if(!strcmp(i.op,"PARAM"))pending++;
        else if(!strcmp(i.op,"CALL")) {
            if(!stack_code && pending!=(size_t)strtoul(i.b,NULL,10))return 0;
            pending=0;int found=0;
            for(size_t j=0;j<ir->count;j++)if(!strcmp(ir->code[j].op,"FUNC")&&!strcmp(ir->code[j].d,i.a))found=1;
            if(!found)return 0;
        }
    }
    return !function;
}
