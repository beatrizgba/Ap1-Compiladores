"""Tradução dirigida pela AST anotada para quádruplas 3AC tipadas."""
from dataclasses import dataclass, replace

@dataclass
class Instruction:
    op:str
    d:str='-'
    a:str='-'
    b:str='-'
    typ:str='void'

    def text(self):return '\t'.join((self.op,self.d,self.a,self.b,self.typ))

def children(n):
    yield from n.children
    yield from n.params
    for field in ('left','right','body','otherwise','size','initializer'):
        child=getattr(n,field)
        if child is not None:yield child

def walk(n):
    yield n
    for child in children(n):yield from walk(child)

def storage(n):return f'{"g" if getattr(n,"global_scope",False) else "v"}{n.line}_{n.column}_{n.value}'

class Generator:
    def __init__(self,program):
        self.program=program;self.code=[];self.temp=0;self.label=0;self.loops=[]
        for n in program.children:
            if n.kind=='decl':n.global_scope=True
    def emit(self,op,d='-',a='-',b='-',typ='void'):
        self.code.append(Instruction(op,d,a,b,typ));return d
    def newtemp(self):self.temp+=1;return f't{self.temp}'
    def newlabel(self):self.label+=1;return f'L{self.label}'
    def value(self,n):
        start=len(self.code)
        result=self.expression(n)
        if n.conversion_to:
            result=self.emit('CAST',self.newtemp(),result,'-',n.conversion_to)
        n.ir_place=result
        n.ir_begin,n.ir_end=start,len(self.code)
        return result
    def expression(self,n):
        kind=n.kind
        if kind=='literal':return n.value.replace('\t','\\t')
        if kind=='id':return self.emit('MOV',self.newtemp(),storage(n.symbol.node),'-',n.semantic_type)
        if kind=='index':
            base,index=self.value(n.left),self.value(n.right)
            return self.emit('LOAD',self.newtemp(),base,index,n.semantic_type)
        if kind=='assign':
            target=self.address(n.left);value=self.value(n.right)
            self.store(target,value,n.left.semantic_type)
            return value
        if kind=='unary':
            value=self.value(n.right)
            if n.value=='+':return value
            return self.emit('NOT' if n.value=='!' else 'NEG',self.newtemp(),value,'-',n.semantic_type)
        if kind=='binary':
            left=self.value(n.left)
            if n.value in {'&&','||'}:
                dest=self.newtemp();end=self.newlabel()
                self.emit('MOV',dest,left,'-','bool')
                self.emit('JFALSE' if n.value=='&&' else 'JTRUE',end,left)
                self.emit('MOV',dest,self.value(n.right),'-','bool');self.emit('LABEL',end)
                return dest
            right=self.value(n.right)
            op={'+':'ADD','-':'SUB','*':'MUL','/':'DIV','%':'MOD','<':'LT','<=':'LE','>':'GT','>=':'GE','==':'EQ','!=':'NE'}[n.value]
            return self.emit(op,self.newtemp(),left,right,n.semantic_type)
        if kind=='call':
            if n.value=='print':self.emit('PRINT','-',self.value(n.children[0]),'-',n.children[0].semantic_type);return '-'
            if n.value=='read':
                target=self.address(n.children[0]);typ=n.children[0].semantic_type
                value=self.emit('READ',self.newtemp(),'-','-',typ);self.store(target,value,typ);return '-'
            args=[(self.value(arg),arg.conversion_to or arg.semantic_type) for arg in n.children]
            for value,typ in args:self.emit('PARAM','-',value,'-',typ)
            return self.emit('CALL',self.newtemp() if n.semantic_type!='void' else '-',n.value,str(len(args)),n.semantic_type)
        raise ValueError('Expressão sem tradução: '+kind)
    def address(self,n):
        start=len(self.code)
        target=(storage(n.symbol.node),None) if n.kind=='id' else (self.value(n.left),self.value(n.right))
        n.ir_place=target[0] if target[1] is None else f'{target[0]}[{target[1]}]'
        n.ir_begin,n.ir_end=start,len(self.code)
        return target
    def store(self,target,value,typ):
        base,index=target
        self.emit('MOV',base,value,'-',typ) if index is None else self.emit('STORE',base,index,value,typ)
    def statement(self,n):
        start=len(self.code);self._statement(n);n.ir_begin,n.ir_end=start,len(self.code)
    def _statement(self,n):
        kind=n.kind
        if kind=='block':
            for child in n.children:self.statement(child)
        elif kind=='decl':
            self.emit('GLOBAL' if getattr(n,'global_scope',False) else 'LOCAL',storage(n),str(n.array_size) if n.array else '-', '-',n.semantic_type)
            if n.initializer:self.emit('MOV',storage(n),self.value(n.initializer),'-',n.type_name)
        elif kind=='expr':self.value(n.left)
        elif kind=='return':self.emit('RETURN','-',self.value(n.left) if n.left else '-','-',n.expected_type)
        elif kind=='if':
            other,end=self.newlabel(),self.newlabel()
            self.emit('JFALSE',other,self.value(n.left));self.statement(n.body);self.emit('JUMP',end)
            self.emit('LABEL',other)
            if n.otherwise:self.statement(n.otherwise)
            self.emit('LABEL',end)
        elif kind in {'while','for'}:
            test,step,end=self.newlabel(),self.newlabel(),self.newlabel()
            if n.initializer:self.value(n.initializer)
            self.emit('LABEL',test)
            if n.left:self.emit('JFALSE',end,self.value(n.left))
            self.loops.append((end,step if kind=='for' else test));self.statement(n.body);self.loops.pop()
            self.emit('LABEL',step)
            if kind=='for' and n.right:self.value(n.right)
            self.emit('JUMP',test);self.emit('LABEL',end)
        elif kind in {'break','continue'}:self.emit('JUMP',self.loops[-1][kind=='continue'])
        elif kind!='empty':raise ValueError('Comando sem tradução: '+kind)
    def generate(self):
        self.emit('FUNC','$init','-','-','void')
        for n in self.program.children:
            if n.kind=='decl':self.emit('GLOBAL',storage(n),str(n.array_size) if n.array else '-','-',n.semantic_type)
        for n in self.program.children:
            if n.kind=='decl' and n.initializer:self.emit('MOV',storage(n),self.value(n.initializer),'-',n.type_name)
        self.emit('RETURN');self.emit('END','$init')
        for n in self.program.children:
            if n.kind!='function':continue
            begin=len(self.code)
            self.temp=self.label=0
            self.emit('FUNC',n.value,'-','-',n.type_name)
            for param in n.params:
                param.ir_begin=len(self.code);self.emit('ARG',storage(param),'-','-',param.semantic_type);param.ir_end=len(self.code)
            self.statement(n.body)
            self.emit('RETURN','-','-' if n.type_name=='void' else '0','-',n.type_name)
            self.emit('END',n.value)
            n.ir_begin,n.ir_end=begin,len(self.code)
        self.program.ir_begin,self.program.ir_end=0,len(self.code)
        return self.code

def render(code):return '\n'.join(i.text() for i in code)+'\n'

def validate(code):
    stack_code=any(i.op in {'PUSH','POP'} for i in code)
    function=None;labels=set();jumps=[];functions=set();calls=[];pending=0
    for i in code:
        if i.op=='FUNC':
            if function is not None or i.d in functions:raise ValueError('Função IR inválida.')
            function=i.d;functions.add(i.d);labels=set();jumps=[]
        elif i.op=='END':
            if function!=i.d or pending or any(label not in labels for label in jumps):raise ValueError('Fluxo IR inválido.')
            function=None
        elif function is None:raise ValueError('Instrução fora de função.')
        elif i.op=='LABEL':
            if i.d in labels:raise ValueError('Rótulo duplicado.')
            labels.add(i.d)
        elif i.op in {'JUMP','JFALSE','JTRUE'}:jumps.append(i.d)
        elif i.op=='PARAM':pending+=1
        elif i.op=='CALL':
            if not stack_code and pending!=int(i.b):raise ValueError('Argumentos IR inválidos.')
            pending=0;calls.append(i.a)
    if function is not None or any(name not in functions for name in calls):raise ValueError('Função IR ausente.')
