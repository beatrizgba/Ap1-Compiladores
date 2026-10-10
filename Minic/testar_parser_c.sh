#!/usr/bin/env bash
# Versão do projeto: compila o ponto de entrada C e valida os casos originais.
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
exec python3 "$project_dir/tests/validar_parser.py" --linguagem c "${1:-tests/parser}" "${2:-parser.c}"
