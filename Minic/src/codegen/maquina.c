#include "../ir/ir.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <math.h>
#include <setjmp.h>

static int vm_is(const char *a,const char *b){return !strcmp(a,b);}
static int vm_binary(const char *op) {
    const char *ops[]={"ADD","SUB","MUL","DIV","MOD","LT","LE","GT","GE","EQ","NE"};
    for(size_t k=0;k<11;k++)if(vm_is(op,ops[k]))return 1;
    return 0;
}
static void vm_push(IR *out,const char *value,const char *typ){ir_emit(out,"PUSH","-",value,"-",typ);}
static IR *vm_lower(IR *ir) {
    IR *out=allocate(ir->ctx,sizeof(*out));out->ctx=ir->ctx;
    for(size_t k=0;k<ir->count;k++) {
        IRInstruction i=ir->code[k];const char *op=i.op;
        if(vm_is(op,"MOV")||vm_is(op,"CAST")||vm_is(op,"NEG")||vm_is(op,"NOT")) {
            vm_push(out,i.a,i.typ);if(!vm_is(op,"MOV"))ir_emit(out,op,"-","-","-",i.typ);ir_emit(out,"POP",i.d,"-","-","void");
        } else if(vm_binary(op)) {
            vm_push(out,i.a,i.typ);vm_push(out,i.b,i.typ);ir_emit(out,op,"-","-","-",i.typ);ir_emit(out,"POP",i.d,"-","-","void");
        } else if(vm_is(op,"LOAD")) {
            vm_push(out,i.a,format_text(ir->ctx,"%s[]",i.typ));vm_push(out,i.b,"int");ir_emit(out,"LOAD","-","-","-",i.typ);ir_emit(out,"POP",i.d,"-","-","void");
        } else if(vm_is(op,"STORE")) {
            vm_push(out,i.d,format_text(ir->ctx,"%s[]",i.typ));vm_push(out,i.a,"int");vm_push(out,i.b,i.typ);ir_emit(out,"STORE","-","-","-",i.typ);
        } else if(vm_is(op,"PARAM"))vm_push(out,i.a,i.typ);
        else if(vm_is(op,"CALL")) {
            ir_emit(out,"CALL","-",i.a,i.b,i.typ);if(!vm_is(i.d,"-"))ir_emit(out,"POP",i.d,"-","-","void");
        } else if(vm_is(op,"JTRUE")||vm_is(op,"JFALSE")) {
            vm_push(out,i.a,"bool");ir_emit(out,op,i.d,"-","-","void");
        } else if(vm_is(op,"RETURN")) {
            if(!vm_is(i.a,"-"))vm_push(out,i.a,i.typ);
            ir_emit(out,vm_is(i.a,"-")?"RET":"RETVAL","-","-","-","void");
        } else if(vm_is(op,"PRINT")){vm_push(out,i.a,i.typ);ir_emit(out,"PRINT","-","-","-",i.typ);}
        else if(vm_is(op,"READ")){ir_emit(out,"READ","-","-","-",i.typ);ir_emit(out,"POP",i.d,"-","-","void");}
        else ir_emit(out,op,i.d,i.a,i.b,i.typ);
    }
    return out;
}
void print_assembly(IR *ir,FILE *out){fputs(".MINICVM\t1\n",out);print_ir(vm_lower(ir),out);}

typedef struct VMArray VMArray;
typedef struct VMValue {int kind;long long integer;double real;const char *text;VMArray *array;} VMValue;
struct VMArray {size_t size;VMValue *items;};
enum {VM_UNSET,VM_INT,VM_FLOAT,VM_BOOL,VM_CHAR,VM_STRING,VM_ARRAY,VM_VOID};
typedef struct VMSlot {const char *name;VMValue value;struct VMSlot *next;} VMSlot;
typedef struct VM {IR *code;VMSlot *globals;size_t steps;int input_started;jmp_buf trap;} VM;
typedef struct VMFrame {VMSlot *locals;VMValue *stack;size_t top,capacity;} VMFrame;
static void vm_fault(VM *vm,const char *message){fprintf(stderr,"ERRO EXECUCAO: %s\n",message);longjmp(vm->trap,1);}
static VMValue vm_int(long long n){return (VMValue){VM_INT,n,0,NULL,NULL};}
static VMValue vm_real(double n){return (VMValue){VM_FLOAT,0,n,NULL,NULL};}
static VMValue vm_bool(int n){VMValue v=vm_int(n);v.kind=VM_BOOL;return v;}
static void vm_set(VM *vm,VMSlot **slots,const char *name,VMValue value) {
    for(VMSlot *s=*slots;s;s=s->next)if(vm_is(s->name,name)){s->value=value;return;}
    VMSlot *slot=allocate(vm->code->ctx,sizeof(*slot));slot->name=name;slot->value=value;slot->next=*slots;*slots=slot;
}
static const char *vm_unquote(Context *ctx,const char *text) {
    size_t length=strlen(text),j=0;char *out=allocate(ctx,length+1);
    for(size_t k=1;k+1<length;k++) {
        char c=text[k];if(c=='\\' && k+2<length){c=text[++k];if(c=='n')c='\n';else if(c=='t')c='\t';else if(c=='r')c='\r';else if(c=='0')c=0;}
        out[j++]=c;
    }
    return out;
}
static VMValue vm_literal(VM *vm,const char *text) {
    if(vm_is(text,"true")||vm_is(text,"false"))return vm_bool(vm_is(text,"true"));
    if(*text=='\''||*text=='"')return (VMValue){*text=='\''?VM_CHAR:VM_STRING,0,0,vm_unquote(vm->code->ctx,text),NULL};
    errno=0;char *end=NULL;
    if(strchr(text,'.')||strchr(text,'e')||strchr(text,'E')) {
        double n=strtod(text,&end);if(!end || *end || !isfinite(n))vm_fault(vm,"Resultado real não finito.");return vm_real(n);
    }
    long long n=strtoll(text,&end,10);
    if(errno==ERANGE)vm_fault(vm,"Overflow inteiro.");
    if(!end||end==text||*end)vm_fault(vm,"Operando inválido.");
    return vm_int(n);
}
static VMValue vm_get(VM *vm,VMFrame *frame,const char *name) {
    VMSlot *slots=*name=='g'?vm->globals:frame->locals;
    for(VMSlot *s=slots;s;s=s->next)if(vm_is(s->name,name)) {
        if(s->value.kind==VM_UNSET)vm_fault(vm,"Leitura de valor não inicializado.");
        return s->value;
    }
    if(*name=='g'||*name=='v'||(*name=='t'&&!vm_is(name,"true")))vm_fault(vm,"Leitura de valor não inicializado.");
    return vm_literal(vm,name);
}
static void vm_stack_push(VM *vm,VMFrame *frame,VMValue value) {
    if(frame->top>=frame->capacity)vm_fault(vm,"Limite da pilha de operandos excedido.");
    frame->stack[frame->top++]=value;
}
static VMValue vm_pop(VM *vm,VMFrame *frame) {
    if(!frame->top)vm_fault(vm,"Pilha de operandos vazia.");
    return frame->stack[--frame->top];
}
static double vm_number(VMValue value){return value.kind==VM_FLOAT?value.real:(double)value.integer;}
static VMValue vm_operate(VM *vm,const char *op,VMValue a,VMValue b) {
    int an=a.kind==VM_INT||a.kind==VM_FLOAT,bn=b.kind==VM_INT||b.kind==VM_FLOAT;
    if(vm_is(op,"NOT")&&a.kind!=VM_BOOL)vm_fault(vm,"Operandos de execução incompatíveis.");
    if(vm_is(op,"NEG")&&!an)vm_fault(vm,"Operandos de execução incompatíveis.");
    if(vm_binary(op)) {
        int same=(vm_is(op,"EQ")||vm_is(op,"NE"))&&a.kind==b.kind&&(a.kind==VM_CHAR||a.kind==VM_STRING||a.kind==VM_BOOL);
        if(!(an&&bn)&&!same)vm_fault(vm,"Operandos de execução incompatíveis.");
        if(vm_is(op,"MOD")&&(a.kind!=VM_INT||b.kind!=VM_INT))vm_fault(vm,"Operandos de execução incompatíveis.");
    }
    if(vm_is(op,"NOT"))return vm_bool(!a.integer);
    if(vm_is(op,"NEG")) {
        if(a.kind==VM_FLOAT){if(!isfinite(-a.real))vm_fault(vm,"Resultado real não finito.");return vm_real(-a.real);}
        if(a.integer==LLONG_MIN)vm_fault(vm,"Overflow inteiro.");
        return vm_int(-a.integer);
    }
    int comparison=vm_is(op,"LT")||vm_is(op,"LE")||vm_is(op,"GT")||vm_is(op,"GE")||vm_is(op,"EQ")||vm_is(op,"NE");
    if(comparison) {
        int result=0;
        if(a.kind==VM_CHAR || a.kind==VM_STRING)result=strcmp(a.text,b.text);
        else if(a.kind==VM_FLOAT || b.kind==VM_FLOAT){double x=vm_number(a),y=vm_number(b);result=(x>y)-(x<y);}
        else result=(a.integer>b.integer)-(a.integer<b.integer);
        return vm_bool(vm_is(op,"LT")?result<0:vm_is(op,"LE")?result<=0:vm_is(op,"GT")?result>0:vm_is(op,"GE")?result>=0:vm_is(op,"EQ")?result==0:result!=0);
    }
    if(a.kind==VM_FLOAT || b.kind==VM_FLOAT) {
        double x=vm_number(a),y=vm_number(b),result=0;
        if((vm_is(op,"DIV")||vm_is(op,"MOD")) && y==0)vm_fault(vm,"Divisão por zero.");
        if(vm_is(op,"ADD"))result=x+y;else if(vm_is(op,"SUB"))result=x-y;else if(vm_is(op,"MUL"))result=x*y;else result=x/y;
        if(!isfinite(result))vm_fault(vm,"Resultado real não finito.");
        return vm_real(result);
    }
    long long x=a.integer,y=b.integer,result=0;int overflow=0;
    if(vm_is(op,"ADD")){overflow=(y>0&&x>LLONG_MAX-y)||(y<0&&x<LLONG_MIN-y);if(!overflow)result=x+y;}
    else if(vm_is(op,"SUB")){overflow=(y<0&&x>LLONG_MAX+y)||(y>0&&x<LLONG_MIN+y);if(!overflow)result=x-y;}
    else if(vm_is(op,"MUL")) {
        overflow=(x>0&&y>0&&x>LLONG_MAX/y)||(x>0&&y<0&&y<LLONG_MIN/x)||(x<0&&y>0&&x<LLONG_MIN/y)||(x<0&&y<0&&x<LLONG_MAX/y);
        if(!overflow)result=x*y;
    }else {
        if(!y)vm_fault(vm,"Divisão por zero.");
        overflow=x==LLONG_MIN&&y==-1;if(!overflow)result=vm_is(op,"DIV")?x/y:x%y;
    }
    if(overflow)vm_fault(vm,"Overflow inteiro.");
    return vm_int(result);
}
static VMValue vm_call(VM *vm,const char *name,VMValue *args,size_t argc,int depth) {
    if(depth>256)vm_fault(vm,"Limite de recursão excedido.");
    size_t pc=0;while(pc<vm->code->count && !(vm_is(vm->code->code[pc].op,"FUNC")&&vm_is(vm->code->code[pc].d,name)))pc++;
    if(pc==vm->code->count)vm_fault(vm,"Função de destino ausente.");
    size_t start=++pc,position=0;VMFrame frame={0};frame.capacity=vm->code->count+256;frame.stack=allocate(vm->code->ctx,frame.capacity*sizeof(VMValue));
    while(pc<vm->code->count) {
        if(++vm->steps>2000000)vm_fault(vm,"Limite de instruções excedido.");
        IRInstruction i=vm->code->code[pc++];const char *op=i.op;
        if(vm_is(op,"PUSH"))vm_stack_push(vm,&frame,vm_get(vm,&frame,i.a));
        else if(vm_is(op,"POP"))vm_set(vm,*i.d=='g'?&vm->globals:&frame.locals,i.d,vm_pop(vm,&frame));
        else if(vm_is(op,"GLOBAL")||vm_is(op,"LOCAL")) {
            VMValue value={0};int global=vm_is(op,"GLOBAL");
            if(global)value=(vm_is(i.typ,"float")||vm_is(i.typ,"float[]"))?vm_real(0):(vm_is(i.typ,"bool")||vm_is(i.typ,"bool[]"))?vm_bool(0):(vm_is(i.typ,"char")||vm_is(i.typ,"char[]"))?(VMValue){VM_CHAR,0,0,"",NULL}:vm_int(0);
            if(!vm_is(i.a,"-")) {
                long long count=strtoll(i.a,NULL,10);if(count<=0||count>1000000)vm_fault(vm,"Tamanho de vetor excede o limite da VM.");
                VMArray *array=allocate(vm->code->ctx,sizeof(*array));array->size=(size_t)count;array->items=allocate(vm->code->ctx,array->size*sizeof(VMValue));
                for(size_t j=0;j<array->size;j++)array->items[j]=value;
                value=(VMValue){VM_ARRAY,0,0,NULL,array};
            }
            vm_set(vm,global?&vm->globals:&frame.locals,i.d,value);
        } else if(vm_is(op,"ARG")) {
            if(position>=argc)vm_fault(vm,"Argumentos de execução inválidos.");
            vm_set(vm,&frame.locals,i.d,args[position++]);
        } else if(vm_binary(op)) {
            VMValue b=vm_pop(vm,&frame),a=vm_pop(vm,&frame);vm_stack_push(vm,&frame,vm_operate(vm,op,a,b));
        } else if(vm_is(op,"NEG")||vm_is(op,"NOT")) {
            VMValue a=vm_pop(vm,&frame);vm_stack_push(vm,&frame,vm_operate(vm,op,a,vm_int(0)));
        } else if(vm_is(op,"CAST")){VMValue a=vm_pop(vm,&frame);if(a.kind!=VM_INT&&a.kind!=VM_FLOAT)vm_fault(vm,"Operandos de execução incompatíveis.");vm_stack_push(vm,&frame,vm_real(vm_number(a)));}
        else if(vm_is(op,"LOAD")||vm_is(op,"STORE")) {
            VMValue value={0};if(vm_is(op,"STORE"))value=vm_pop(vm,&frame);
            VMValue index=vm_pop(vm,&frame),base=vm_pop(vm,&frame);
            if(base.kind!=VM_ARRAY||index.kind!=VM_INT||index.integer<0||(unsigned long long)index.integer>=base.array->size)vm_fault(vm,"Índice fora dos limites do vetor.");
            if(vm_is(op,"STORE"))base.array->items[index.integer]=value;
            else{value=base.array->items[index.integer];if(value.kind==VM_UNSET)vm_fault(vm,"Leitura de valor não inicializado.");vm_stack_push(vm,&frame,value);}
        } else if(vm_is(op,"CALL")) {
            size_t count=(size_t)strtoul(i.b,NULL,10);if(count>frame.top)vm_fault(vm,"Argumentos de execução inválidos.");
            frame.top-=count;VMValue value=vm_call(vm,i.a,&frame.stack[frame.top],count,depth+1);
            if(!vm_is(i.typ,"void"))vm_stack_push(vm,&frame,value);
        } else if(vm_is(op,"JUMP")||vm_is(op,"JFALSE")||vm_is(op,"JTRUE")) {
            int take=1;if(!vm_is(op,"JUMP")){VMValue value=vm_pop(vm,&frame);take=!!value.integer==vm_is(op,"JTRUE");}
            if(take) {
                size_t target=start;while(target<vm->code->count && !vm_is(vm->code->code[target].op,"END") && !(vm_is(vm->code->code[target].op,"LABEL")&&vm_is(vm->code->code[target].d,i.d)))target++;
                if(target==vm->code->count||vm_is(vm->code->code[target].op,"END"))vm_fault(vm,"Rótulo de execução ausente.");
                pc=target;
            }
        } else if(vm_is(op,"PRINT")) {
            VMValue value=vm_pop(vm,&frame);
            if(vm_is(i.typ,"bool"))puts(value.integer?"true":"false");
            else if(vm_is(i.typ,"float"))printf("%.12g\n",vm_number(value));
            else if(value.kind==VM_CHAR||value.kind==VM_STRING){if(value.kind==VM_CHAR&&!*value.text)fputc(0,stdout);else fputs(value.text,stdout);fputc('\n',stdout);}
            else printf("%lld\n",value.integer);
        } else if(vm_is(op,"READ")) {
            char word[4096];if(scanf("%4095s",word)!=1)vm_fault(vm,"Entrada incompatível com o tipo solicitado.");
            if(!vm->input_started) {
                vm->input_started=1;
                if(!strncmp(word,"\xef\xbb\xbf",3)) {
                    memmove(word,word+3,strlen(word+3)+1);
                    if(!*word&&scanf("%4095s",word)!=1)vm_fault(vm,"Entrada incompatível com o tipo solicitado.");
                }
            }
            VMValue value={0};char *end=NULL;errno=0;
            if(vm_is(i.typ,"int")){value=vm_int(strtoll(word,&end,10));if(errno||end==word||*end)vm_fault(vm,"Entrada incompatível com o tipo solicitado.");}
            else if(vm_is(i.typ,"float")){value=vm_real(strtod(word,&end));if(end==word||*end||!isfinite(value.real)||strpbrk(word,"xXpP"))vm_fault(vm,"Entrada incompatível com o tipo solicitado.");}
            else if(vm_is(i.typ,"bool")) {
                if(!vm_is(word,"true")&&!vm_is(word,"false")&&!vm_is(word,"1")&&!vm_is(word,"0"))vm_fault(vm,"Entrada incompatível com o tipo solicitado.");
                value=vm_bool(vm_is(word,"true")||vm_is(word,"1"));
            } else {
                size_t characters=0;for(size_t j=0;word[j];j++)if(((unsigned char)word[j]&0xc0)!=0x80)characters++;
                if(characters!=1)vm_fault(vm,"Entrada incompatível com o tipo solicitado.");
                value=(VMValue){VM_CHAR,0,0,copy_text(vm->code->ctx,word,strlen(word)),NULL};
            }
            vm_stack_push(vm,&frame,value);
        } else if(vm_is(op,"RETVAL"))return vm_pop(vm,&frame);
        else if(vm_is(op,"RET")||vm_is(op,"END"))return (VMValue){VM_VOID,0,0,NULL,NULL};
        else if(!vm_is(op,"LABEL"))vm_fault(vm,"Instrução de execução desconhecida.");
    }
    vm_fault(vm,"Função sem retorno de execução.");return vm_int(0);
}
int execute_assembly(IR *assembly) {
    VM vm={0};vm.code=assembly;if(setjmp(vm.trap))return 6;
    vm_call(&vm,"$init",NULL,0,0);VMValue result=vm_call(&vm,"main",NULL,0,0);
    return (int)((unsigned long long)result.integer&255);
}
int execute_ir(IR *ir){return execute_assembly(vm_lower(ir));}
IR *read_assembly(Context *ctx) {
    if(strncmp(ctx->source,".MINICVM\t1\n",11))return NULL;
    IR *out=allocate(ctx,sizeof(*out));out->ctx=ctx;char *cursor=ctx->source+11;
    while(*cursor) {
        char *fields[5];fields[0]=cursor;int count=1;
        while(*cursor && *cursor!='\n') {if(*cursor=='\t'){if(count>=5)return NULL;*cursor=0;fields[count++]=cursor+1;}cursor++;}
        if(count!=5)return NULL;
        if(*cursor)*cursor++=0;
        ir_emit(out,fields[0],fields[1],fields[2],fields[3],fields[4]);
    }
    return out;
}
