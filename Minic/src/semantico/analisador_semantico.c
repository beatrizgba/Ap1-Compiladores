#include "../common.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <math.h>

typedef struct Scope { Symbol *symbols; struct Scope *parent; } Scope;
typedef struct Analyzer { Context *ctx; Scope *scope; Node *function; int loop_depth; } Analyzer;
static int equal(const char *a,const char *b) { return !strcmp(a,b); }
static int numeric(const char *t) { return equal(t,"int")||equal(t,"float"); }
static int array_type(const char *t) { size_t n=strlen(t);return n>=2 && !strcmp(t+n-2,"[]"); }
static const char *decl_type(Analyzer *a,Node *n) { return n->array?format_text(a->ctx,"%s[]",n->type_name):n->type_name; }
static int compatible(const char *target,const char *actual) { return equal(target,actual)||(equal(target,"float")&&equal(actual,"int")); }
static int constant(Node *n,long long *out);
static int numeric_constant(Node *n,double *out);
static void promote(Node *n,const char *target,const char *actual) {
    if(equal(target,"float")&&equal(actual,"int"))n->conversion_to="float";
}
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
        n->symbol=s;
    }
    n->semantic_type=decl_type(a,n);
    n->category=n->kind==N_FUNCTION?"function":n->array?"array":n->kind==N_PARAM?"param":"decl";
    n->assignable=!n->array && n->kind!=N_FUNCTION && !equal(n->type_name,"void");
    long long size=0;
    if(n->array && n->size && constant(n->size,&size) && size>0){n->array_size=size;n->has_array_size=1;}
    if(equal(n->type_name,"void") && n->kind!=N_FUNCTION) {
        n->semantic_type="error";
        add_error(a->ctx,"SEM004",n,format_text(a->ctx,"“%s” não pode ter tipo void.",n->value));
    }
}
static const char *expression_type(Analyzer *a,Node *n,const char *context);
static const char *expression_type_impl(Analyzer *a,Node *n,const char *context);
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
    if(n->kind==N_BINARY) {
        long long left,right;
        if(!constant(n->left,&left)||!constant(n->right,&right))return 0;
        if(equal(n->value,"+")) {
            if((right>0 && left>LLONG_MAX-right)||(right<0 && left<LLONG_MIN-right))return 0;
            *out=left+right;
        }else if(equal(n->value,"-")) {
            if((right<0 && left>LLONG_MAX+right)||(right>0 && left<LLONG_MIN+right))return 0;
            *out=left-right;
        }else if(equal(n->value,"*")) {
            if((left>0 && right>0 && left>LLONG_MAX/right)||
               (left>0 && right<0 && right<LLONG_MIN/left)||
               (left<0 && right>0 && left<LLONG_MIN/right)||
               (left<0 && right<0 && left<LLONG_MAX/right))return 0;
            *out=left*right;
        }else if(equal(n->value,"/")||equal(n->value,"%")) {
            if(!right || (left==LLONG_MIN && right==-1))return 0;
            *out=equal(n->value,"/")?left/right:left%right;
        }else return 0;
        return 1;
    }
    return 0;
}
static int numeric_constant(Node *n,double *out) {
    if(equal(n->semantic_type,"int")){long long value;if(!constant(n,&value))return 0;*out=(double)value;return 1;}
    if(n->kind==N_LITERAL && equal(n->type_name,"float")){*out=strtod(n->value,NULL);return 1;}
    if(n->kind==N_UNARY && (equal(n->value,"+")||equal(n->value,"-"))) {
        if(!numeric_constant(n->right,out))return 0;
        if(equal(n->value,"-"))*out=-*out;
        return 1;
    }
    if(n->kind==N_BINARY) {
        double x,y;if(!numeric_constant(n->left,&x)||!numeric_constant(n->right,&y))return 0;
        if(equal(n->value,"+"))*out=x+y;
        else if(equal(n->value,"-"))*out=x-y;
        else if(equal(n->value,"*"))*out=x*y;
        else if(equal(n->value,"/") && y!=0)*out=x/y;
        else return 0;
        return 1;
    }
    return 0;
}
static void check_decl(Analyzer *a,Node *n) {
    if(n->array) {
        n->size->expected_type="int";
        const char *typ=expression_type(a,n->size,"expressão");long long size=0;
        if(!equal(typ,"error") && (!equal(typ,"int")||!constant(n->size,&size)||size<=0))
            add_error(a->ctx,"SEM006",n->size,format_text(a->ctx,"Tamanho do vetor “%s” deve ser uma constante inteira positiva.",n->value));
    }
    if(n->initializer) {
        n->initializer->expected_type=n->type_name;
        const char *actual=expression_type(a,n->initializer,"inicialização");
        if(!equal(actual,"error") && (n->array || !compatible(n->type_name,actual)))assignment_error(a,n,n->initializer,actual);
        else if(!equal(actual,"error"))promote(n->initializer,n->type_name,actual);
    }
}
static int guarantees_return(Node *n) {
    return n->flow==FLOW_RETURN;
}
static void block_check(Analyzer *a,Node *n,int new_scope) {
    n->semantic_type="void";n->category="command";n->loop_depth=a->loop_depth;n->flow=FLOW_NORMAL;
    if(new_scope)push_scope(a);
    for(Node *child=n->children;child;child=child->next) {
        statement_check(a,child);
        if(n->flow & FLOW_NORMAL)n->flow=(n->flow & ~FLOW_NORMAL)|child->flow;
    }
    if(new_scope)pop_scope(a);
}
static void statement_check(Analyzer *a,Node *n) {
    n->semantic_type="void";n->category="command";n->loop_depth=a->loop_depth;n->flow=FLOW_NORMAL;
    switch(n->kind) {
        case N_BLOCK:
            block_check(a,n,1);break;
        case N_DECL:declare(a,n);check_decl(a,n);break;
        case N_EXPR:expression_type(a,n->left,"comando");break;
        case N_IF:case N_WHILE:case N_FOR: {
            if(n->kind==N_FOR && n->initializer)expression_type(a,n->initializer,"comando");
            if(n->left)n->left->expected_type="bool";
            const char *typ=n->left?expression_type(a,n->left,"condição"):"bool";
            if(!equal(typ,"bool") && !equal(typ,"error"))
                add_error(a->ctx,"SEM005",n->left,format_text(a->ctx,"Condição de %s deve ter tipo bool; recebeu %s (expressão “%s”).",n->kind==N_IF?"if":n->kind==N_FOR?"for":"while",typ,source_text(a->ctx,n->left)));
            if(n->kind!=N_IF)a->loop_depth++;
            statement_check(a,n->body);
            if(n->kind==N_FOR && n->right)expression_type(a,n->right,"comando");
            if(n->kind!=N_IF)a->loop_depth--;
            if(n->otherwise)statement_check(a,n->otherwise);
            n->flow=n->kind!=N_IF?(FLOW_NORMAL|(n->body->flow & FLOW_RETURN)):
                (n->body->flow|(n->otherwise?n->otherwise->flow:FLOW_NORMAL));
            break;
        }
        case N_BREAK:case N_CONTINUE:
            if(!a->loop_depth)add_error(a->ctx,"SEM010",n,format_text(a->ctx,"%s só pode ocorrer dentro de um laço.",n->kind==N_BREAK?"break":"continue"));
            else n->flow=n->kind==N_BREAK?FLOW_BREAK:FLOW_CONTINUE;
            break;
        case N_RETURN: {
            n->flow=FLOW_RETURN;
            if(!a->function){
                n->semantic_type="error";
                if(n->left)expression_type(a,n->left,"retorno");
                add_error(a->ctx,"SEM009",n,"return fora de uma função.");break;
            }
            const char *target=a->function->type_name,*actual=n->left?expression_type(a,n->left,"retorno"):"void";
            n->expected_type=target;if(n->left)n->left->expected_type=target;
            if(!equal(actual,"error") && (!compatible(target,actual)||(equal(target,"void")&&n->left)))
                add_error(a->ctx,"SEM009",n->left?n->left:n,format_text(a->ctx,"Retorno %s incompatível com o tipo %s da função “%s”; conversão implícita de %s para %s não permitida.",actual,target,a->function->value,actual,target));
            else if(n->left && !equal(actual,"error"))promote(n->left,target,actual);
            break;
        }
        default:break;
    }
}
static const char *expression_type(Analyzer *a,Node *n,const char *context) {
    n->loop_depth=a->loop_depth;
    const char *result=expression_type_impl(a,n,context);n->semantic_type=result;
    if(!*n->category)n->category="value";
    if(equal(result,"error")){n->category="error";n->assignable=0;}
    return result;
}
static const char *expression_type_impl(Analyzer *a,Node *n,const char *context) {
    switch(n->kind) {
        case N_LITERAL:
            n->category="literal";
            if(a->ctx->final_mode && equal(n->type_name,"int")) {
                errno=0;strtoll(n->value,NULL,10);
                if(errno==ERANGE){add_error(a->ctx,"SEM016",n,"Literal inteiro fora do intervalo de 64 bits.");return "error";}
            }
            if(a->ctx->final_mode && equal(n->type_name,"float") && !isfinite(strtod(n->value,NULL))) {
                add_error(a->ctx,"SEM016",n,"Literal real não finito.");return "error";
            }
            return n->type_name;
        case N_ID: {
            Symbol *s=lookup(a,n->value);
            if(!s){add_error(a->ctx,"SEM001",n,format_text(a->ctx,"Identificador “%s” não declarado neste escopo.",n->value));return "error";}
            n->symbol=s;n->category=s->node->category;n->array_size=s->node->array_size;n->has_array_size=s->node->has_array_size;n->assignable=s->node->assignable;
            if(s->function){add_error(a->ctx,"SEM004",n,format_text(a->ctx,"“%s” é função e não pode ser usada como variável.",n->value));return "error";}
            return s->typ;
        }
        case N_INDEX: {
            const char *base=expression_type(a,n->left,"expressão"),*index=expression_type(a,n->right,"expressão");
            if(!equal(index,"int")&&!equal(index,"error"))
                add_error(a->ctx,"SEM006",n->right,format_text(a->ctx,"Índice do vetor “%s” deve ser int; recebeu %s (expressão “%s”).",source_text(a->ctx,n->left),index,source_text(a->ctx,n->right)));
            if(equal(base,"error")||equal(index,"error"))return "error";
            if(!array_type(base)){add_error(a->ctx,"SEM006",n->left,format_text(a->ctx,"“%s” não é um vetor.",source_text(a->ctx,n->left)));return "error";}
            if(!equal(index,"int"))return "error";
            n->symbol=n->left->symbol;n->category="element";n->assignable=1;
            long long value=0;
            if(constant(n->right,&value) && (value<0 || (n->left->has_array_size && value>=n->left->array_size))) {
                const char *size=n->left->has_array_size?format_text(a->ctx,"%lld",n->left->array_size):"desconhecido";
                add_error(a->ctx,"SEM006",n->right,format_text(a->ctx,"Índice %lld fora dos limites do vetor “%s” (tamanho %s).",value,source_text(a->ctx,n->left),size));return "error";
            }
            return copy_text(a->ctx,base,strlen(base)-2);
        }
        case N_CALL: {
            if(equal(n->value,"print")||equal(n->value,"read")) {
                size_t total=0;const char *typ="error";
                for(Node *arg=n->children;arg;arg=arg->next){typ=expression_type(a,arg,"argumento");total++;}
                n->category="builtin";
                if(total!=1)add_error(a->ctx,"SEM007",n,format_text(a->ctx,"“%s” espera 1 argumento, mas recebeu %zu.",n->value,total));
                else if(!equal(typ,"error")) {
                    Node *arg=n->children;
                    if(equal(n->value,"read") && (!arg->assignable || !(numeric(typ)||equal(typ,"bool")||equal(typ,"char"))))
                        add_error(a->ctx,"SEM013",arg,"read exige uma variável ou elemento de vetor gravável.");
                    if(equal(n->value,"print") && !(numeric(typ)||equal(typ,"bool")||equal(typ,"char")||equal(typ,"string")))
                        add_error(a->ctx,"SEM004",arg,"print exige um valor escalar ou literal de texto.");
                }
                if(!equal(context,"comando")){add_error(a->ctx,"SEM012",n,format_text(a->ctx,"“%s” não produz valor e só pode ser usado como comando.",n->value));return "error";}
                return "void";
            }
            Symbol *s=lookup(a,n->value);size_t count=0,expected=0;
            for(Node *arg=n->children;arg;arg=arg->next)count++;
            const char **types=allocate(a->ctx,(count?count:1)*sizeof(*types));size_t i=0;
            for(Node *arg=n->children;arg;arg=arg->next)types[i++]=expression_type(a,arg,"argumento");
            if(!s){add_error(a->ctx,"SEM001",n,format_text(a->ctx,"Identificador “%s” não declarado neste escopo.",n->value));return "error";}
            if(!s->function){add_error(a->ctx,"SEM007",n,format_text(a->ctx,"“%s” não é uma função.",n->value));return "error";}
            n->symbol=s;n->category="call";
            for(Node *p=s->node->params;p;p=p->next)expected++;
            if(count!=expected)add_error(a->ctx,"SEM007",n,format_text(a->ctx,"“%s” espera %zu argumentos, mas recebeu %zu.",n->value,expected,count));
            Node *arg=n->children,*param=s->node->params;i=0;
            while(arg && param) {
                const char *target=decl_type(a,param);
                arg->expected_type=target;
                if(!equal(types[i],"error") && !compatible(target,types[i]))
                    add_error(a->ctx,"SEM008",arg,format_text(a->ctx,"Argumento %zu de “%s”: esperado %s, recebido %s (expressão “%s”).",i+1,n->value,target,types[i],source_text(a->ctx,arg)));
                else if(!equal(types[i],"error"))promote(arg,target,types[i]);
                arg=arg->next;param=param->next;i++;
            }
            if(equal(s->typ,"void") && !equal(context,"comando")) {
                const char *use=equal(context,"atribuição")?"expressão de atribuição":context;
                add_error(a->ctx,"SEM012",n,format_text(a->ctx,"Função “%s” não produz valor (retorno void) e não pode ser usada como %s.",n->value,use));return "error";
            }
            return s->typ;
        }
        case N_ASSIGN: {
            const char *right_context=n->right->kind==N_ASSIGN && (equal(context,"comando")||equal(context,"cadeia"))?"cadeia":"atribuição";
            const char *left=expression_type(a,n->left,"expressão"),*right=expression_type(a,n->right,right_context);
            if((n->left->kind!=N_ID && n->left->kind!=N_INDEX)||array_type(left)) {
                const char *description=n->left->kind==N_LITERAL && equal(n->left->type_name,"int")?format_text(a->ctx,"o literal inteiro “%s”",n->left->value):format_text(a->ctx,"“%s”",source_text(a->ctx,n->left));
                add_error(a->ctx,"SEM013",n->left,format_text(a->ctx,"Destino de atribuição não é atribuível; %s não designa uma variável ou elemento de vetor.",description));return "error";
            }
            if(equal(left,"error")||equal(right,"error"))return "error";
            n->expected_type=n->right->expected_type=left;
            if(!compatible(left,right)){assignment_error(a,n->left,n->right,right);return "error";}
            promote(n->right,left,right);
            if(!equal(context,"comando")&&!equal(context,"cadeia")) {
                add_error(a->ctx,"SEM004",n,format_text(a->ctx,"Atribuição não produz valor e não pode ser usada como %s.",context));return "error";
            }
            n->category="command";return equal(context,"cadeia")?left:"void";
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
            double divisor=0;
            if(a->ctx->final_mode && (equal(op,"/")||equal(op,"%")) && numeric_constant(n->right,&divisor) && divisor==0) {
                add_error(a->ctx,"SEM015",n->right,"Divisor constante zero não permitido.");return "error";
            }
            if(numbers && (equal(left,"float")||equal(right,"float"))) {
                promote(n->left,"float",left);promote(n->right,"float",right);
            }
            if(equal(op,"<")||equal(op,"<=")||equal(op,">")||equal(op,">=")||equal(op,"==")||equal(op,"!=")||equal(op,"&&")||equal(op,"||"))return "bool";
            return equal(left,"float")||equal(right,"float")?"float":"int";
        }
        default:return "error";
    }
}
void analyze_semantics(Context *ctx,Node *program) {
    Analyzer a={ctx,NULL,NULL,0};push_scope(&a);
    for(Node *n=program->children;n;n=n->next)if(n->kind==N_DECL||n->kind==N_FUNCTION)declare(&a,n);
    if(ctx->final_mode) {
        int mains=0;Node *main_node=NULL;
        for(Node *n=program->children;n;n=n->next) {
            if(n->kind==N_FUNCTION && equal(n->value,"main")){main_node=n;mains++;}
        }
        if(mains!=1 || !equal(main_node->type_name,"int") || main_node->params)
            add_error(ctx,"SEM014",program,"O programa deve definir exatamente uma função int main() sem parâmetros.");
        for(Node *n=program->children;n;n=n->next)if(n->kind!=N_FUNCTION && n->kind!=N_DECL)add_error(ctx,"SEM014",n,"Comando fora de uma função.");
    }
    for(Node *n=program->children;n;n=n->next) {
        if(n->kind==N_FUNCTION) {
            a.function=n;push_scope(&a);
            for(Node *p=n->params;p;p=p->next)declare(&a,p);
            block_check(&a,n->body,0);
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
    program->semantic_type=ctx->error_count?"error":"void";program->category="program";
}
