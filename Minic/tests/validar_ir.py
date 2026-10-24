"""Casos oficiais preservados + execução diferencial + invariantes independentes."""
from pathlib import Path
import json
import os
import subprocess
import sys
import tempfile
import argparse

ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build'
SUITE=ROOT/'tests/ir/originais/testes'
EXPECTED={'P01':'11\n','P02':'8\n','P03':'38\n','P04':'1\n','P05':'6\n','P06':'6\n','P07':'12\n','P08':'1\n','P09':'8\n5\n','P10':'120\n',
          'O01':'20\n','O02':'13\n','O03':'6\n','O04':'9\n','O05':'22\n23\n','O06':'10\n','O07':'11\n','O08':'168\n','O09':'0\n2\n4\n6\n8\n','O10':'4\n'}
ERRORS={'E01':(4,'SEM001'),'E02':(4,'SEM002'),'E03':(4,'SEM003'),'E04':(4,'SEM004'),'E05':(4,'SEM007'),'E06':(4,'SEM009'),'E07':(4,'SEM010'),'E08':(4,'SEM006'),'E09':(3,'SINTATICO'),'E10':(2,'LEXICO')}

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--language',choices=['both','python','c'],default='both')
    parser.add_argument('--entry',type=Path)
    parser.add_argument('--suite',type=Path,default=SUITE)
    options=parser.parse_args()
    BUILD.mkdir(exist_ok=True);sys.path.insert(0,str(ROOT))
    subprocess.run([sys.executable,str(ROOT/'tools/gerar_parser_lr.py'),'--check'],check=True)
    from src.ir.gerador import Instruction,validate
    from src.optimizer.otimizador import edges,definitely_initialized,uses
    from src.codegen.maquina import read_assembly
    from src.ir.atributos import dependency_graph
    from src.scanner.scanner import scan
    from src.parser.parser import parse
    from src.semantico.analisador_semantico import analyze
    from src.ir.gerador import Generator
    binary=BUILD/('minic-ir.exe' if os.name=='nt' else 'minic-ir')
    if options.language!='python':subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Wpedantic','-Werror',str(options.entry.resolve() if options.entry else ROOT/'minic.c'),'-o',str(binary)],check=True)
    commands=[]
    if options.language!='c':commands.append([sys.executable,str(options.entry.resolve() if options.entry else ROOT/'minic.py')])
    if options.language!='python':commands.append([str(binary)])
    suite=options.suite.resolve();sources=sorted(suite.rglob('*.c'))
    assert {p.stem for p in sources}==set(EXPECTED)|set(ERRORS), 'A suíte deve conter exatamente os 30 casos P01–P10/E01–E10/O01–O10.'
    def pair(flags,source,input_text=''):
        results=[subprocess.run(cmd+flags+[str(source)],cwd=ROOT,input=input_text.encode('utf-8'),capture_output=True,timeout=30) for cmd in commands]
        a=results[0]
        assert all((a.returncode,a.stdout,a.stderr)==(b.returncode,b.stdout,b.stderr) for b in results[1:]),(source,flags,[(r.returncode,r.stdout,r.stderr) for r in results])
        return a
    def ir_from(result):
        assert result.returncode==0 and not result.stderr,(result.stdout,result.stderr)
        code=[Instruction(*line.split('\t')) for line in result.stdout.decode('utf-8').splitlines()]
        validate(code);start=0
        for k,i in enumerate(code):
            if i.op=='FUNC':start=k
            elif i.op=='END':
                body=code[start+1:k];_,pred=edges(body);initialized=definitely_initialized(body,pred)
                for j,ins in enumerate(body):
                    assert all(not name.startswith('t') or name in initialized[j] for name in uses(ins)),('Temporário usado sem definição em todos os caminhos',ins)
        return code
    with tempfile.TemporaryDirectory(dir=BUILD) as directory:
        directory=Path(directory)
        for source in sources:
            name=source.stem
            if name in ERRORS:
                status,code=ERRORS[name];result=pair(['--ir'],source)
                assert result.returncode==status and code.encode() in result.stdout+result.stderr,(name,result)
                assert b'FUNC\t' not in result.stdout,name
                continue
            versions=[]
            for opt in ('-O0','-O1'):
                code=ir_from(pair(['--ir',opt],source));versions.append(code)
                result=pair(['--run',opt],source)
                assert result.returncode==0 and result.stdout.decode('utf-8')==EXPECTED[name] and not result.stderr,(name,opt,result)
                result=pair(['--target=asm',opt],source);assert result.returncode==0 and not result.stderr
                read_assembly(result.stdout.decode('utf-8'))
                path=directory/(name+opt+'.mvm');path.write_bytes(result.stdout)
                result=pair(['--exec'],path)
                assert (result.returncode,result.stdout.decode('utf-8'),result.stderr)==(0,EXPECTED[name],b''),(name,opt,result)
            if name.startswith('O') and name!='O09':assert len(versions[1])<len(versions[0]),name
            if name in {'O01','O05','O08'}:assert not any(i.op=='MUL' for i in versions[1]),name
            if name=='O07':assert not any(i.op in {'JTRUE','JFALSE'} for i in versions[1]),name
            if name=='O10':assert not any(i.op=='PRINT' and i.a=='99' for i in versions[1])
        print('IR: 30/30 casos oficiais validados por categoria, estrutura e comportamento; '+('C/Python equivalentes.' if options.language=='both' else 'implementação '+options.language+'.'))
        print('Backend: 20 programas × 2 modos; assembly salvo, recarregado e executado.')
        regressions=json.loads((ROOT/'tests/ir/regressoes.json').read_text(encoding='utf-8'))
        for case in regressions:
            source=directory/(case['nome']+'.c');source.write_bytes(case['fonte'].encode('utf-8'))
            for opt in ('-O0','-O1'):
                result=pair(['--ir' if 'fase' in case else '--run',opt],source,case.get('entrada',''))
                if 'fase' in case:
                    assert result.returncode==case['fase'],(case,result)
                    if 'codigo' in case:assert case['codigo'].encode() in result.stdout,(case,result)
                    if 'diagnosticos' in case:assert result.stderr.count(b'ERRO SINTATICO')==case['diagnosticos'],(case,result)
                    assert b'FUNC\t' not in result.stdout
                else:
                    assert result.returncode==case.get('status',0) and result.stdout.decode('utf-8')==case['saida'],(case,opt,result)
                    if 'erro' in case:assert case['erro'].encode('utf-8') in result.stderr,(case,result)
                    else:assert not result.stderr,(case,result)
            if case['nome']=='cse_entrada':
                code=ir_from(pair(['--ir','-O1'],source));assert sum(i.op=='MUL' for i in code)==1
            if case['nome'] in {'caracteres_texto','tab_literal','read_char_unicode','limite_int64'}:
                result=pair(['--target=asm','-O1'],source);assert result.returncode==0
                saved=directory/(case['nome']+'.mvm');saved.write_bytes(result.stdout)
                result=pair(['--exec'],saved,case.get('entrada',''))
                assert result.returncode==case.get('status',0) and result.stdout.decode('utf-8')==case['saida'] and not result.stderr
        print(f'Regressões finais: {len(regressions)}/{len(regressions)}, incluindo entradas, efeitos, traps e -O0/-O1.')
        source=next(suite.rglob('P03.c'))
        for flag in ('--tokens','--ast','--symbols','--deps','--sdd','--cfg','--check'):
            result=pair([flag],source);assert result.returncode==0 and not result.stderr,(flag,result)
        result=pair(['--trace-parser','--ir'],source)
        assert result.returncode==0 and all(s in result.stderr for s in (b'shift ',b'reduce ',b'accept'))
        program=parse(scan(source.read_text(encoding='utf-8')));assert not analyze(program,source.read_text(encoding='utf-8'),True)
        Generator(program).generate();_,edge,order=dependency_graph(program)
        positions={name:k for k,name in enumerate(order)};assert all(positions[a]<positions[b] for a,b in edge)
        loop=next(suite.rglob('P06.c'));result=pair(['--cfg'],loop);assert b'// laco ' in result.stdout
        error_source=next(suite.rglob('E01.c'));artifact=directory/'nao-gerado.mvm'
        result=pair(['--target=asm','-o',str(artifact)],error_source);assert result.returncode==4 and not artifact.exists()
        artifact.write_bytes(b'arquivo anterior')
        result=pair(['--target=asm','-o',str(artifact)],error_source);assert result.returncode==4 and artifact.read_bytes()==b'arquivo anterior'
        malformed=directory/'invalido.mvm';malformed.write_bytes(b'cabecalho invalido\n')
        result=pair(['--exec'],malformed);assert result.returncode==5 and b'ERRO GERACAO' in result.stderr
        malformed.write_bytes(b'.MINICVM\t1\nFUNC\t$init\t-\t-\tvoid\nRET\t-\t-\t-\tvoid\nEND\t$init\t-\t-\tvoid\nFUNC\tmain\t-\t-\tint\nPUSH\t-\t"texto"\t-\tstring\nPUSH\t-\t1\t-\tint\nADD\t-\t-\t-\tint\nRETVAL\t-\t-\t-\tvoid\nEND\tmain\t-\t-\tvoid\n')
        result=pair(['--exec'],malformed);assert result.returncode==6 and b'Operandos' in result.stderr
        print('Parser SLR, SDD, grafo acíclico, símbolos, CFG e temporários definidos: contratos conferidos.')

if __name__=='__main__':main()
