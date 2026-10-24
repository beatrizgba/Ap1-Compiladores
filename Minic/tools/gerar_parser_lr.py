"""Gera tabelas SLR(1), sem dependências externas; conflito else = shift.

As ações das reduções constroem atributos sintetizados (AST). A gramática e as
tabelas resultantes são compartilhadas pelas implementações C e Python.
"""
from collections import defaultdict, deque
from pathlib import Path
import json
import sys

ROOT = Path(__file__).resolve().parents[1]
RULES = []
def generated(path,text):
    if '--check' in sys.argv:
        assert path.read_text(encoding='utf-8')==text, f'Tabela desatualizada: {path}. Execute tools/gerar_parser_lr.py.'
    else:path.write_text(text,encoding='utf-8')
def rule(lhs, rhs, action='pass'):
    RULES.append((lhs, rhs.split() if rhs else [], action))

rule('Start', 'Program', 'pass')
rule('Program', 'Items', 'program')
rule('Items', '', 'empty')
rule('Items', 'Items Item', 'append')
rule('Item', 'Decl')
rule('Item', 'Function')
rule('Item', 'Stmt')
for typ in ['int','float','bool','char','void']: rule('Type', typ, 'token')
rule('Function', 'Type IDENT ( Params ) Block', 'function')
rule('Params', '', 'empty')
rule('Params', 'void', 'empty')
rule('Params', 'ParamList')
rule('ParamList', 'Param')
rule('ParamList', 'ParamList , Param', 'append_comma')
rule('Param', 'Type IDENT', 'param')
rule('Param', 'Type IDENT [ ]', 'param_array')
rule('Decl', 'Type Declarators ;', 'decls')
rule('Declarators', 'Declarator')
rule('Declarators', 'Declarators , Declarator', 'append_comma')
rule('Declarator', 'IDENT', 'decl')
rule('Declarator', 'IDENT = Expr', 'decl_init')
rule('Declarator', 'IDENT [ Expr ]', 'decl_array')
rule('Declarator', 'IDENT [ Expr ] = Expr', 'decl_array_init')
rule('Block', '{ BlockItems }', 'block')
rule('BlockItems', '', 'empty')
rule('BlockItems', 'BlockItems BlockItem', 'append')
rule('BlockItem', 'Decl')
rule('BlockItem', 'Stmt')
rule('Stmt', 'Block')
rule('Stmt', 'if ( Expr ) Stmt', 'if')
rule('Stmt', 'if ( Expr ) Stmt else Stmt', 'if_else')
rule('Stmt', 'while ( Expr ) Stmt', 'while')
rule('Stmt', 'for ( OptionalExpr ; OptionalExpr ; OptionalExpr ) Stmt', 'for')
rule('Stmt', 'return OptionalExpr ;', 'return')
rule('Stmt', 'break ;', 'break')
rule('Stmt', 'continue ;', 'continue')
rule('Stmt', ';', 'empty_stmt')
rule('Stmt', 'Expr ;', 'expr_stmt')
rule('OptionalExpr', '', 'empty')
rule('OptionalExpr', 'Expr')
rule('Expr', 'Or')
rule('Expr', 'Or = Expr', 'assign')
levels = [('Or','And',['||']),('And','Equality',['&&']),
          ('Equality','Relation',['==','!=']),('Relation','Sum',['<','<=','>','>=']),
          ('Sum','Product',['+','-']),('Product','Unary',['*','/','%'])]
for lhs, child, ops in levels:
    rule(lhs, child)
    for op in ops: rule(lhs, lhs+' '+op+' '+child, 'binary')
rule('Unary','Postfix')
for op in ['!','-','+']: rule('Unary',op+' Unary','unary')
rule('Postfix','Atom')
rule('Postfix','Postfix [ Expr ]','index')
rule('Postfix','Name ( Args )','call')
rule('Args','','empty')
rule('Args','ArgList')
rule('ArgList','Expr')
rule('ArgList','ArgList , Expr','append_comma')
rule('Atom','Name','id')
rule('Name','IDENT','token')
rule('Name','print','token')
rule('Name','read','token')
rule('Atom','( Expr )','parentheses')
for token in ['INT_LIT','FLOAT_LIT','CHAR_LIT','STRING_LIT','TRUE','FALSE']:
    rule('Atom',token,'literal')

def generate():
    nts = list(dict.fromkeys(lhs for lhs,_,_ in RULES))
    terminals = sorted({s for _,rhs,_ in RULES for s in rhs if s not in nts}|{'EOF'})
    by_lhs = defaultdict(list)
    for i,(lhs,_,_) in enumerate(RULES): by_lhs[lhs].append(i)
    first={s:{s} for s in terminals};first.update({s:set() for s in nts})
    nullable=set()
    changed=True
    while changed:
        changed=False
        for lhs,rhs,_ in RULES:
            before=len(first[lhs])
            for s in rhs:
                first[lhs]|=first[s]
                if s not in nullable: break
            else:
                if lhs not in nullable: nullable.add(lhs);changed=True
            changed |= len(first[lhs])!=before
    follow={s:set() for s in nts};follow['Start'].add('EOF')
    changed=True
    while changed:
        changed=False
        for lhs,rhs,_ in RULES:
            trailer=follow[lhs].copy()
            for s in reversed(rhs):
                if s in nts:
                    before=len(follow[s]);follow[s]|=trailer;changed|=before!=len(follow[s])
                trailer=(trailer|first[s]) if s in nullable else first[s].copy()
    def closure(items):
        items=set(items);queue=list(items)
        for r,dot in queue:
            rhs=RULES[r][1]
            if dot<len(rhs) and rhs[dot] in nts:
                for k in by_lhs[rhs[dot]]:
                    if (k,0) not in items: items.add((k,0));queue.append((k,0))
        return frozenset(items)
    states=[closure([(0,0)])];indexes={states[0]:0};transitions={}
    for i,items in enumerate(states):
        symbols=sorted({RULES[r][1][dot] for r,dot in items if dot<len(RULES[r][1])})
        for symbol in symbols:
            dest=closure((r,dot+1) for r,dot in items if dot<len(RULES[r][1]) and RULES[r][1][dot]==symbol)
            if dest not in indexes:indexes[dest]=len(states);states.append(dest)
            transitions[i,symbol]=indexes[dest]
    action=[{} for _ in states];goto=[{} for _ in states];conflicts=[]
    def put(i,s,v):
        old=action[i].get(s)
        if old is not None and old!=v:
            # A associação de else ao if mais próximo é a única ambiguidade.
            if s=='else' and (old>0 or v>0):
                conflicts.append((i,s,old,v));v=max(old,v)
            else:raise ValueError(('Conflito inesperado',i,s,old,v))
        action[i][s]=v
    for i,items in enumerate(states):
        for r,dot in sorted(items):
            lhs,rhs,_=RULES[r]
            if dot<len(rhs):
                s=rhs[dot];dest=transitions[i,s]
                if s in nts:goto[i][s]=dest
                else:put(i,s,dest+1)
            elif r==0:put(i,'EOF',32767)
            else:
                for s in sorted(follow[lhs]):put(i,s,-r)
    data={'rules':RULES,'terminals':terminals,'nonterminals':nts,'action':action,'goto':goto,
          'conflicts':conflicts}
    generated(ROOT/'src/parser/tabelas_lr.json',json.dumps(data,ensure_ascii=False,separators=(',',':'))+'\n')
    kinds=list(dict.fromkeys(a for _,_,a in RULES))
    def cstr(s):return json.dumps(s,ensure_ascii=False)
    out=['/* Gerado por tools/gerar_parser_lr.py. Não editar manualmente. */',
         f'#define LR_STATES {len(states)}',f'#define LR_TERMINALS {len(terminals)}',
         f'#define LR_NONTERMINALS {len(nts)}',
         'enum { '+', '.join('LR_'+s.upper() for s in kinds)+' };',
         'static const char *lr_terminals[]={'+','.join(map(cstr,terminals))+'};',
         'static const char *lr_rules[]={'+','.join(cstr(lhs+' -> '+(' '.join(rhs) or 'ε')) for lhs,rhs,_ in RULES)+'};']
    for name,rows,cols in [('action',action,terminals),('goto',goto,nts)]:
        out.append(f'static const short lr_{name}[LR_STATES][{len(cols)}]={{')
        out += ['{'+','.join(str(row.get(s,0)) for s in cols)+'},' for row in rows]
        out.append('};')
    for name,values in [('length',[len(rhs) for _,rhs,_ in RULES]),('lhs',[nts.index(lhs) for lhs,_,_ in RULES]),('kind',['LR_'+a.upper() for _,_,a in RULES])]:
        out.append('static const short lr_'+name+'[]={'+','.join(map(str,values))+'};')
    generated(ROOT/'src/parser/tabelas_lr.h','\n'.join(out)+'\n')
    generated(ROOT/'docs/gramatica.bnf','\n'.join(f'{i:02d}. {lhs} -> '+(' '.join(rhs) or 'ε')+f'  {{{a}}}' for i,(lhs,rhs,a) in enumerate(RULES))+'\n')
    print(f'SLR: {len(states)} estados; {len(RULES)} produções; {len(conflicts)} conflito else resolvido por shift.')

if __name__=='__main__':generate()
