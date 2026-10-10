#!/usr/bin/env python3
"""Etapa 2: lê fonte MINIC e imprime AST somente após análise completa."""
import sys
from pathlib import Path
from src.common import CompileError, write_output
from src.scanner.scanner import scan
from src.parser.parser import parse, render_ast


def main():
    if len(sys.argv) != 2:
        write_output('Uso: python parser.py codigo.c\n', sys.stderr)
        return 2
    try:
        source = Path(sys.argv[1]).read_text(encoding='utf-8')
        write_output(render_ast(parse(scan(source))) + '\n')
    except (OSError, UnicodeError) as error:
        write_output(f'ERRO ENTRADA: {error}\n', sys.stderr)
        return 2
    except CompileError as error:
        write_output(error.render() + '\n', sys.stderr)
        return 1
    return 0


if __name__ == '__main__': raise SystemExit(main())
