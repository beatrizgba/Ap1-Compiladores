#!/usr/bin/env bash
# Versão do projeto: usa os critérios de aceitação/rejeição do README dos testes.
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
exec python3 "$project_dir/tests/validar_parser.py" --linguagem py "${1:-tests/parser}" "${2:-parser.py}"
