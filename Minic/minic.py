#!/usr/bin/env python3
"""Etapa 3: scanner da etapa 1 → parser da etapa 2 → análise semântica."""
import sys
from pathlib import Path
from src.common import CompileError, write_output
from src.scanner.scanner import scan
from src.parser.parser import parse
from src.semantico.analisador_semantico import analyze, render_diagnostics


def main():
    if len(sys.argv) != 2:
        write_output('Uso: python minic.py codigo.c\n', sys.stderr)
        return 2
    try:
        source = Path(sys.argv[1]).read_text(encoding='utf-8')
        program = parse(scan(source))
        errors = analyze(program, source)
    except (OSError, UnicodeError) as error:
        write_output(f'ERRO ENTRADA: {error}\n', sys.stderr)
        return 2
    except CompileError as error:
        write_output(error.render() + '\n', sys.stderr)
        return 1
    write_output(render_diagnostics(errors))
    return 1 if errors else 0


if __name__ == '__main__': raise SystemExit(main())
