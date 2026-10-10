#include "../common.h"
#include <setjmp.h>
#include <string.h>

typedef struct Parser { Context *ctx; size_t pos; jmp_buf failure; } Parser;
static Token *current(Parser *p) { return &p->ctx->tokens[p->pos]; }
static int is_text(Parser *p,const char *text) { return !strcmp(current(p)->text,text); }
static Token *take(Parser *p) { Token *t=current(p);if(strcmp(t->kind,"EOF"))p->pos++;return t; }
static void parse_fail(Parser *p,const char *message) {
    Token *t=current(p);p->ctx->stage="SINTATICO";
    p->ctx->failure=copy_text(p->ctx,message,strlen(message));
    p->ctx->failure_line=t->line;p->ctx->failure_column=t->column;
    longjmp(p->failure,1);
}
static Token *match(Parser *p,const char *text) { return is_text(p,text)?take(p):NULL; }
static Token *expect(Parser *p,const char *text) {
    if(!is_text(p,text))parse_fail(p,format_text(p->ctx,"esperado '%s'",text));
    return take(p);
}
static int is_type(Parser *p) {
    const char *t=current(p)->text;
    return !strcmp(t,"int")||!strcmp(t,"float")||!strcmp(t,"char")||!strcmp(t,"bool")||!strcmp(t,"void");
}
static Token *type_token(Parser *p) { if(!is_type(p))parse_fail(p,"tipo esperado");return take(p); }
static Token *identifier(Parser *p) { if(strcmp(current(p)->kind,"IDENT"))parse_fail(p,"identificador esperado");return take(p); }
static Node *node(Parser *p,Kind kind,Token *token,size_t end) {
    Node *n=allocate(p->ctx,sizeof(*n));n->kind=kind;n->token=token;n->end=end;n->value="";n->type_name="";return n;
}
static void append(Node **head,Node **tail,Node *n) {
    if(!*head)*head=n;else (*tail)->next=n;
    while(n->next)n=n->next;
    *tail=n;
}
static Node *expression(Parser *p);
static Node *statement(Parser *p);
static Node *block(Parser *p);
static Node *declaration(Parser *p,int global_scope) {
    Token *typ=type_token(p),*name=identifier(p);
    if(global_scope && match(p,"(")) {
        Node *params=NULL,*tail=NULL;
        if(is_text(p,"void") && p->pos+1<p->ctx->count && !strcmp(p->ctx->tokens[p->pos+1].text,")"))take(p);
        else if(!is_text(p,")")) {
            do {
                Token *pt=type_token(p),*pn=identifier(p);
                Node *param=node(p,N_PARAM,pn,pn->end);param->value=pn->text;param->type_name=pt->text;
                if(match(p,"[")){param->array=1;expect(p,"]");}
                append(&params,&tail,param);
            }while(match(p,","));
        }
        expect(p,")");Node *body=block(p);
        Node *n=node(p,N_FUNCTION,typ,body->end);n->value=name->text;n->type_name=typ->text;n->params=params;n->body=body;return n;
    }
    Node *head=NULL,*tail=NULL;
    do {
        Node *n=node(p,N_DECL,name,name->end);n->value=name->text;n->type_name=typ->text;
        if(match(p,"[")){n->array=1;n->size=expression(p);expect(p,"]");}
        if(match(p,"="))n->initializer=expression(p);
        n->end=p->ctx->tokens[p->pos-1].end;append(&head,&tail,n);
        if(!match(p,","))break;
        name=identifier(p);
    }while(1);
    expect(p,";");return head;
}
static Node *block(Parser *p) {
    Token *start=expect(p,"{");Node *n=node(p,N_BLOCK,start,start->end),*tail=NULL;
    while(!is_text(p,"}")) {
        if(!strcmp(current(p)->kind,"EOF"))parse_fail(p,"esperado '}'");
        Node *child=is_type(p)?declaration(p,0):statement(p);append(&n->children,&tail,child);
    }
    n->end=take(p)->end;return n;
}
static Node *statement(Parser *p) {
    Token *start=current(p);Node *n;
    if(is_text(p,"{"))return block(p);
    if(match(p,"if")) {
        n=node(p,N_IF,start,start->end);expect(p,"(");n->left=expression(p);expect(p,")");n->body=statement(p);
        if(match(p,"else"))n->otherwise=statement(p);
        n->end=(n->otherwise?n->otherwise:n->body)->end;return n;
    }
    if(match(p,"while")) {
        n=node(p,N_WHILE,start,start->end);expect(p,"(");n->left=expression(p);expect(p,")");n->body=statement(p);n->end=n->body->end;return n;
    }
    if(match(p,"return")) {
        n=node(p,N_RETURN,start,start->end);if(!is_text(p,";"))n->left=expression(p);n->end=expect(p,";")->end;return n;
    }
    if(match(p,";"))return node(p,N_EMPTY,start,start->end);
    n=node(p,N_EXPR,start,start->end);n->left=expression(p);n->end=expect(p,";")->end;return n;
}
static int precedence(const char *op) {
    if(!strcmp(op,"||"))return 1;
    if(!strcmp(op,"&&"))return 2;
    if(!strcmp(op,"==")||!strcmp(op,"!="))return 3;
    if(!strcmp(op,"<")||!strcmp(op,"<=")||!strcmp(op,">")||!strcmp(op,">="))return 4;
    if(!strcmp(op,"+")||!strcmp(op,"-"))return 5;
    if(!strcmp(op,"*")||!strcmp(op,"/")||!strcmp(op,"%"))return 6;
    return 0;
}
static Node *primary(Parser *p) {
    Token *t=current(p);char *typ=NULL;
    if(!strcmp(t->kind,"INT_LIT"))typ="int";
    if(!strcmp(t->kind,"FLOAT_LIT"))typ="float";
    if(!strcmp(t->kind,"CHAR_LIT"))typ="char";
    if(!strcmp(t->kind,"STRING_LIT"))typ="string";
    if(!strcmp(t->kind,"TRUE")||!strcmp(t->kind,"FALSE"))typ="bool";
    if(typ){take(p);Node *n=node(p,N_LITERAL,t,t->end);n->value=t->text;n->type_name=typ;return n;}
    if(match(p,"(")){Node *n=expression(p);expect(p,")");return n;}
    if(!strcmp(t->kind,"IDENT")||!strcmp(t->kind,"PRINT")||!strcmp(t->kind,"READ")) {
        take(p);Node *n=node(p,N_ID,t,t->end);n->value=t->text;
        if(match(p,"(")) {
            n->kind=N_CALL;Node *tail=NULL;
            if(!is_text(p,")"))do {Node *arg=expression(p);append(&n->children,&tail,arg);}while(match(p,","));
            n->end=expect(p,")")->end;
        }
        while(match(p,"[")) {
            Node *index=expression(p);Node *access=node(p,N_INDEX,t,expect(p,"]")->end);access->left=n;access->right=index;n=access;
        }
        return n;
    }
    parse_fail(p,"expressão esperada");return NULL;
}
static Node *unary(Parser *p) {
    if(is_text(p,"!")||is_text(p,"-")||is_text(p,"+")) {
        Token *op=take(p);Node *right=unary(p),*n=node(p,N_UNARY,op,right->end);n->value=op->text;n->right=right;return n;
    }
    return primary(p);
}
static Node *binary(Parser *p,int minimum) {
    Node *left=unary(p);
    while(precedence(current(p)->text)>=minimum) {
        Token *op=take(p);Node *right=binary(p,precedence(op->text)+1),*n=node(p,N_BINARY,left->token,right->end);n->value=op->text;n->left=left;n->right=right;left=n;
    }
    return left;
}
static Node *expression(Parser *p) {
    Node *left=binary(p,1);
    if(match(p,"=")){Node *right=expression(p),*n=node(p,N_ASSIGN,left->token,right->end);n->left=left;n->right=right;return n;}
    return left;
}
Node *parse_program(Context *ctx) {
    Parser p;p.ctx=ctx;p.pos=0;
    if(setjmp(p.failure))return NULL;
    Node *program=node(&p,N_PROGRAM,current(&p),0),*tail=NULL;
    while(strcmp(current(&p)->kind,"EOF")) {
        Node *child=is_type(&p)?declaration(&p,1):statement(&p);append(&program->children,&tail,child);
    }
    program->end=current(&p)->end;return program;
}
static void ast_list(Node *head,FILE *out) {
    for(Node *n=head;n;n=n->next){if(n!=head)fputs(", ",out);print_ast(n,out);}
}
void print_ast(Node *n,FILE *out) {
    if(!n){fputs("NULL",out);return;}
    switch(n->kind) {
        case N_PROGRAM:fputs("Program(",out);ast_list(n->children,out);break;
        case N_FUNCTION:
            fprintf(out,"Function(%s %s(",n->type_name,n->value);
            for(Node *p=n->params;p;p=p->next){if(p!=n->params)fputc(',',out);fprintf(out,"%s %s%s",p->type_name,p->value,p->array?"[]":"");}
            fputs(") ",out);print_ast(n->body,out);break;
        case N_BLOCK:fputs("Block(",out);ast_list(n->children,out);break;
        case N_DECL:
            fprintf(out,"VarDecl(%s %s",n->type_name,n->value);
            if(n->array){fputs(" size=",out);print_ast(n->size,out);}
            if(n->initializer){fputs(" = ",out);print_ast(n->initializer,out);}break;
        case N_LITERAL:fprintf(out,"Lit(%s,%s",!strcmp(n->type_name,"float")?"real":n->type_name,n->value);break;
        case N_ID:fprintf(out,"Id(%s",n->value);break;
        case N_CALL:
            fprintf(out,"Call(Id(%s)",n->value);for(Node *a=n->children;a;a=a->next){fputc(',',out);print_ast(a,out);}break;
        case N_INDEX:fputs("Index(",out);print_ast(n->left,out);fputc(',',out);print_ast(n->right,out);break;
        case N_BINARY:fprintf(out,"Binary(%s, ",n->value);print_ast(n->left,out);fputs(", ",out);print_ast(n->right,out);break;
        case N_UNARY:fprintf(out,"Unary(%s,",n->value);print_ast(n->right,out);break;
        case N_ASSIGN:fputs("Assign(",out);print_ast(n->left,out);fputs(", ",out);print_ast(n->right,out);break;
        case N_EXPR:fputs("ExprStmt(",out);print_ast(n->left,out);break;
        case N_RETURN:fputs("Return(",out);print_ast(n->left,out);break;
        case N_IF:fputs("If(",out);print_ast(n->left,out);fputs(", ",out);print_ast(n->body,out);fputs(", ",out);print_ast(n->otherwise,out);break;
        case N_WHILE:fputs("While(",out);print_ast(n->left,out);fputs(", ",out);print_ast(n->body,out);break;
        default:fputs("EmptyStmt",out);return;
    }
    fputc(')',out);
}
