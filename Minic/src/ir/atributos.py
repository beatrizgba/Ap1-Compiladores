"""Evidências da SDD: atributos calculados e grafo de dependências acíclico."""
import json
from src.ir.gerador import walk, children, storage

def dependency_graph(program):
    nodes=list(walk(program));ids={id(n):k for k,n in enumerate(nodes)}
    attributes={};edges=set()
    for k,n in enumerate(nodes):
        values={'type':n.semantic_type or '-', 'symbol':n.symbol.node.value if n.symbol else '-',
                'loop':str(n.loop_depth), 'place':getattr(n,'ir_place','-'),
                'code':f'{getattr(n,"ir_begin",0)}:{getattr(n,"ir_end",0)}'}
        for attr,value in values.items():attributes[f'n{k}_{attr}']=f'n{k}.{attr}={value}'
        for a,b in [('symbol','type'),('type','place'),('place','code'),('loop','code')]:edges.add((f'n{k}_{a}',f'n{k}_{b}'))
        if n.symbol and n.symbol.node is not n:
            edges.add((f'n{ids[id(n.symbol.node)]}_symbol',f'n{k}_symbol'))
        for child in children(n):
            c=ids[id(child)]
            edges.add((f'n{c}_code',f'n{k}_code'))
            if child.kind!='function':edges.add((f'n{k}_loop',f'n{c}_loop'))
            if n.kind in {'binary','unary','index','assign','call'}:edges.add((f'n{c}_type',f'n{k}_type'))
    # Uma ordenação topológica verifica que as regras não se tornaram cíclicas.
    remaining=set(attributes);order=[]
    while remaining:
        ready=sorted(v for v in remaining if not any(b==v and a in remaining for a,b in edges))
        if not ready:raise ValueError('Ciclo no grafo de atributos.')
        order.extend(ready);remaining.difference_update(ready)
    return attributes,sorted(edges),order

def dependencies_dot(program):
    attributes,edges,_=dependency_graph(program)
    lines=['digraph SDD {']
    for name,label in attributes.items():lines.append(f'  {name} [label={json.dumps(label,ensure_ascii=False)}];')
    for a,b in edges:lines.append(f'  {a} -> {b};')
    return '\n'.join(lines+['}'])+'\n'

def attributes_text(program):
    _,_,order=dependency_graph(program)
    lines=['no\tclasse\tlinha\tcoluna\ttipo\tsimbolo\tplace\tcode\tloop']
    for k,n in enumerate(walk(program)):
        lines.append('\t'.join(map(str,(k,n.kind,n.line,n.column,n.semantic_type,n.symbol.node.value if n.symbol else '-',getattr(n,'ir_place','-'),f'{getattr(n,"ir_begin",0)}:{getattr(n,"ir_end",0)}',n.loop_depth))))
    lines.append('ordem_topologica\t'+','.join(order))
    return '\n'.join(lines)+'\n'

def symbols_text(program):
    nodes=list(walk(program));lines=['nome\tcategoria\ttipo\tnivel\tlinha\tcoluna\tendereco\tslot\tinicializacao\tusos']
    def visit(n,level,slot):
        if n.kind in {'decl','param','function'}:
            uses=sum(1 for ref in nodes if ref is not n and ref.symbol and ref.symbol.node is n)
            state='parametro' if n.kind=='param' else 'inicializador' if n.initializer else 'zero' if getattr(n,'global_scope',False) else 'declarada'
            lines.append('\t'.join(map(str,(n.value,n.category,n.semantic_type,level,n.line,n.column,n.value if n.kind=='function' else storage(n),'-' if n.kind=='function' else slot[0],state,uses))))
            if n.kind!='function':slot[0]+=n.array_size if n.array and n.kind!='param' else 1
        if n.kind=='function':
            local=[0]
            for param in n.params:visit(param,1,local)
            for child in n.body.children:visit(child,1,local)
        elif n.kind in {'program','block'}:
            for child in n.children:visit(child,level+(n.kind=='block'),slot)
        else:
            for child in children(n):visit(child,level,slot)
    visit(program,0,[0])
    return '\n'.join(lines)+'\n'
