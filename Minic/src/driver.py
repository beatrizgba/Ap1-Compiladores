"""Interface final; a chamada simples da etapa 3 continua compatível."""
from pathlib import Path
import json
import sys
from src.common import CompileError,write_output
from src.scanner.scanner import scan
from src.parser.parser import parse,render_ast
from src.semantico.analisador_semantico import analyze,render_diagnostics
from src.ir.gerador import Generator,render,validate
from src.ir.atributos import dependencies_dot,attributes_text,symbols_text
from src.optimizer.otimizador import optimize,cfg_dot
from src.codegen.maquina import lower,assembly,read_assembly,execute,RuntimeFault

HELP='''Uso: minic [opções] arquivo.c
  --tokens       mostrar tokens
  --ast          mostrar AST construída pelo parser SLR
  --symbols      mostrar símbolos e slots dos frames
  --check        verificar semântica sem gerar código
  --ir           gerar quádruplas 3AC
  --target=asm   gerar assembly da MINIC-VM
  --run          compilar e executar na MINIC-VM
  --exec         executar arquivo de assembly .mvm
  --sdd          mostrar atributos e ordem topológica
  --deps         mostrar grafo de dependências (DOT)
  --cfg          mostrar grafo de fluxo de controle (DOT)
  --trace-parser registrar shift/reduce em stderr
  -O0 / -O1      desativar/ativar otimizações
  -o arquivo     salvar a saída gerada
Sem opções: interface compatível com os scripts semânticos da etapa 3.
'''

def main(args):
    if args==['--help']:write_output(HELP);return 0
    mode=None;path=None;output=None;opt=False;trace=None;legacy=len(args)==1 and not args[0].startswith('-')
    modes={'--tokens':'tokens','--ast':'ast','--symbols':'symbols','--check':'check','--ir':'ir',
           '--target=asm':'asm','--run':'run','--exec':'exec','--sdd':'sdd','--deps':'deps','--cfg':'cfg'}
    index=0
    while index<len(args):
        arg=args[index];index+=1
        if arg in modes:
            if mode is not None:write_output(HELP,sys.stderr);return 1
            mode=modes[arg]
        elif arg in {'-O0','-O1'}:opt=arg=='-O1'
        elif arg=='--trace-parser':trace=[]
        elif arg=='-o' and index<len(args):output=args[index];index+=1
        elif arg.startswith('-') or path is not None:write_output(HELP,sys.stderr);return 1
        else:path=arg
    if path is None or (output and mode in {'run','exec','check'}):write_output(HELP,sys.stderr);return 1
    mode=mode or ('legacy' if legacy else 'ir')
    try:
        source=Path(path).read_text(encoding='utf-8')
    except (OSError,UnicodeError):write_output('ERRO ENTRADA: não foi possível ler o arquivo\n',sys.stderr);return 2 if legacy else 1
    try:
        if mode=='exec':
            try:code=read_assembly(source)
            except ValueError:raise ValueError('Assembly MINIC-VM inválido.')
        else:
            tokens=scan(source)
            if mode=='tokens':text=''.join(f'{t.kind}\t{t.text}\t{t.line}\t{t.column}\n' for t in tokens)
            else:
                try:program=parse(tokens,trace,recover=not legacy)
                finally:
                    if trace is not None:write_output('\n'.join(trace)+'\n',sys.stderr)
                if mode=='ast':text=render_ast(program)+'\n'
                else:
                    errors=analyze(program,source,final_mode=not legacy)
                    if mode in {'legacy','check'} or errors:
                        write_output(render_diagnostics(errors));return (1 if legacy else 4) if errors else 0
                    code=Generator(program).generate();validate(code)
                    if mode=='sdd':text=attributes_text(program)
                    elif mode=='deps':text=dependencies_dot(program)
                    elif mode=='symbols':text=symbols_text(program)
                    else:
                        if opt:code=optimize(code);validate(code)
                        if mode=='ir':text=render(code)
                        elif mode=='cfg':text=cfg_dot(code)
                        elif mode=='asm':text=assembly(code)
                        else:code=lower(code)
        if mode in {'run','exec'}:
            if hasattr(sys.stdin,'reconfigure'):sys.stdin.reconfigure(encoding='utf-8')
            class UTF8Output:
                def write(self,text):write_output(text)
            return execute(code,output=UTF8Output())
        if output:Path(output).write_bytes(text.encode('utf-8'))
        else:write_output(text)
        return 0
    except CompileError as error:
        message=error.render()
        if not legacy and error.stage=='LEXICO':message+=f'; símbolo “{error.token.text[:1]}”.'
        write_output(message+'\n',sys.stderr)
        return 1 if legacy else 2 if error.stage=='LEXICO' else 3
    except RuntimeFault as error:write_output(f'ERRO EXECUCAO: {error}\n',sys.stderr);return 6
    except (ValueError,OSError,OverflowError) as error:write_output(f'ERRO GERACAO: {error}\n',sys.stderr);return 5
