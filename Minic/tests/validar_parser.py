#!/usr/bin/env python3
"""Valida os 50 casos segundo o README do professor: AST ou rejeição com diagnóstico."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]


def compact(text): return re.sub(r'\s+', '', text)


def known_reference(case, actual, expected):
    return (case.name.startswith('24_') and expected.count(')')==expected.count('(')+1
        and len(expected)==len(actual)+1
        and any(expected[:i]+expected[i+1:]==actual for i,c in enumerate(expected) if c==')'))


def main():
    sys.stdout.reconfigure(encoding='utf-8', newline='\n')
    sys.stderr.reconfigure(encoding='utf-8', newline='\n')
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--linguagem',choices=('py','c'),required=True)
    parser.add_argument('diretorio')
    parser.add_argument('parser')
    args=parser.parse_args()
    directory=Path(args.diretorio).resolve()
    source=Path(args.parser).resolve()
    if not directory.is_dir() or not source.is_file():
        parser.error('informe um diretório de testes e um arquivo do parser existentes')
    cases=sorted(directory.rglob('codigo.c'))
    if not cases: parser.error('nenhum codigo.c encontrado no diretório informado')
    build=ROOT/'build';build.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(dir=build) as temporary:
        if args.linguagem=='c':
            binary=Path(temporary)/('parser.exe' if os.name=='nt' else 'parser')
            subprocess.run(['gcc','-std=c11','-Wall','-Wextra',str(source),'-o',str(binary)],check=True)
            command=[str(binary)]
        else: command=[sys.executable,str(source)]
        passed=known=0
        for path in cases:
            case=path.parent
            expectation=(case/'resultado.esperado.txt').read_text(encoding='utf-8').splitlines()[0]
            result=subprocess.run(command+[str(path)],capture_output=True,timeout=10)
            reference_issue=False
            if expectation=='REJEITADO':
                ok=result.returncode==1 and not result.stdout and b'SINTATICO' in result.stderr
            elif expectation=='ACEITO':
                actual=compact(result.stdout.decode('utf-8'))
                expected=compact((case/'ast.esperada.txt').read_text(encoding='utf-8'))
                reference_issue=known_reference(case,actual,expected)
                ok=result.returncode==0 and not result.stderr and actual.count('(')==actual.count(')') and (actual==expected or reference_issue)
            else: raise ValueError(f'Resultado esperado desconhecido em {case}')
            if ok:
                passed+=1
                if reference_issue: known+=1
            suffix=' — referência original com parêntese excedente' if reference_issue else ''
            print(f'[{"OK" if ok else "FALHA"}] {case.name}{suffix}')
        print(f'Resultado: {passed}/{len(cases)} aprovados; {len(cases)-passed} reprovados; {known} ressalva de gabarito.')
        return 0 if passed==len(cases) else 1


if __name__=='__main__': raise SystemExit(main())
