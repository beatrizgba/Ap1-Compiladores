"""Confere os dados que a próxima etapa recebe, além dos diagnósticos do terminal."""
import subprocess
import tempfile
import os
from pathlib import Path

from src.scanner.scanner import scan
from src.parser.parser import parse, render_ast
from src.semantico.analisador_semantico import analyze


def walk(n):
    if n is None: return
    yield n
    for child in n.children: yield from walk(child)
    for param in n.params: yield from walk(param)
    for field in ('left', 'right', 'body', 'otherwise', 'size', 'initializer'):
        yield from walk(getattr(n, field))


def snapshot(program):
    rows=[]
    for n in walk(program):
        symbol = f'{n.symbol.node.value}:{n.symbol.node.line}:{n.symbol.node.column}' if n.symbol else '-'
        values=[n.kind,n.line,n.column,n.semantic_type,n.category,n.conversion_to,
                n.expected_type,n.array_size if n.array_size is not None else '-',
                int(n.assignable),n.loop_depth,n.flow,symbol]
        rows.append('\t'.join(map(str,values)))
    return ('\n'.join(rows)+'\n').encode('utf-8')


def verify(root, build, env):
    driver=build/('anotacoes.exe' if os.name=='nt' else 'anotacoes')
    subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Wpedantic','-Werror',
        str(root/'tests/mostrar_anotacoes.c'),str(root/'src/common.c'),str(root/'src/scanner/scanner.c'),
        str(root/'src/parser/parser.c'),str(root/'src/semantico/analisador_semantico.c'),'-o',str(driver)],check=True)
    sources=[
        'float promove(int n) { return n; } float soma(float x,int y) { return x+y; } int principal() { int v[3]; int i=1; float r=1; r=soma(2,v[i]); while(i<3) { if(i==2) break; i=i+1; continue; } return v[0]; }',
        'int x; int f() { int x; {float x; x=1;} x=2; return x; }',
        'int f() { return ausente+1; }',
        'return ausente;',
        'float f() {return g(1);} float g(float n) {return n;}',
    ]
    with tempfile.TemporaryDirectory(dir=build) as directory:
        for index, source in enumerate(sources):
            path=Path(directory)/f'anotacoes-{index}.c'
            path.write_bytes(source.encode('utf-8'))
            program=parse(scan(source))
            original=render_ast(program)
            errors=analyze(program,source)
            assert render_ast(program)==original, 'A análise alterou a estrutura sintática.'
            nodes=list(walk(program))
            assert all(n.semantic_type for n in nodes), 'Há nó sem anotação de tipo.'
            c=subprocess.run([str(driver),str(path)],capture_output=True,check=True,env=env)
            assert c.stdout==snapshot(program) and not c.stderr, 'Anotações C/Python diferentes.'
            if index==0:
                assert not errors
                promoted=program.children[0].body.children[0].left
                assert promoted.semantic_type=='int' and promoted.conversion_to=='float'
                assert promoted.symbol.node is program.children[0].params[0]
                mixed=program.children[1].body.children[0].left
                assert mixed.semantic_type=='float' and mixed.right.conversion_to=='float'
                call=next(n for n in nodes if n.kind=='call')
                assert call.symbol.node is program.children[1] and call.semantic_type=='float'
                assert call.children[0].conversion_to=='float' and call.children[0].expected_type=='float'
                indices=[n for n in nodes if n.kind=='index']
                assert all(n.assignable and n.semantic_type=='int' for n in indices)
                assert all(n.left.array_size==3 and n.symbol.node.array_size==3 for n in indices)
                assert all(n.loop_depth==1 for n in nodes if n.kind in {'break','continue'})
            elif index==1:
                assert not errors
                returned=program.children[1].body.children[-1].left
                assert returned.symbol.node is program.children[1].body.children[0]
                inner=program.children[1].body.children[1].children[1].left.left
                assert inner.symbol.node is program.children[1].body.children[1].children[0]
            elif index==2:
                assert [e[2] for e in errors]==['SEM001']
                expression=program.children[0].body.children[0].left
                assert expression.semantic_type==expression.left.semantic_type=='error'
                assert expression.left.symbol is None
            elif index==3:
                assert [e[2] for e in errors]==['SEM009','SEM001']
                assert program.children[0].semantic_type=='error'
                assert program.children[0].left.semantic_type=='error'
            else:
                assert not errors
                call=program.children[0].body.children[0].left
                assert call.symbol.node is program.children[1]
                assert call.children[0].conversion_to=='float' and call.semantic_type=='float'
    print('AST anotada: tipos, símbolos, conversões, escopos, vetores e contexto equivalentes C/Python.')
