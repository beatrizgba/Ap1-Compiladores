#include "../ir/ir.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

static int opt_eq(const char *a,const char *b){return !strcmp(a,b);}
static int opt_var(const char *s){return s[0] && (s[0]=='g'||s[0]=='v'||s[0]=='t') && s[1]>='0'&&s[1]<='9';}
static int opt_const(const char *s){return !opt_eq(s,"-")&&!opt_var(s);}
static int opt_bin(const char *op) {
    const char *ops[]={"ADD","SUB","MUL","DIV","MOD","LT","LE","GT","GE","EQ","NE"};
    for(size_t i=0;i<11;i++)if(opt_eq(op,ops[i]))return 1;
    return 0;
}
static const char *opt_def(IRInstruction i) {
    if(opt_eq(i.d,"-"))return NULL;
    return opt_bin(i.op)||opt_eq(i.op,"MOV")||opt_eq(i.op,"CAST")||opt_eq(i.op,"NEG")||opt_eq(i.op,"NOT")||opt_eq(i.op,"READ")||opt_eq(i.op,"CALL")||opt_eq(i.op,"LOCAL")||opt_eq(i.op,"GLOBAL")||opt_eq(i.op,"ARG")||opt_eq(i.op,"LOAD")?i.d:NULL;
}
static int opt_int(const char *s,long long *out) {
    if(opt_eq(s,"true")||opt_eq(s,"false")){*out=opt_eq(s,"true");return 1;}
    errno=0;char *end=NULL;*out=strtoll(s,&end,10);return !errno && end!=s && !*end;
}
static const char *opt_fold(Context *ctx,IRInstruction i,const char *a,const char *b) {
    if(opt_eq(i.op,"MOV"))return opt_const(a)?a:NULL;
    long long x,y=0,result=0;
    if(!opt_int(a,&x) || (opt_bin(i.op)&&!opt_int(b,&y)))return NULL;
    if(opt_eq(i.op,"NOT"))return x?"false":"true";
    if(opt_eq(i.op,"NEG")){if(x==LLONG_MIN)return NULL;return format_text(ctx,"%lld",-x);}
    if(!opt_bin(i.op))return NULL;
    if(opt_eq(i.op,"LT"))return x<y?"true":"false";
    if(opt_eq(i.op,"LE"))return x<=y?"true":"false";
    if(opt_eq(i.op,"GT"))return x>y?"true":"false";
    if(opt_eq(i.op,"GE"))return x>=y?"true":"false";
    if(opt_eq(i.op,"EQ"))return x==y?"true":"false";
    if(opt_eq(i.op,"NE"))return x!=y?"true":"false";
    if(opt_eq(i.op,"ADD")) {
        if((y>0&&x>LLONG_MAX-y)||(y<0&&x<LLONG_MIN-y))return NULL;
        result=x+y;
    } else if(opt_eq(i.op,"SUB")) {
        if((y<0&&x>LLONG_MAX+y)||(y>0&&x<LLONG_MIN+y))return NULL;
        result=x-y;
    } else if(opt_eq(i.op,"MUL")) {
        if((x>0&&y>0&&x>LLONG_MAX/y)||(x>0&&y<0&&y<LLONG_MIN/x)||(x<0&&y>0&&x<LLONG_MIN/y)||(x<0&&y<0&&x<LLONG_MAX/y))return NULL;
        result=x*y;
    } else {
        if(!y||(x==LLONG_MIN&&y==-1))return NULL;
        result=opt_eq(i.op,"DIV")?x/y:x%y;
    }
    return format_text(ctx,"%lld",result);
}
typedef struct OptGraph {size_t n,v;const char **names;int *s1,*s2;IRInstruction *code;Context *ctx;} OptGraph;
static int opt_index(OptGraph *g,const char *name) {
    for(size_t j=0;j<g->v;j++)if(opt_eq(name,g->names[j]))return (int)j;
    return -1;
}
static OptGraph opt_graph(Context *ctx,IRInstruction *code,size_t n) {
    OptGraph g={n,0,NULL,NULL,NULL,code,ctx};g.names=allocate(ctx,(n*3+1)*sizeof(*g.names));
    g.s1=allocate(ctx,(n+1)*sizeof(*g.s1));g.s2=allocate(ctx,(n+1)*sizeof(*g.s2));
    for(size_t k=0;k<n;k++) {
        const char *names[]={code[k].d,code[k].a,code[k].b};
        for(int j=0;j<3;j++)if(opt_var(names[j])&&opt_index(&g,names[j])<0)g.names[g.v++]=names[j];
        IRInstruction i=code[k];g.s1[k]=g.s2[k]=-1;
        int jump=opt_eq(i.op,"JUMP")||opt_eq(i.op,"JTRUE")||opt_eq(i.op,"JFALSE");
        if(jump)for(size_t j=0;j<n;j++)if(opt_eq(code[j].op,"LABEL")&&opt_eq(code[j].d,i.d))g.s1[k]=(int)j;
        if(!opt_eq(i.op,"JUMP")&&!opt_eq(i.op,"RETURN")&&k+1<n){if(g.s1[k]<0)g.s1[k]=(int)k+1;else g.s2[k]=(int)k+1;}
    }
    return g;
}
static const char *opt_lookup(OptGraph *g,const char **state,const char *name) {int index=opt_index(g,name);return index>=0&&state[index]?state[index]:name;}
static int opt_same(const char *a,const char *b){return (!a&&!b)||(a&&b&&opt_eq(a,b));}
static const char **opt_forward(OptGraph *g,int **reachable_out) {
    size_t cells=(g->n+1)*(g->v+1);
    const char **in=allocate(g->ctx,cells*sizeof(*in)),**out=allocate(g->ctx,cells*sizeof(*out));
    const char **state=allocate(g->ctx,(g->v+1)*sizeof(*state)),**result=allocate(g->ctx,(g->v+1)*sizeof(*result));
    int *reach=allocate(g->ctx,(g->n+1)*sizeof(*reach));int changed=1;
    while(changed) {
        changed=0;
        for(size_t k=0;k<g->n;k++) {
            int seen=k==0;memset(state,0,g->v*sizeof(*state));
            for(size_t p=0;p<g->n;p++)if(reach[p]&&(g->s1[p]==(int)k||g->s2[p]==(int)k)) {
                const char **prior=&out[p*g->v];
                if(!seen){memcpy(state,prior,g->v*sizeof(*state));seen=1;}
                else for(size_t j=0;j<g->v;j++)if(!opt_same(state[j],prior[j]))state[j]=NULL;
            }
            if(!seen)continue;
            if(!reach[k]){reach[k]=1;changed=1;}
            memcpy(result,state,g->v*sizeof(*state));IRInstruction i=g->code[k];
            const char *a=opt_lookup(g,state,i.a),*b=opt_lookup(g,state,i.b),*dest=opt_def(i);
            if(opt_eq(i.op,"CALL")||opt_eq(i.op,"STORE")||opt_eq(i.op,"READ"))memset(result,0,g->v*sizeof(*result));
            if(dest){int j=opt_index(g,dest);if(j>=0)result[j]=*dest=='g'?NULL:opt_fold(g->ctx,i,a,b);}
            for(size_t j=0;j<g->v;j++) {
                if(!opt_same(in[k*g->v+j],state[j])||!opt_same(out[k*g->v+j],result[j]))changed=1;
                in[k*g->v+j]=state[j];out[k*g->v+j]=result[j];
            }
        }
    }
    *reachable_out=reach;return in;
}
static unsigned char *opt_initialized(OptGraph *g) {
    size_t cells=(g->n+1)*(g->v+1);
    unsigned char *in=allocate(g->ctx,cells),*out=allocate(g->ctx,cells),*state=allocate(g->ctx,g->v+1);
    memset(in,1,cells);memset(out,1,cells);int changed=1;
    while(changed) {
        changed=0;
        for(size_t k=0;k<g->n;k++) {
            memset(state,k?1:0,g->v);int seen=k==0;
            for(size_t p=0;p<g->n;p++)if(g->s1[p]==(int)k||g->s2[p]==(int)k){seen=1;for(size_t j=0;j<g->v;j++)state[j]&=out[p*g->v+j];}
            if(!seen)memset(state,0,g->v);
            for(size_t j=0;j<g->v;j++){if(in[k*g->v+j]!=state[j])changed=1;in[k*g->v+j]=state[j];}
            const char *dest=opt_def(g->code[k]);int d=dest?opt_index(g,dest):-1;if(d>=0)state[d]=!opt_eq(g->code[k].op,"LOCAL");
            for(size_t j=0;j<g->v;j++){if(out[k*g->v+j]!=state[j])changed=1;out[k*g->v+j]=state[j];}
        }
    }
    return in;
}
static void opt_uses(OptGraph *g,IRInstruction i,unsigned char *set) {
    const char *fields[3]={NULL,NULL,NULL};
    if(opt_bin(i.op)||opt_eq(i.op,"LOAD")){fields[0]=i.a;fields[1]=i.b;}
    else if(opt_eq(i.op,"STORE")){fields[0]=i.d;fields[1]=i.a;fields[2]=i.b;}
    else if(opt_eq(i.op,"MOV")||opt_eq(i.op,"CAST")||opt_eq(i.op,"NEG")||opt_eq(i.op,"NOT")||opt_eq(i.op,"PARAM")||opt_eq(i.op,"PRINT")||opt_eq(i.op,"RETURN")||opt_eq(i.op,"JTRUE")||opt_eq(i.op,"JFALSE"))fields[0]=i.a;
    for(int j=0;j<3;j++)if(fields[j]){int index=opt_index(g,fields[j]);if(index>=0)set[index]=1;}
}
static unsigned char *opt_liveness(OptGraph *g) {
    size_t cells=(g->n+1)*(g->v+1);
    unsigned char *in=allocate(g->ctx,cells),*out=allocate(g->ctx,cells),*state=allocate(g->ctx,g->v+1);int changed=1;
    while(changed) {
        changed=0;
        for(size_t index=g->n;index>0;index--) {
            size_t k=index-1;memset(state,0,g->v);
            for(size_t j=0;j<g->v;j++) {
                if(g->s1[k]>=0)state[j]|=in[(size_t)g->s1[k]*g->v+j];
                if(g->s2[k]>=0)state[j]|=in[(size_t)g->s2[k]*g->v+j];
                if(out[k*g->v+j]!=state[j])changed=1;
                out[k*g->v+j]=state[j];
            }
            const char *dest=opt_def(g->code[k]);int d=dest?opt_index(g,dest):-1;if(d>=0)state[d]=0;
            opt_uses(g,g->code[k],state);
            for(size_t j=0;j<g->v;j++){if(in[k*g->v+j]!=state[j])changed=1;in[k*g->v+j]=state[j];}
        }
    }
    return out;
}
static size_t opt_function(Context *ctx,IRInstruction *code,size_t n) {
    for(int iteration=0;iteration<8;iteration++) {
        size_t original_n=n;IRInstruction *original=allocate(ctx,(n+1)*sizeof(*original));memcpy(original,code,n*sizeof(*code));
        OptGraph g=opt_graph(ctx,code,n);int *reachable=NULL;const char **states=opt_forward(&g,&reachable);size_t used=0;
        for(size_t k=0;k<n;k++) {
            if(!reachable[k])continue;
            IRInstruction i=code[k];const char **state=&states[k*g.v];
            if(opt_bin(i.op)||opt_eq(i.op,"LOAD")||opt_eq(i.op,"STORE")){i.a=opt_lookup(&g,state,i.a);i.b=opt_lookup(&g,state,i.b);}
            else if(opt_eq(i.op,"MOV")||opt_eq(i.op,"CAST")||opt_eq(i.op,"NEG")||opt_eq(i.op,"NOT")||opt_eq(i.op,"PARAM")||opt_eq(i.op,"PRINT")||opt_eq(i.op,"RETURN")||opt_eq(i.op,"JTRUE")||opt_eq(i.op,"JFALSE"))i.a=opt_lookup(&g,state,i.a);
            const char *value=opt_fold(ctx,i,i.a,i.b);if(value){i.op="MOV";i.a=value;i.b="-";}
            if(opt_eq(i.typ,"int")) {
                if(((opt_eq(i.op,"ADD")||opt_eq(i.op,"SUB"))&&opt_eq(i.b,"0"))||(opt_eq(i.op,"MUL")&&opt_eq(i.b,"1"))){i.op="MOV";i.b="-";}
                else if((opt_eq(i.op,"ADD")&&opt_eq(i.a,"0"))||(opt_eq(i.op,"MUL")&&opt_eq(i.a,"1"))){i.op="MOV";i.a=i.b;i.b="-";}
            }
            if((opt_eq(i.op,"JTRUE")||opt_eq(i.op,"JFALSE"))&&(opt_eq(i.a,"true")||opt_eq(i.a,"false"))) {
                if(opt_eq(i.a,"true")==opt_eq(i.op,"JTRUE")){i.op="JUMP";i.a="-";}else continue;
            }
            code[used++]=i;
        }
        n=used;
        IRInstruction *available=allocate(ctx,(n+1)*sizeof(*available)),*aliases=allocate(ctx,(n+1)*sizeof(*aliases));size_t acount=0,alias_count=0;
        for(size_t k=0;k<n;k++) {
            IRInstruction i=code[k];
            int leader=k==0||opt_eq(i.op,"LABEL")||opt_eq(code[k?k-1:0].op,"JUMP")||opt_eq(code[k?k-1:0].op,"JTRUE")||opt_eq(code[k?k-1:0].op,"JFALSE")||opt_eq(code[k?k-1:0].op,"RETURN");
            if(leader||opt_eq(i.op,"CALL")||opt_eq(i.op,"READ")||opt_eq(i.op,"STORE")){acount=0;alias_count=0;}
            const char *dest=opt_def(i);size_t kept=0;
            for(size_t j=0;j<acount;j++)if(!dest||(!opt_eq(available[j].a,dest)&&!opt_eq(available[j].b,dest)&&!opt_eq(available[j].d,dest)))available[kept++]=available[j];
            acount=kept;
            kept=0;
            for(size_t j=0;j<alias_count;j++)if(!dest||(!opt_eq(aliases[j].d,dest)&&!opt_eq(aliases[j].a,dest)))aliases[kept++]=aliases[j];
            alias_count=kept;
            if(opt_bin(i.op)||opt_eq(i.op,"NEG")||opt_eq(i.op,"NOT")||opt_eq(i.op,"CAST")) {
                IRInstruction key=i;
                for(size_t j=0;j<alias_count;j++){if(opt_eq(aliases[j].d,i.a))key.a=aliases[j].a;if(opt_eq(aliases[j].d,i.b))key.b=aliases[j].a;}
                int found=0;
                for(size_t j=0;j<acount;j++)if(opt_eq(available[j].op,key.op)&&opt_eq(available[j].a,key.a)&&opt_eq(available[j].b,key.b)&&opt_eq(available[j].typ,key.typ)) {
                    code[k].op="MOV";code[k].a=available[j].d;code[k].b="-";found=1;break;
                }
                if(!found)available[acount++]=key;
            }
            if(opt_eq(code[k].op,"MOV")&&!opt_eq(code[k].d,code[k].a)) {
                const char *value=code[k].a;for(size_t j=0;j<alias_count;j++)if(opt_eq(aliases[j].d,value)){value=aliases[j].a;break;}
                aliases[alias_count++]=(IRInstruction){"MOV",code[k].d,value,"-",code[k].typ};
            }
        }
        g=opt_graph(ctx,code,n);unsigned char *initialized=opt_initialized(&g),*live=opt_liveness(&g);used=0;
        for(size_t k=0;k<n;k++) {
            IRInstruction i=code[k];int d=opt_index(&g,i.d),a=opt_index(&g,i.a);
            int safe=opt_const(i.a)||(a>=0&&initialized[k*g.v+(size_t)a]);
            int removable=(opt_eq(i.op,"MOV")||opt_eq(i.op,"CAST")||opt_eq(i.op,"NOT"))&&*i.d!='g'&&d>=0&&!live[k*g.v+(size_t)d]&&safe;
            if(!removable)code[used++]=i;
        }
        n=used;int same=n==original_n;
        for(size_t k=0;same&&k<n;k++)if(!opt_eq(code[k].op,original[k].op)||!opt_eq(code[k].d,original[k].d)||!opt_eq(code[k].a,original[k].a)||!opt_eq(code[k].b,original[k].b)||!opt_eq(code[k].typ,original[k].typ))same=0;
        if(same)break;
    }
    return n;
}
void optimize_ir(IR *ir) {
    IR *out=allocate(ir->ctx,sizeof(*out));out->ctx=ir->ctx;size_t start=0;
    for(size_t k=0;k<ir->count;k++) {
        IRInstruction i=ir->code[k];
        if(opt_eq(i.op,"FUNC"))start=k;
        else if(opt_eq(i.op,"END")) {
            IRInstruction first=ir->code[start];ir_emit(out,first.op,first.d,first.a,first.b,first.typ);
            size_t count=opt_function(ir->ctx,&ir->code[start+1],k-start-1);
            for(size_t j=0;j<count;j++){IRInstruction x=ir->code[start+1+j];ir_emit(out,x.op,x.d,x.a,x.b,x.typ);}
            ir_emit(out,i.op,i.d,i.a,i.b,i.typ);
        }
    }
    ir->code=out->code;ir->count=out->count;ir->capacity=out->capacity;
}
static void opt_graph_text(FILE *out,const char *text) {
    for(const char *p=text;*p;p++){if(*p=='\\'||*p=='"')fputc('\\',out);fputc(*p,out);}
}
void print_cfg(IR *ir,FILE *out) {
    size_t start=0;fputs("digraph CFG {\n",out);
    for(size_t end=0;end<ir->count;end++) {
        if(opt_eq(ir->code[end].op,"FUNC"))start=end;
        else if(opt_eq(ir->code[end].op,"END")) {
            OptGraph g=opt_graph(ir->ctx,&ir->code[start+1],end-start-1);if(!g.n)continue;
            unsigned char *leader=allocate(ir->ctx,g.n+1);leader[0]=1;
            for(size_t k=0;k<g.n;k++) {
                const char *op=g.code[k].op;if(opt_eq(op,"LABEL"))leader[k]=1;
                if(k+1<g.n&&(opt_eq(op,"JUMP")||opt_eq(op,"JTRUE")||opt_eq(op,"JFALSE")||opt_eq(op,"RETURN")))leader[k+1]=1;
            }
            size_t *begins=allocate(ir->ctx,(g.n+1)*sizeof(*begins)),*owner=allocate(ir->ctx,(g.n+1)*sizeof(*owner)),blocks=0;
            for(size_t k=0;k<g.n;k++){if(leader[k])begins[blocks++]=k;owner[k]=blocks-1;}begins[blocks]=g.n;
            unsigned char *succ=allocate(ir->ctx,blocks*blocks),*dom=allocate(ir->ctx,blocks*blocks),*reachable=allocate(ir->ctx,blocks),*state=allocate(ir->ctx,blocks);reachable[0]=1;
            for(size_t b=0;b<blocks;b++) {
                size_t k=begins[b+1]-1;if(g.s1[k]>=0)succ[b*blocks+owner[g.s1[k]]]=1;if(g.s2[k]>=0)succ[b*blocks+owner[g.s2[k]]]=1;
            }
            int changed=1;
            while(changed){changed=0;for(size_t a=0;a<blocks;a++)if(reachable[a])for(size_t b=0;b<blocks;b++)if(succ[a*blocks+b]&&!reachable[b]){reachable[b]=1;changed=1;}}
            for(size_t b=0;b<blocks;b++)if(reachable[b])for(size_t j=0;j<blocks;j++)dom[b*blocks+j]=b?reachable[j]:j==0;
            changed=1;
            while(changed) {
                changed=0;
                for(size_t b=1;b<blocks;b++)if(reachable[b]) {
                    memcpy(state,reachable,blocks);
                    for(size_t p=0;p<blocks;p++)if(reachable[p]&&succ[p*blocks+b])for(size_t j=0;j<blocks;j++)state[j]&=dom[p*blocks+j];
                    state[b]=1;
                    for(size_t j=0;j<blocks;j++){if(dom[b*blocks+j]!=state[j])changed=1;dom[b*blocks+j]=state[j];}
                }
            }
            for(size_t b=0;b<blocks;b++) {
                fprintf(out,"  f%zub%zu [shape=box,label=\"B%zu dom=",start,b,b);int first=1;
                for(size_t j=0;j<blocks;j++)if(dom[b*blocks+j]){fprintf(out,"%s%zu",first?"":",",j);first=0;}
                fputs("\\l",out);
                for(size_t k=begins[b];k<begins[b+1];k++){IRInstruction i=g.code[k];opt_graph_text(out,format_text(ir->ctx,"%s %s %s %s %s",i.op,i.d,i.a,i.b,i.typ));fputs("\\l",out);}
                fputs("\"];\n",out);
                for(size_t j=0;j<blocks;j++)if(succ[b*blocks+j])fprintf(out,"  f%zub%zu -> f%zub%zu;\n",start,b,start,j);
            }
            for(size_t tail=0;tail<blocks;tail++)for(size_t head=0;head<blocks;head++)if(succ[tail*blocks+head]&&dom[tail*blocks+head]) {
                memset(state,0,blocks);state[head]=state[tail]=1;changed=tail!=head;
                while(changed){changed=0;for(size_t node=0;node<blocks;node++)if(state[node]&&node!=head)for(size_t p=0;p<blocks;p++)if(reachable[p]&&succ[p*blocks+node]&&!state[p]){state[p]=1;changed=1;}}
                fprintf(out,"  // laco f%zu cabecalho=B%zu cauda=B%zu corpo=",start,head,tail);int first=1;
                for(size_t j=0;j<blocks;j++)if(state[j]){fprintf(out,"%s%zu",first?"":",",j);first=0;}
                fputc('\n',out);
            }
        }
    }
    fputs("}\n",out);
}
