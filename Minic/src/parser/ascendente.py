"""Parser ascendente SLR(1): pilha de estados e atributos sintetizados."""
import json
from pathlib import Path
from src.common import Node, CompileError

TABLE = json.loads(Path(__file__).with_name('tabelas_lr.json').read_text(encoding='utf-8'))

def reduce(action, v, start, end):
    def node(kind, token=start, **fields): return Node(kind, token, end, **fields)
    if action=='empty':return []
    if action in {'pass','token'}:return v[0]
    if action=='append':return v[0]+(v[1] if isinstance(v[1],list) else [v[1]])
    if action=='append_comma':return (v[0] if isinstance(v[0],list) else [v[0]])+[v[2]]
    if action=='program':return node('program',children=v[0])
    if action=='function':return node('function',value=v[1].text,type_name=v[0].text,params=v[3] if isinstance(v[3],list) else [v[3]],body=v[5])
    if action in {'param','param_array'}:
        result=node('param',v[1],value=v[1].text,type_name=v[0].text,array=action=='param_array');result.end=v[1].end;return result
    if action=='decls':
        decls=v[1] if isinstance(v[1],list) else [v[1]]
        for decl in decls:decl.type_name=v[0].text
        return decls
    if action.startswith('decl'):
        return node('decl',v[0],value=v[0].text,array='array' in action,
                    size=v[2] if 'array' in action else None,
                    initializer=v[-1] if 'init' in action else None)
    if action=='block':return node('block',children=v[1])
    if action in {'if','if_else','while'}:
        return node('while' if action=='while' else 'if',left=v[2],body=v[4],otherwise=v[6] if action=='if_else' else None)
    if action=='for':return node('for',initializer=v[2] or None,left=v[4] or None,right=v[6] or None,body=v[8])
    if action=='return':return node('return',left=v[1] or None)
    if action in {'break','continue'}:return node(action)
    if action=='empty_stmt':return node('empty')
    if action=='expr_stmt':return node('expr',left=v[0])
    if action in {'assign','binary'}:
        result=node(action,v[0].token,left=v[0],right=v[2],value=v[1].text if action=='binary' else '');result.end=v[2].end;return result
    if action=='unary':
        result=node('unary',right=v[1],value=v[0].text);result.end=v[1].end;return result
    if action=='index':return node('index',v[0].token,left=v[0],right=v[2])
    if action=='call':return node('call',v[0],value=v[0].text,children=v[2] if isinstance(v[2],list) else [v[2]])
    if action=='id':return node('id',v[0],value=v[0].text)
    if action=='parentheses':return v[1]
    if action=='literal':return node('literal',value=v[0].text,type_name={'INT_LIT':'int','FLOAT_LIT':'float','CHAR_LIT':'char','STRING_LIT':'string','TRUE':'bool','FALSE':'bool'}[v[0].kind])
    raise AssertionError(action)

class SyntaxErrors(CompileError):
    def __init__(self,errors):
        first=errors[0];super().__init__(first.stage,first.token,first.message);self.errors=errors
    def render(self):return '\n'.join(error.render() for error in self.errors)

def parse(tokens, trace=None, recover=False):
    from src.common import Token
    states=[0];values=[];spans=[];pos=0;errors=[];injected=None;last_repair=-1
    while True:
        token=injected or tokens[pos]
        term=token.kind if token.kind in TABLE['terminals'] else token.text
        action=TABLE['action'][states[-1]].get(term,0)
        if action==32767:
            if errors:raise SyntaxErrors(errors)
            result=values[-1];result.token=tokens[0];result.end=token.end
            if trace is not None:trace.append('accept')
            return result
        if action>0:
            if trace is not None:trace.append(f'shift {term} -> {action-1}')
            states.append(action-1);values.append(token);spans.append((token,token.end))
            if injected:injected=None
            else:pos+=1
        elif action<0:
            lhs,rhs,kind=TABLE['rules'][-action];count=len(rhs)
            start,end=(spans[-count][0],spans[-1][1]) if count else (token,token.start)
            args=values[-count:] if count else []
            value=reduce(kind,args,start,end)
            if count:del states[-count:];del values[-count:];del spans[-count:]
            states.append(TABLE['goto'][states[-1]][lhs]);values.append(value);spans.append((start,end))
            if trace is not None:trace.append(f'reduce {-action}: {lhs} -> '+(' '.join(rhs) or 'ε'))
        else:
            expected=', '.join(sorted(TABLE['action'][states[-1]]))
            error=CompileError('SINTATICO',token,f"encontrado '{token.text}'; esperado: {expected}")
            if not recover:raise error
            errors.append(error)
            if len(errors)>=10 or token.kind=='EOF':raise SyntaxErrors(errors)
            if ';' in TABLE['action'][states[-1]] and last_repair!=pos:
                injected=Token('SEMICOLON',';',token.line,token.column,token.start,token.start);last_repair=pos
                continue
            injected=None
            while tokens[pos].kind!='EOF' and tokens[pos].text not in {';','}'}:pos+=1
            if tokens[pos].kind=='EOF':raise SyntaxErrors(errors)
            while len(states)>1 and not TABLE['action'][states[-1]].get(tokens[pos].text):
                states.pop();values.pop();spans.pop()
            if not TABLE['action'][states[-1]].get(tokens[pos].text):pos+=1
