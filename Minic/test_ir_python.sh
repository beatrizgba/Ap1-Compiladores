#!/usr/bin/env bash
# Verificador do projeto; o script original está em tests/ir/originais.
set -euo pipefail
[[ $# -eq 2 ]] || { echo "Uso: $0 minic.py diretorio_dos_testes" >&2; exit 2; }
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec "${PYTHON_BIN:-python3}" "$ROOT/tests/validar_ir.py" --language python --entry "$1" --suite "$2"
