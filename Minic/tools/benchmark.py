"""Mede compilação e execução separadamente; compara resultados antes de medir."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import platform
import statistics
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--repeticoes',type=int,default=5)
    parser.add_argument('--saida',type=Path,default=ROOT/'build/benchmark')
    args=parser.parse_args()
    if args.repeticoes<1:parser.error('Use pelo menos uma repetição.')
    target=args.saida.resolve();target.mkdir(parents=True,exist_ok=True)
    binary=target/('minic.exe' if os.name=='nt' else 'minic')
    subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Wpedantic','-Werror',str(ROOT/'minic.c'),'-o',str(binary)],check=True)
    metadata={'sistema':platform.platform(),'maquina':platform.machine(),'python':sys.version,
              'gcc':subprocess.run(['gcc','--version'],capture_output=True,text=True,check=True).stdout.splitlines()[0],
              'repeticoes':args.repeticoes,'aquecimentos':1,'tamanhos':[20,200,2000],
              'metrica':'mediana de processo --exec (inclui inicialização do interpretador/VM; compilação medida separadamente)',
              'fonte_sha256':hashlib.sha256((ROOT/'examples/benchmark.c').read_bytes()).hexdigest()}
    rows=[]
    for language,cmd in [('Python',[sys.executable,str(ROOT/'minic.py')]),('C',[str(binary)])]:
        versions={}
        for optimization in ('-O0','-O1'):
            assembly=target/(language+optimization+'.mvm')
            start=time.perf_counter()
            subprocess.run(cmd+[optimization,'--target=asm',str(ROOT/'examples/benchmark.c'),'-o',str(assembly)],capture_output=True,check=True)
            compile_ms=(time.perf_counter()-start)*1000
            ir=subprocess.run(cmd+[optimization,'--ir',str(ROOT/'examples/benchmark.c')],capture_output=True,check=True)
            versions[optimization]=(assembly,compile_ms,len(ir.stdout.splitlines()),len(assembly.read_bytes().splitlines())-1)
        for n in metadata['tamanhos']:
            for optimization,(assembly,compile_ms,ir_count,vm_count) in versions.items():
                command=cmd+['--exec',str(assembly)];data=f'{n}\n'.encode();expected=f'{n*42}\n'.encode()
                warm=subprocess.run(command,input=data,capture_output=True,check=True)
                assert warm.stdout==expected and not warm.stderr
                timings=[]
                for _ in range(args.repeticoes):
                    start=time.perf_counter();result=subprocess.run(command,input=data,capture_output=True,check=True);elapsed=(time.perf_counter()-start)*1000
                    assert result.stdout==expected and not result.stderr
                    timings.append(elapsed)
                rows.append({'linguagem':language,'entrada_n':n,'modo':optimization,'instrucoes_ir':ir_count,'instrucoes_vm':vm_count,
                             'compilacao_ms':round(compile_ms,3),'execucao_mediana_ms':round(statistics.median(timings),3),'saida':n*42,'status':0})
    with (target/'resultados.csv').open('w',newline='',encoding='utf-8') as out:
        writer=csv.DictWriter(out,fieldnames=list(rows[0]),lineterminator='\n');writer.writeheader();writer.writerows(rows)
    (target/'ambiente.json').write_text(json.dumps(metadata,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(f'Benchmark: {len(rows)} medições agregadas; saídas/status conferidos; resultados em {target}.')

if __name__=='__main__':main()
