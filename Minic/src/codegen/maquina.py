"""Backend MINIC-VM: assembly de pilha, frames, vetores e I/O tipada."""
import ast
import math
import re
import sys
from src.ir.gerador import Instruction, render, validate

MIN,MAX=-(1<<63),(1<<63)-1
UNSET=object()
BINARY={'ADD','SUB','MUL','DIV','MOD','LT','LE','GT','GE','EQ','NE'}

class RuntimeFault(Exception):pass

def lower(code):
    result=[]
    def emit(op,d='-',a='-',b='-',typ='void'):result.append(Instruction(op,d,a,b,typ))
    def push(value,typ):emit('PUSH','-',value,'-',typ)
    for i in code:
        if i.op in {'MOV','CAST','NEG','NOT'}:
            push(i.a,i.typ)
            if i.op!='MOV':emit(i.op,typ=i.typ)
            emit('POP',i.d)
        elif i.op in BINARY:
            push(i.a,i.typ);push(i.b,i.typ);emit(i.op,typ=i.typ);emit('POP',i.d)
        elif i.op=='LOAD':
            push(i.a,i.typ+'[]');push(i.b,'int');emit('LOAD',typ=i.typ);emit('POP',i.d)
        elif i.op=='STORE':
            push(i.d,i.typ+'[]');push(i.a,'int');push(i.b,i.typ);emit('STORE',typ=i.typ)
        elif i.op=='PARAM':push(i.a,i.typ)
        elif i.op=='CALL':
            emit('CALL','-',i.a,i.b,i.typ)
            if i.d!='-':emit('POP',i.d)
        elif i.op in {'JTRUE','JFALSE'}:push(i.a,'bool');emit(i.op,i.d)
        elif i.op=='RETURN':
            if i.a!='-':push(i.a,i.typ)
            emit('RETVAL' if i.a!='-' else 'RET')
        elif i.op=='PRINT':push(i.a,i.typ);emit('PRINT',typ=i.typ)
        elif i.op=='READ':emit('READ',typ=i.typ);emit('POP',i.d)
        else:result.append(Instruction(i.op,i.d,i.a,i.b,i.typ))
    return result

def assembly(code):return '.MINICVM\t1\n'+render(lower(code))

def read_assembly(text):
    lines=text.splitlines()
    if not lines or lines[0]!='.MINICVM\t1':raise ValueError('Cabeçalho MINIC-VM inválido.')
    code=[]
    for line in lines[1:]:
        fields=line.split('\t')
        if len(fields)!=5:raise ValueError('Instrução MINIC-VM inválida.')
        code.append(Instruction(*fields))
    validate(code)
    return code

def literal(text):
    if text=='true':return True
    if text=='false':return False
    if text.startswith(('"',"'")):return ast.literal_eval(text)
    try:return float(text) if any(c in text for c in '.eE') else int(text)
    except ValueError:raise RuntimeFault('Operando inválido: '+text)

def operation(op,a,b=None):
    numeric=lambda x:type(x) in {int,float}
    if op=='NOT' and type(a)!=bool:raise RuntimeFault('Operandos de execução incompatíveis.')
    if op=='NEG' and not numeric(a):raise RuntimeFault('Operandos de execução incompatíveis.')
    if op in BINARY:
        comparison=op in {'LT','LE','GT','GE','EQ','NE'}
        compatible=(numeric(a) and numeric(b)) or (op in {'EQ','NE'} and type(a)==type(b) and type(a) in {str,bool})
        if not compatible or (not comparison and not (numeric(a) and numeric(b))) or (op=='MOD' and (type(a)!=int or type(b)!=int)):
            raise RuntimeFault('Operandos de execução incompatíveis.')
    if op=='NOT':return not a
    if op=='NEG':value=-a
    elif op=='ADD':value=a+b
    elif op=='SUB':value=a-b
    elif op=='MUL':value=a*b
    elif op in {'DIV','MOD'}:
        if b==0:raise RuntimeFault('Divisão por zero.')
        if isinstance(a,int) and isinstance(b,int):
            if a==MIN and b==-1:raise RuntimeFault('Overflow inteiro.')
            quotient=(abs(a)//abs(b))*(-1 if (a<0)!=(b<0) else 1)
            value=quotient if op=='DIV' else a-quotient*b
        else:value=a/b
    elif op=='LT':return a<b
    elif op=='LE':return a<=b
    elif op=='GT':return a>b
    elif op=='GE':return a>=b
    elif op=='EQ':return a==b
    elif op=='NE':return a!=b
    else:raise RuntimeFault('Operação desconhecida: '+op)
    if isinstance(value,int) and not MIN<=value<=MAX:raise RuntimeFault('Overflow inteiro.')
    if isinstance(value,float) and not math.isfinite(value):raise RuntimeFault('Resultado real não finito.')
    return value

def execute(code,input_stream=None,output=None):
    input_stream=sys.stdin if input_stream is None else input_stream
    output=sys.stdout if output is None else output
    functions={};labels={};current=None
    for index,i in enumerate(code):
        if i.op=='FUNC':current=i.d;functions[current]=index+1;labels[current]={}
        elif i.op=='LABEL':labels[current][i.d]=index
    globals_={};steps=0;words=(word for line in input_stream for word in line.split())
    def invoke(name,args,depth):
        nonlocal steps
        if depth>256:raise RuntimeFault('Limite de recursão excedido.')
        if name not in functions:raise RuntimeFault('Função de destino ausente.')
        local={};stack=[];position=0;pc=functions[name]
        def pop():
            if not stack:raise RuntimeFault('Pilha de operandos vazia.')
            return stack.pop()
        def get(text):
            if text.startswith('g') and text in globals_:value=globals_[text]
            elif text in local:value=local[text]
            elif len(text)>1 and text[0] in 'gvt' and text[1].isdigit():raise RuntimeFault('Leitura de valor não inicializado.')
            else:value=literal(text)
            if value is UNSET:raise RuntimeFault('Leitura de valor não inicializado.')
            if isinstance(value,int) and not MIN<=value<=MAX:raise RuntimeFault('Overflow inteiro.')
            if isinstance(value,float) and not math.isfinite(value):raise RuntimeFault('Resultado real não finito.')
            return value
        while pc<len(code):
            steps+=1
            if steps>2000000:raise RuntimeFault('Limite de instruções excedido.')
            i=code[pc];pc+=1;op=i.op
            if op=='PUSH':stack.append(get(i.a))
            elif op=='POP':
                target=globals_ if i.d.startswith('g') else local;target[i.d]=pop()
            elif op in {'LOCAL','GLOBAL'}:
                target=globals_ if op=='GLOBAL' else local
                element=i.typ.removesuffix('[]')
                default=(False if element=='bool' else '\0' if element=='char' else 0.0 if element=='float' else 0) if op=='GLOBAL' else UNSET
                if i.a!='-':
                    size=int(i.a)
                    if not 0<size<=1000000:raise RuntimeFault('Tamanho de vetor excede o limite da VM.')
                    target[i.d]=[default]*size
                else:target[i.d]=default
            elif op=='ARG':
                if position>=len(args):raise RuntimeFault('Argumentos de execução inválidos.')
                local[i.d]=args[position];position+=1
            elif op in BINARY:
                b,a=pop(),pop();stack.append(operation(op,a,b))
            elif op in {'NEG','NOT'}:stack.append(operation(op,pop()))
            elif op=='CAST':
                value=pop()
                if type(value) not in {int,float}:raise RuntimeFault('Operandos de execução incompatíveis.')
                stack.append(float(value))
            elif op in {'LOAD','STORE'}:
                value=pop() if op=='STORE' else None
                index,base=pop(),pop()
                if not isinstance(base,list) or type(index)!=int or not 0<=index<len(base):raise RuntimeFault('Índice fora dos limites do vetor.')
                if op=='STORE':base[index]=value
                else:
                    if base[index] is UNSET:raise RuntimeFault('Leitura de valor não inicializado.')
                    stack.append(base[index])
            elif op=='CALL':
                count=int(i.b)
                if len(stack)<count:raise RuntimeFault('Argumentos de execução inválidos.')
                actual=stack[-count:] if count else []
                if count:del stack[-count:]
                value=invoke(i.a,actual,depth+1)
                if i.typ!='void':stack.append(value)
            elif op in {'JUMP','JFALSE','JTRUE'}:
                take=op=='JUMP' or (bool(pop())==(op=='JTRUE'))
                if take:
                    if i.d not in labels[name]:raise RuntimeFault('Rótulo de execução ausente.')
                    pc=labels[name][i.d]
            elif op=='PRINT':
                value=pop();text=('true' if value else 'false') if i.typ=='bool' else format(value,'.12g') if i.typ=='float' else str(value)
                output.write(text+'\n')
            elif op=='READ':
                try:
                    word=next(words)
                    if i.typ=='int':
                        if not re.fullmatch(r'[+-]?[0-9]+',word):raise ValueError()
                        value=int(word)
                    elif i.typ=='float':
                        if not re.fullmatch(r'[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?',word):raise ValueError()
                        value=float(word)
                    elif i.typ=='bool':
                        if word not in {'true','false','1','0'}:raise ValueError()
                        value=word in {'true','1'}
                    else:
                        if len(word)!=1:raise ValueError()
                        value=word
                    if isinstance(value,int) and not MIN<=value<=MAX:raise ValueError()
                    if isinstance(value,float) and not math.isfinite(value):raise ValueError()
                except (StopIteration,ValueError):raise RuntimeFault('Entrada incompatível com o tipo solicitado.')
                stack.append(value)
            elif op=='RETVAL':return pop()
            elif op in {'RET','END'}:return None
            elif op!='LABEL':raise RuntimeFault('Instrução de execução desconhecida: '+op)
        raise RuntimeFault('Função sem retorno de execução.')
    invoke('$init',[],0)
    result=invoke('main',[],0)
    return int(result or 0)&255
