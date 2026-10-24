/* Driver ascendente SLR(1); reduções sintetizam a AST na pilha. */
#include "../common.h"
#include "tabelas_lr.h"
#include <string.h>

typedef struct LRValue { Token *token; size_t end; Node *node,*tail; } LRValue;
static Node *lr_node(Context *ctx,Kind kind,Token *token,size_t end) {
    Node *n=allocate(ctx,sizeof(*n));n->kind=kind;n->token=token;n->end=end;
    n->value=n->type_name="";n->semantic_type=n->category=n->conversion_to=n->expected_type="";
    n->flow=FLOW_NORMAL;return n;
}
static LRValue lr_reduce(Context *ctx,int kind,LRValue *v,Token *start,size_t end) {
    LRValue out={start,end,NULL,NULL};Node *n=NULL;
    switch(kind) {
        case LR_EMPTY:break;
        case LR_PASS:case LR_TOKEN:out.node=v[0].node;out.tail=v[0].tail;break;
        case LR_APPEND:case LR_APPEND_COMMA: {
            LRValue b=v[kind==LR_APPEND?1:2];out.node=v[0].node;
            if(out.node){v[0].tail->next=b.node;out.tail=b.tail?b.tail:v[0].tail;}
            else{out.node=b.node;out.tail=b.tail;}break;
        }
        case LR_PROGRAM:n=lr_node(ctx,N_PROGRAM,start,end);n->children=v[0].node;break;
        case LR_FUNCTION:
            n=lr_node(ctx,N_FUNCTION,start,end);n->value=v[1].token->text;n->type_name=v[0].token->text;n->params=v[3].node;n->body=v[5].node;break;
        case LR_PARAM:case LR_PARAM_ARRAY:
            n=lr_node(ctx,N_PARAM,v[1].token,v[1].token->end);n->value=v[1].token->text;n->type_name=v[0].token->text;n->array=kind==LR_PARAM_ARRAY;break;
        case LR_DECLS:
            out.node=v[1].node;out.tail=v[1].tail;
            for(Node *d=out.node;d;d=d->next)d->type_name=v[0].token->text;
            break;
        case LR_DECL:case LR_DECL_INIT:case LR_DECL_ARRAY:case LR_DECL_ARRAY_INIT:
            n=lr_node(ctx,N_DECL,v[0].token,end);n->value=v[0].token->text;
            n->array=kind==LR_DECL_ARRAY||kind==LR_DECL_ARRAY_INIT;
            if(n->array)n->size=v[2].node;
            if(kind==LR_DECL_INIT)n->initializer=v[2].node;
            if(kind==LR_DECL_ARRAY_INIT)n->initializer=v[5].node;
            break;
        case LR_BLOCK:n=lr_node(ctx,N_BLOCK,start,end);n->children=v[1].node;break;
        case LR_IF:case LR_IF_ELSE:case LR_WHILE:
            n=lr_node(ctx,kind==LR_WHILE?N_WHILE:N_IF,start,end);n->left=v[2].node;n->body=v[4].node;
            if(kind==LR_IF_ELSE)n->otherwise=v[6].node;
            break;
        case LR_FOR:
            n=lr_node(ctx,N_FOR,start,end);n->initializer=v[2].node;n->left=v[4].node;n->right=v[6].node;n->body=v[8].node;break;
        case LR_RETURN:n=lr_node(ctx,N_RETURN,start,end);n->left=v[1].node;break;
        case LR_BREAK:case LR_CONTINUE:n=lr_node(ctx,kind==LR_BREAK?N_BREAK:N_CONTINUE,start,end);break;
        case LR_EMPTY_STMT:n=lr_node(ctx,N_EMPTY,start,end);break;
        case LR_EXPR_STMT:n=lr_node(ctx,N_EXPR,start,end);n->left=v[0].node;break;
        case LR_ASSIGN:case LR_BINARY:
            n=lr_node(ctx,kind==LR_ASSIGN?N_ASSIGN:N_BINARY,v[0].node->token,v[2].node->end);
            n->left=v[0].node;n->right=v[2].node;if(kind==LR_BINARY)n->value=v[1].token->text;break;
        case LR_UNARY:n=lr_node(ctx,N_UNARY,start,v[1].node->end);n->right=v[1].node;n->value=v[0].token->text;break;
        case LR_INDEX:n=lr_node(ctx,N_INDEX,v[0].node->token,end);n->left=v[0].node;n->right=v[2].node;break;
        case LR_CALL:n=lr_node(ctx,N_CALL,start,end);n->value=v[0].token->text;n->children=v[2].node;break;
        case LR_ID:n=lr_node(ctx,N_ID,start,end);n->value=v[0].token->text;break;
        case LR_PARENTHESES:out.node=v[1].node;out.tail=v[1].tail;break;
        case LR_LITERAL: {
            n=lr_node(ctx,N_LITERAL,start,end);n->value=v[0].token->text;char *t=v[0].token->kind;
            n->type_name=!strcmp(t,"INT_LIT")?"int":!strcmp(t,"FLOAT_LIT")?"float":!strcmp(t,"CHAR_LIT")?"char":!strcmp(t,"STRING_LIT")?"string":"bool";
            break;
        }
    }
    if(n){out.node=n;out.tail=n;}
    return out;
}
Node *parse_program(Context *ctx) {
    size_t capacity=ctx->count*4+128,top=0,pos=0,last_repair=(size_t)-1;Token *injected=NULL;
    int *states=allocate(ctx,capacity*sizeof(*states));LRValue *values=allocate(ctx,capacity*sizeof(*values));
    while(1) {
        Token *t=injected?injected:&ctx->tokens[pos];int term=-1;
        for(int i=0;i<LR_TERMINALS;i++)if(!strcmp(lr_terminals[i],t->kind)){term=i;break;}
        if(term<0)for(int i=0;i<LR_TERMINALS;i++)if(!strcmp(lr_terminals[i],t->text)){term=i;break;}
        int action=term<0?0:lr_action[states[top]][term];
        if(action==32767){if(ctx->syntax_count)return NULL;Node *n=values[top].node;n->token=ctx->tokens;n->end=t->end;if(ctx->trace_parser)fputs("accept\n",stderr);return n;}
        if(action>0) {
            if(ctx->trace_parser)fprintf(stderr,"shift %s -> %d\n",lr_terminals[term],action-1);
            ++top;states[top]=action-1;values[top]=(LRValue){t,t->end,NULL,NULL};if(injected)injected=NULL;else ++pos;
        } else if(action<0) {
            int r=-action,count=lr_length[r];Token *start=count?values[top-count+1].token:t;
            size_t end=count?values[top].end:t->start;
            LRValue out=lr_reduce(ctx,lr_kind[r],&values[top-count+1],start,end);
            top-=(size_t)count;int next=lr_goto[states[top]][lr_lhs[r]];
            ++top;states[top]=next;values[top]=out;
            if(ctx->trace_parser)fprintf(stderr,"reduce %d: %s\n",r,lr_rules[r]);
        } else {
            char *expected="";
            for(int i=0;i<LR_TERMINALS;i++)if(lr_action[states[top]][i])expected=format_text(ctx,"%s%s%s",expected,*expected?", ":"",lr_terminals[i]);
            ctx->stage="SINTATICO";ctx->failure=format_text(ctx,"encontrado '%s'; esperado: %s",t->text,expected);
            ctx->failure_line=t->line;ctx->failure_column=t->column;
            if(!ctx->final_mode)return NULL;
            ctx->syntax_report=format_text(ctx,"%sERRO SINTATICO linha %d, coluna %d: %s\n",ctx->syntax_report?ctx->syntax_report:"",t->line,t->column,ctx->failure);
            if(++ctx->syntax_count>=10||!strcmp(t->kind,"EOF"))return NULL;
            int semicolon=-1;for(int k=0;k<LR_TERMINALS;k++)if(!strcmp(lr_terminals[k],";"))semicolon=k;
            if(semicolon>=0&&lr_action[states[top]][semicolon]&&last_repair!=pos) {
                injected=allocate(ctx,sizeof(*injected));*injected=(Token){"SEMICOLON",";",t->line,t->column,t->start,t->start};last_repair=pos;continue;
            }
            injected=NULL;
            while(strcmp(ctx->tokens[pos].kind,"EOF")&&strcmp(ctx->tokens[pos].text,";")&&strcmp(ctx->tokens[pos].text,"}"))pos++;
            if(!strcmp(ctx->tokens[pos].kind,"EOF"))return NULL;
            int sync=-1;for(int k=0;k<LR_TERMINALS;k++)if(!strcmp(lr_terminals[k],ctx->tokens[pos].text))sync=k;
            while(top&&(sync<0||!lr_action[states[top]][sync]))top--;
            if(sync<0||!lr_action[states[top]][sync])pos++;
        }
    }
}
