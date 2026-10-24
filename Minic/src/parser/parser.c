#include "ascendente.c.inc"

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
        case N_FOR:fputs("For(",out);print_ast(n->initializer,out);fputs(", ",out);print_ast(n->left,out);fputs(", ",out);print_ast(n->right,out);fputs(", ",out);print_ast(n->body,out);break;
        case N_BREAK:fputs("Break(",out);break;
        case N_CONTINUE:fputs("Continue(",out);break;
        default:fputs("EmptyStmt",out);return;
    }
    fputc(')',out);
}
