"""CFG, ponto fixo forward, CSE local e liveness backward conservadora."""
from dataclasses import replace
from src.codegen.maquina import operation, RuntimeFault, literal, BINARY

def variable(value):return len(value)>1 and value[0] in 'gvt' and value[1].isdigit()
def constant(value):return value!='-' and not variable(value)
def definition(i):
    return i.d if i.op in BINARY|{'MOV','CAST','NEG','NOT','READ','CALL','LOCAL','GLOBAL','ARG','LOAD'} and i.d!='-' else None
def uses(i):
    fields=([i.a,i.b] if i.op in BINARY|{'LOAD'} else
            [i.d,i.a,i.b] if i.op=='STORE' else
            [i.a] if i.op in {'MOV','CAST','NEG','NOT','PARAM','PRINT','RETURN','JTRUE','JFALSE'} else [])
    return {x for x in fields if variable(x)}
def edges(code):
    labels={i.d:k for k,i in enumerate(code) if i.op=='LABEL'}
    successors=[]
    for k,i in enumerate(code):
        dest=[]
        if i.op in {'JUMP','JTRUE','JFALSE'}:dest.append(labels[i.d])
        if i.op not in {'JUMP','RETURN'} and k+1<len(code):dest.append(k+1)
        successors.append(sorted(set(dest)))
    predecessors=[[] for _ in code]
    for k,items in enumerate(successors):
        for j in items:predecessors[j].append(k)
    return successors,predecessors

def fold(i,a,b):
    if i.op=='MOV':return a if constant(a) else None
    if i.op not in BINARY|{'NEG','NOT'} or not constant(a) or (i.op in BINARY and not constant(b)):return None
    try:
        x=literal(a);y=literal(b) if i.op in BINARY else None
        if not isinstance(x,(int,bool)) or (y is not None and not isinstance(y,(int,bool))):return None
        value=operation(i.op,x,y)
        return ('true' if value else 'false') if isinstance(value,bool) else str(value)
    except (RuntimeFault,ValueError,TypeError):return None

def forward(code,preds):
    incoming=[None for _ in code];outgoing=[None for _ in code]
    changed=True
    while changed:
        changed=False
        for k,i in enumerate(code):
            states=([{}] if k==0 else [])+[outgoing[j] for j in preds[k] if outgoing[j] is not None]
            if not states:continue
            state={name:value for name,value in states[0].items() if all(s.get(name)==value for s in states[1:])}
            result=state.copy();dest=definition(i)
            a,b=state.get(i.a,i.a),state.get(i.b,i.b)
            if i.op in {'CALL','STORE','READ'}:result.clear()
            if dest:
                result.pop(dest,None);value=fold(i,a,b)
                if value is not None and not dest.startswith('g'):result[dest]=value
            if state!=incoming[k] or result!=outgoing[k]:changed=True;incoming[k]=state;outgoing[k]=result
    return incoming

def definitely_initialized(code,preds):
    universe={definition(i) for i in code}-{None}
    incoming=[set(universe) for _ in code];outgoing=[set(universe) for _ in code]
    changed=True
    while changed:
        changed=False
        for k,i in enumerate(code):
            states=([set()] if k==0 else [])+[outgoing[j] for j in preds[k]]
            state=set.intersection(*states) if states else set()
            result=state.copy();dest=definition(i)
            if dest:
                result.discard(dest)
                if i.op!='LOCAL':result.add(dest)
            if state!=incoming[k] or result!=outgoing[k]:changed=True;incoming[k]=state;outgoing[k]=result
    return incoming

def optimize_function(code):
    # Repetimos porque uma condição dobrada pode remover um predecessor.
    for _ in range(8):
        original=[i.text() for i in code]
        succ,pred=edges(code);states=forward(code,pred);rewritten=[]
        for k,i in enumerate(code):
            if states[k] is None:continue
            state=states[k];new=replace(i)
            if i.op in BINARY|{'LOAD'}:new.a=state.get(i.a,i.a);new.b=state.get(i.b,i.b)
            elif i.op in {'MOV','CAST','NEG','NOT','PARAM','PRINT','RETURN','JTRUE','JFALSE'}:new.a=state.get(i.a,i.a)
            elif i.op=='STORE':new.a=state.get(i.a,i.a);new.b=state.get(i.b,i.b)
            value=fold(new,new.a,new.b)
            if value is not None:new=replace(new,op='MOV',a=value,b='-')
            if new.op in {'ADD','SUB','MUL'} and new.typ=='int':
                if (new.op in {'ADD','SUB'} and new.b=='0') or (new.op=='MUL' and new.b=='1'):
                    new=replace(new,op='MOV',b='-')
                elif (new.op=='ADD' and new.a=='0') or (new.op=='MUL' and new.a=='1'):
                    new=replace(new,op='MOV',a=new.b,b='-')
            if new.op in {'JTRUE','JFALSE'} and new.a in {'true','false'}:
                if (new.a=='true')==(new.op=='JTRUE'):new=replace(new,op='JUMP',a='-')
                else:continue
            rewritten.append(new)
        code=rewritten
        succ,pred=edges(code)
        # CSE dentro de cada bloco; efeitos e redefinições invalidam o cache.
        leaders={0}|{k for k,i in enumerate(code) if i.op=='LABEL'}|{k+1 for k,i in enumerate(code[:-1]) if i.op in {'JUMP','JTRUE','JFALSE','RETURN'}}
        available={};aliases={}
        for k,i in enumerate(code):
            if k in leaders or i.op in {'CALL','READ','STORE'}:available.clear();aliases.clear()
            dest=definition(i)
            if dest:
                available={key:value for key,value in available.items() if dest not in key[1:3] and value!=dest}
                aliases={name:value for name,value in aliases.items() if name!=dest and value!=dest}
            if i.op in BINARY|{'NEG','NOT','CAST'}:
                key=(i.op,aliases.get(i.a,i.a),aliases.get(i.b,i.b),i.typ)
                if key in available:code[k]=replace(i,op='MOV',a=available[key],b='-')
                else:available[key]=i.d
            if code[k].op=='MOV' and code[k].d!=code[k].a:aliases[code[k].d]=aliases.get(code[k].a,code[k].a)
        # Só removemos definições mortas comprovadamente sem erro/efeito.
        succ,pred=edges(code);initialized=definitely_initialized(code,pred)
        live_in=[set() for _ in code];live_out=[set() for _ in code];changed=True
        while changed:
            changed=False
            for k in reversed(range(len(code))):
                out=set().union(*(live_in[j] for j in succ[k]));dest=definition(code[k])
                incoming=uses(code[k])|(out-({dest} if dest else set()))
                if incoming!=live_in[k] or out!=live_out[k]:changed=True;live_in[k]=incoming;live_out[k]=out
        code=[i for k,i in enumerate(code) if not (i.op in {'MOV','CAST','NOT'} and not i.d.startswith('g') and i.d not in live_out[k] and (constant(i.a) or i.a in initialized[k]))]
        if [i.text() for i in code]==original:break
    return code

def optimize(code):
    result=[];start=0
    for k,i in enumerate(code):
        if i.op=='FUNC':start=k
        elif i.op=='END':result.append(code[start]);result.extend(optimize_function(code[start+1:k]));result.append(i)
    return result

def cfg_dot(code):
    lines=['digraph CFG {'];start=0
    for end,i in enumerate(code):
        if i.op=='FUNC':start=end
        elif i.op=='END':
            body=code[start+1:end];blocks,succ,dom,loops=basic_blocks(body)
            for k,block in enumerate(blocks):
                label=f'B{k} dom='+','.join(map(str,sorted(dom[k])))+'\\l'
                label+=''.join(ins.text().replace('\\','\\\\').replace('"','\\"').replace('\t',' ')+'\\l' for ins in block)
                lines.append(f'  f{start}b{k} [shape=box,label="{label}"];')
                for j in sorted(succ[k]):lines.append(f'  f{start}b{k} -> f{start}b{j};')
            for head,tail,nodes in loops:lines.append(f'  // laco f{start} cabecalho=B{head} cauda=B{tail} corpo='+','.join(map(str,sorted(nodes))))
    return '\n'.join(lines+['}'])+'\n'

def basic_blocks(code):
    if not code:return [],[],[],[]
    leaders=sorted({0}|{k for k,i in enumerate(code) if i.op=='LABEL'}|{k+1 for k,i in enumerate(code[:-1]) if i.op in {'JUMP','JTRUE','JFALSE','RETURN'}})
    blocks=[code[start:end] for start,end in zip(leaders,leaders[1:]+[len(code)])]
    owner={k:b for b,start in enumerate(leaders) for k in range(start,leaders[b+1] if b+1<len(leaders) else len(code))}
    edges_,_=edges(code);succ=[{owner[j] for j in edges_[start+len(blocks[b])-1]} for b,start in enumerate(leaders)]
    preds=[{b for b,s in enumerate(succ) if k in s} for k in range(len(blocks))]
    reachable={0};changed=True
    while changed:
        following=reachable|set().union(*(succ[k] for k in reachable));changed=following!=reachable;reachable=following
    dom=[({0} if k==0 else reachable.copy()) if k in reachable else set() for k in range(len(blocks))]
    changed=True
    while changed:
        changed=False
        for k in sorted(reachable-{0}):
            value={k}|set.intersection(*(dom[p] for p in preds[k]&reachable))
            if value!=dom[k]:changed=True;dom[k]=value
    loops=[]
    for tail in range(len(blocks)):
        for head in sorted(succ[tail]):
            if head not in dom[tail]:continue
            nodes={head,tail};pending=[tail] if tail!=head else []
            while pending:
                node=pending.pop()
                for pred in preds[node]&reachable:
                    if pred not in nodes:nodes.add(pred);pending.append(pred)
            loops.append((head,tail,nodes))
    return blocks,succ,dom,loops
