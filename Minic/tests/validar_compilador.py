#!/usr/bin/env python3
"""Validação das três etapas, gabaritos originais e equivalência C/Python."""
import hashlib
import io
import json
import os
from pathlib import Path
import re
import runpy
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build'
ENV = {**os.environ, 'PYTHONDONTWRITEBYTECODE': '1'}

def binary(name): return BUILD/(name+'.exe' if os.name=='nt' else name)


def invoke(command, source):
    return subprocess.run(command + [str(source)], cwd=ROOT, env=ENV, capture_output=True, timeout=10)


def pair(entry, source):
    py = invoke([sys.executable, str(ROOT / (entry + '.py'))], source)
    c = invoke([str(binary(entry))], source)
    assert (py.returncode, py.stdout, py.stderr) == (c.returncode, c.stdout, c.stderr), f'C/Python divergem em {source}: {py.stdout!r}/{py.stderr!r} vs {c.stdout!r}/{c.stderr!r}'
    return py


def compact(text): return re.sub(r'\s+', '', text)


def main():
    BUILD.mkdir(exist_ok=True)
    sys.path.insert(0, str(ROOT))
    from src.common import write_output
    # Simula encoding Windows e tradução de LF, incluindo acentos/CRLF.
    raw = io.BytesIO()
    windows_stream = io.TextIOWrapper(raw, encoding='cp1252', newline='\r\n')
    message = 'SEM001 — linha 1, coluna 1: análise.\r\nResumo.'
    write_output(message, windows_stream)
    assert raw.getvalue() == message.encode('utf-8')
    print('Saída UTF-8: bytes preservados sob encoding CP1252 e tradução de LF simulados.')
    digest = hashlib.sha256((ROOT/'src/lexer/lexer.l').read_bytes().replace(b'\r\n', b'\n')).hexdigest()
    assert f'SHA256: {digest}' in (ROOT/'src/scanner/scanner.c').read_text(encoding='utf-8').splitlines()[0], 'Scanner C desatualizado: execute python tools/gerar_scanner.py'
    for entry in ('parser', 'minic'):
        subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Wpedantic','-Werror',str(ROOT/(entry+'.c')),'-o',str(binary(entry))],check=True,cwd=ROOT)

    exact = known = 0
    cases = sorted((ROOT/'tests/parser/casos').iterdir())
    assert len(cases) == 50
    for case in cases:
        result = pair('parser', case/'codigo.c')
        if int(case.name[:2]) <= 25:
            assert result.returncode == 0 and not result.stderr, case
            actual = compact(result.stdout.decode())
            expected = compact((case/'ast.esperada.txt').read_text(encoding='utf-8'))
            assert actual.count('(') == actual.count(')'), case
            if actual == expected: exact += 1
            else:
                # Referência 24 contém um ')' excedente. Não altera o gabarito.
                assert case.name.startswith('24_') and expected.count(')') == expected.count('(')+1
                assert len(expected) == len(actual)+1
                assert any(expected[:i]+expected[i+1:] == actual for i,c in enumerate(expected) if c == ')'), case
                known += 1
        else:
            assert result.returncode == 1 and not result.stdout and b'SINTATICO' in result.stderr, case
    print(f'Parser: 25/25 válidos aceitos; 25/25 inválidos rejeitados sem AST. AST: {exact}/25 referências iguais após normalizar espaços; {known} referência com parêntese excedente (caso 24).')
    for language, entry in [('python','parser.py'),('c','parser.c')]:
        subprocess.run(['bash',str(ROOT/f'testar_parser_{language}.sh'),'tests/parser',entry],check=True,cwd=ROOT,env=ENV,stdout=subprocess.DEVNULL)
    print('Scripts do projeto para o parser: 50/50, em C e Python; ressalva explícita do caso 24.')

    semantic_cases = sorted((ROOT/'tests/semantico').glob('*.c'))
    assert len(semantic_cases) == 20
    for source in semantic_cases:
        result = pair('minic', source)
        assert result.stdout == source.with_suffix('.gabarito').read_bytes() and not result.stderr, source
        assert result.returncode == (0 if int(source.name[:2]) <= 10 else 1), source
    print('Semântico: 20/20 gabaritos originais e códigos de saída corretos, em C e Python.')
    for language in ('py', 'c'):
        subprocess.run(['bash', str(ROOT/f'testes_semanticos_{language}.sh'),f'minic.{language}',str(ROOT/'tests/semantico')],check=True,cwd=ROOT,env=ENV,stdout=subprocess.DEVNULL)
    print('Scripts originais do professor: Python 20/20; C 20/20.')

    regressions = json.loads((ROOT/'tests/regressao/casos.json').read_text(encoding='utf-8'))
    regressions.extend([
        {'nome':'256_argumentos','fonte':'int f('+','.join(f'int a{i}' for i in range(256))+'){ return 0; } int principal(){ return f('+','.join('1' for _ in range(256))+'); }','status':0,'codigos':[]},
        {'nome':'escopos_profundos','fonte':'int principal(){'+'{'*125+'int x; {} x=1;'+'}'*125+'return 0;}','status':0,'codigos':[]},
        {'nome':'crlf','fonte':'int principal() {\r\n    return 0;\r\n}\r\n','status':0,'codigos':[]},
        {'nome':'limite_diagnosticos','fonte':'int f(){'+''.join(f'ausente{i}=1;' for i in range(105))+'return 0;}','status':1,'codigos':['SEM001']*100},
    ])
    with tempfile.TemporaryDirectory(dir=BUILD) as directory:
        for case in regressions:
            source = Path(directory)/(case['nome']+'.c')
            source.write_bytes(case['fonte'].encode('utf-8'))
            result = pair('minic',source)
            assert result.returncode == case['status'], (case,result.stdout,result.stderr)
            if 'etapa' in case: assert case['etapa'].encode() in result.stderr, case
            else:
                codes = re.findall(r'^SEM\d+',result.stdout.decode(),re.MULTILINE)
                assert codes == case['codigos'], (case,codes,result.stdout)
    print(f'Regressões: {len(regressions)}/{len(regressions)}; saídas e status equivalentes C/Python.')
    annotations=runpy.run_path(str(ROOT/'tests/verificar_anotacoes.py'))
    annotations['verify'](ROOT,BUILD,ENV)

    # Reutiliza as comparações da etapa 1 com saídas fora dos resultados antigos.
    runner = runpy.run_path(str(ROOT/'tests/run_tests.py'))
    lexer = binary('lexer-etapa1')
    subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Wpedantic','-Werror',str(ROOT/'scanner.c'),'-o',str(lexer)],check=True)
    fixtures = sorted((ROOT/'tests/casos-programas-c').glob('*.c')) + sorted((ROOT/'tests/casos-invalidos').glob('*.minic'))
    assert len(fixtures) == 10
    for source in fixtures:
        assert runner['test_case']([sys.executable,str(ROOT/'scanner.py')],source,BUILD/'scanner-python'),source
        assert runner['test_case']([str(lexer)],source,BUILD/'scanner-c'),source
    subprocess.run([sys.executable,str(ROOT/'tests/check_fixtures.py')],check=True)
    print('Etapa 1 preservada: Python 10/10; C 10/10. i06 sem fonte no material original.')
    sys.stdout.flush()
    subprocess.run([sys.executable,str(ROOT/'tests/validar_ir.py')],check=True,cwd=ROOT,env=ENV)


if __name__=='__main__': main()
