#!/usr/bin/env python3
"""Entrada da etapa 1 com JSONL UTF-8, também no terminal do Windows."""
import sys

if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8', newline='\n')
    sys.stderr.reconfigure(encoding='utf-8', newline='\n')
    from src.lexer.lexer import main
    raise SystemExit(main())
