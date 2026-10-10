#include "../common.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>

typedef struct Symbol { Node *node; const char *typ; int function; struct Symbol *next; } Symbol;
typedef struct Scope { Symbol *symbols; struct Scope *parent; } Scope;
typedef struct Analyzer { Context *ctx; Scope *scope; Node *function; } Analyzer;
static int equal(const char *a,const char *b) { return !strcmp(a,b); }
static int numeric(const char *t) { return equal(t,"int")||equal(t,"float"); }
static int array_type(const char *t) { size_t n=strlen(t);return n>=2 && !strcmp(t+n-2,"[]"); }
static const char *decl_type(Analyzer *a,Node *n) { return n->array?format_text(a->ctx,"%s[]",n->type_name):n->type_name; }
static int compatible(const char *target,const char *actual) { return equal(target,actual)||(equal(target,"float")&&equal(actual,"int")); }
static void push_scope(Analyzer *a) { Scope *s=allocate(a->ctx,sizeof(*s));s->parent=a->scope;a->scope=s; }
static void pop_scope(Analyzer *a) { a->scope=a->scope->parent; }
static Symbol *lookup(Analyzer *a,const char *name) {
    for(Scope *scope=a->scope;scope;scope=scope->parent)
        for(Symbol *s=scope->symbols;s;s=s->next)if(equal(s->node->value,name))return s;
    return NULL;
}
static void declare(Analyzer *a,Node *n) {
    Symbol *old=NULL;
    for(Symbol *s=a->scope->symbols;s;s=s->next)if(equal(s->node->value,n->value)){old=s;break;}
    if(old) {
        add_error(a->ctx,"SEM002",n,format_text(a->ctx,"“%s” já declarado neste escopo; declaração anterior na linha %d, coluna %d (tipo %s).",n->value,old->node->token->line,old->node->token->column,old->typ));
    } else {
        Symbol *s=allocate(a->ctx,sizeof(*s));s->node=n;s->typ=decl_type(a,n);s->function=n->kind==N_FUNCTION;s->next=a->scope->symbols;a->scope->symbols=s;
    }
    if(equal(n->type_name,"void") && n->kind!=N_FUNCTION)
        add_error(a->ctx,"SEM004",n,format_text(a->ctx,"“%s” não pode ter tipo void.",n->value));
}
static const char *expression_type(Analyzer *a,Node *n,const char *context);
static void statement_check(Analyzer *a,Node *n);
static void assignment_error(Analyzer *a,Node *target,Node *expr,const char *actual) {
    const char *typ=target->kind==N_DECL?decl_type(a,target):expression_type(a,target,"expressão");
    const char *name=target->kind==N_DECL?target->value:source_text(a->ctx,target);
    add_error(a->ctx,"SEM003",expr,format_text(a->ctx,"Não é possível atribuir %s a %s sem conversão permitida (destino “%s”; expressão “%s”).",actual,typ,name,source_text(a->ctx,expr)));
}
static int constant(Node *n,long long *out) {
    if(n->kind==N_LITERAL && equal(n->type_name,"int")) {
        errno=0;char *end=NULL;*out=strtoll(n->value,&end,10);return !errno && end && !*end;
    }
    if(n->kind==N_UNARY && (equal(n->value,"+")||equal(n->value,"-"))) {
        if(!constant(n->right,out))return 0;
        if(equal(n->value,"-")){if(*out==(-9223372036854775807LL-1))return 0;*out=-*out;}
        return 1;
    }
    return 0;
}
static void check_decl(Analyzer *a,Node *n) {
    if(n->array) {
        const char *typ=expression_type(a,n->size,"expressão");long long size=0;
        if(!equal(typ,"error") && (!equal(typ,"int")||!constant(n->size,&size)||size<=0))
            add_error(a->ctx,"SEM006",n->size,format_text(a->ctx,"Tamanho do vetor “%s” deve ser uma constante inteira positiva.",n->value));
    }
    if(n->initializer) {
        const char *actual=expression_type(a,n->initializer,"inicialização");
        if(!equal(actual,"error") && (n->array || !compatible(n->type_name,actual)))assignment_error(a,n,n->initializer,actual);
    }
}
static int guarantees_return(Node *n) {
    if(n->kind==N_RETURN)return 1;
    if(n->kind==N_BLOCK){for(Node *child=n->children;child;child=child->next)if(guarantees_return(child))return 1;return 0;}
    if(n->kind==N_IF)return n->otherwise && guarantees_return(n->body) && guarantees_return(n->otherwise);
    return 0;
}
static void statement_check(Analyzer *a,Node *n) {
    switch(n->kind) {
        case N_BLOCK:
            push_scope(a);for(Node *child=n->children;child;child=child->next)statement_check(a,child);pop_scope(a);break;
        case N_DECL:declare(a,n);check_decl(a,n);break;
        case N_EXPR:expression_type(a,n->left,n->left->kind==N_CALL?"comando":"expressão");break;
        case N_IF:case N_WHILE: {
            const char *typ=expression_type(a,n->left,"condição");
            if(!equal(typ,"bool") && !equal(typ,"error"))
                add_error(a->ctx,"SEM005",n->left,format_text(a->ctx,"Condição de %s deve ter tipo bool; recebeu %s (expressão “%s”).",n->kind==N_IF?"if":"while",typ,source_text(a->ctx,n->left)));
            statement_check(a,n->body);if(n->otherwise)statement_check(a,n->otherwise);break;
        }
        case N_RETURN: {
            if(!a->function){add_error(a->ctx,"SEM009",n,"return fora de uma função.");break;}
            const char *target=a->function->type_name,*actual=n->left?expression_type(a,n->left,"retorno"):"void";
            if(!equal(actual,"error") && (!compatible(target,actual)||(equal(target,"void")&&n->left)))
                add_error(a->ctx,"SEM009",n->left?n->left:n,format_text(a->ctx,"Retorno %s incompatível com o tipo %s da função “%s”; conversão implícita de %s para %s não permitida.",actual,target,a->function->value,actual,target));
            break;
        }
        default:break;
    }
}
static const char *expression_type(Analyzer *a,Node *n,const char *context) {
    switch(n->kind) {
        case N_LITERAL:return n->type_name;
        case N_ID: {
            Symbol *s=lookup(a,n->value);
            if(!s){add_error(a->ctx,"SEM001",n,format_text(a->ctx,"Identificador “%s” não declarado neste escopo.",n->value));return "error";}
            if(s->function){add_error(a->ctx,"SEM004",n,format_text(a->ctx,"“%s” é função e não pode ser usada como variável.",n->value));return "error";}
            return s->typ;
        }
        case N_INDEX: {
            const char *base=expression_type(a,n->left,"expressão"),*index=expression_type(a,n->right,"expressão");
            if(!equal(index,"int")&&!equal(index,"error"))
                add_error(a->ctx,"SEM006",n->right,format_text(a->ctx,"Índice do vetor “%s” deve ser int; recebeu %s (expressão “%s”).",source_text(a->ctx,n->left),index,source_text(a->ctx,n->right)));
            if(equal(base,"error")||equal(index,"error"))return "error";
            if(!array_type(base)){add_error(a->ctx,"SEM006",n->left,format_text(a->ctx,"“%s” não é um vetor.",source_text(a->ctx,n->left)));return "error";}
            return equal(index,"int")?copy_text(a->ctx,base,strlen(base)-2):"error";
        }
        case N_CALL: {
            Symbol *s=lookup(a,n->value);size_t count=0,expected=0;
            for(Node *arg=n->children;arg;arg=arg->next)count++;
            const char **types=allocate(a->ctx,(count?count:1)*sizeof(*types));size_t i=0;
            for(Node *arg=n->children;arg;arg=arg->next)types[i++]=expression_type(a,arg,"argumento");
            if(!s){add_error(a->ctx,"SEM001",n,format_text(a->ctx,"Identificador “%s” não declarado neste escopo.",n->value));return "error";}
            if(!s->function){add_error(a->ctx,"SEM007",n,format_text(a->ctx,"“%s” não é uma função.",n->value));return "error";}
            for(Node *p=s->node->params;p;p=p->next)expected++;
            if(count!=expected)add_error(a->ctx,"SEM007",n,format_text(a->ctx,"“%s” espera %zu argumentos, mas recebeu %zu.",n->value,expected,count));
            Node *arg=n->children,*param=s->node->params;i=0;
            while(arg && param) {
                const char *target=decl_type(a,param);
                if(!equal(types[i],"error") && !compatible(target,types[i]))
                    add_error(a->ctx,"SEM008",arg,format_text(a->ctx,"Argumento %zu de “%s”: esperado %s, recebido %s (expressão “%s”).",i+1,n->value,target,types[i],source_text(a->ctx,arg)));
                arg=arg->next;param=param->next;i++;
            }
            if(equal(s->typ,"void") && !equal(context,"comando")) {
                const char *use=equal(context,"atribuição")?"expressão de atribuição":context;
                add_error(a->ctx,"SEM012",n,format_text(a->ctx,"Função “%s” não produz valor (retorno void) e não pode ser usada como %s.",n->value,use));return "error";
            }
            return s->typ;
        }
        case N_ASSIGN: {
            const char *left=expression_type(a,n->left,"expressão"),*right=expression_type(a,n->right,"atribuição");
            if((n->left->kind!=N_ID && n->left->kind!=N_INDEX)||array_type(left)) {
                const char *description=n->left->kind==N_LITERAL && equal(n->left->type_name,"int")?format_text(a->ctx,"o literal inteiro “%s”",n->left->value):format_text(a->ctx,"“%s”",source_text(a->ctx,n->left));
                add_error(a->ctx,"SEM013",n->left,format_text(a->ctx,"Destino de atribuição não é atribuível; %s não designa uma variável ou elemento de vetor.",description));return "error";
            }
            if(equal(left,"error")||equal(right,"error"))return "error";
            if(!compatible(left,right))assignment_error(a,n->left,n->right,right);
            return left;
        }
        case N_UNARY: {
            const char *typ=expression_type(a,n->right,"expressão");
            if(equal(typ,"error"))return typ;
            int valid=equal(n->value,"!")?equal(typ,"bool"):numeric(typ);
            if(!valid){add_error(a->ctx,"SEM004",n,format_text(a->ctx,"Operador “%s” não aceita operando %s.",n->value,typ));return "error";}
            return typ;
        }
        case N_BINARY: {
            const char *left=expression_type(a,n->left,"expressão"),*right=expression_type(a,n->right,"expressão"),*op=n->value;
            if(equal(left,"error")||equal(right,"error"))return "error";
            int numbers=numeric(left)&&numeric(right),valid=numbers;
            if(equal(op,"%"))valid=equal(left,"int")&&equal(right,"int");
            if(equal(op,"&&")||equal(op,"||"))valid=equal(left,"bool")&&equal(right,"bool");
            if(equal(op,"==")||equal(op,"!="))valid=numbers || (equal(left,right) && (equal(left,"bool")||equal(left,"char")));
            if(!valid){add_error(a->ctx,"SEM004",n,format_text(a->ctx,"Operador “%s” não aceita operandos %s e %s.",op,left,right));return "error";}
            if(equal(op,"<")||equal(op,"<=")||equal(op,">")||equal(op,">=")||equal(op,"==")||equal(op,"!=")||equal(op,"&&")||equal(op,"||"))return "bool";
            return equal(left,"float")||equal(right,"float")?"float":"int";
        }
        default:return "error";
    }
}
void analyze_semantics(Context *ctx,Node *program) {
    Analyzer a={ctx,NULL,NULL};push_scope(&a);
    for(Node *n=program->children;n;n=n->next)if(n->kind==N_DECL||n->kind==N_FUNCTION)declare(&a,n);
    for(Node *n=program->children;n;n=n->next) {
        if(n->kind==N_FUNCTION) {
            a.function=n;push_scope(&a);
            for(Node *p=n->params;p;p=p->next)declare(&a,p);
            for(Node *child=n->body->children;child;child=child->next)statement_check(&a,child);
            if(!equal(n->type_name,"void")&&!guarantees_return(n->body)) {
                const char *hint="há um caminho que alcança o fim do corpo.";Node *last=n->body->children;
                if(last){while(last->next)last=last->next;
                    if(last->kind==N_IF && !last->otherwise && last->left->kind==N_ID)
                        hint=format_text(ctx,"o ramo em que “%s” é falso alcança o fim do corpo.",last->left->value);
                }
                add_error(ctx,"SEM011",n,format_text(ctx,"A função “%s” pode terminar sem retornar %s; %s",n->value,n->type_name,hint));
            }
            pop_scope(&a);a.function=NULL;
        }else if(n->kind==N_DECL)check_decl(&a,n);
        else statement_check(&a,n);
    }
}
