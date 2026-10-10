#!/usr/bin/env python3
"""Etapa 3: scanner da etapa 1 → parser da etapa 2 → análise semântica."""
import sys
from pathlib import Path
from src.common import CompileError
from src.scanner.scanner import scan
from src.parser.parser import parse
from src.semantico.analisador_semantico import analyze, render_diagnostics


def main():
    if len(sys.argv) != 2:
        print('Uso: python minic.py codigo.c', file=sys.stderr)
        return 2
    try:
        source = Path(sys.argv[1]).read_text(encoding='utf-8')
        program = parse(scan(source))
        errors = analyze(program, source)
    except (OSError, UnicodeError) as error:
        print(f'ERRO ENTRADA: {error}', file=sys.stderr)
        return 2
    except CompileError as error:
        print(error.render(), file=sys.stderr)
        return 1
    sys.stdout.write(render_diagnostics(errors))
    return 1 if errors else 0


if __name__ == '__main__': raise SystemExit(main())
