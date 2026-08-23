import difflib
import os
import shutil
import subprocess
import sys
from pathlib import Path

TESTS = Path(__file__).resolve().parent
ROOT = TESTS.parent
SRC = ROOT / "src" / "lexer"

PROGRAMS = TESTS / "casos-programas-c"
INVALIDOS = TESTS / "casos-invalidos"
RESULTS = TESTS / "resultados"


def decode_output(data):
    if not data:
        return ""
    if isinstance(data, str):
        return data
    for encoding in ("utf-8", "cp1252", "latin-1"):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            pass
    return data.decode("utf-8", errors="replace")


def write_output(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text or "", encoding="utf-8")


def compare(actual, expected, label):
    actual = (actual or "").replace("\r\n", "\n").strip()
    expected = (expected or "").replace("\r\n", "\n").strip()

    if actual == expected:
        return True

    print(f"\n  Diferença em {label}:")
    diff = difflib.unified_diff(
        expected.splitlines(),
        actual.splitlines(),
        fromfile="ESPERADO",
        tofile="PRODUZIDO",
        lineterm=""
    )
    print("\n".join(diff))
    return False


def expected_path(source):
    # Ex.: c01_fibonacci.c -> c01_fibonacci.c.expected.jsonl
    # Ex.: i01_x.minic -> i01_x.expected.jsonl
    if source.suffix == ".c":
        return source.with_name(source.name + ".expected.jsonl")
    return source.with_name(source.stem + ".expected.jsonl")


def errors_path(source):
    # Ex.: i01_x.minic -> i01_x.errors.jsonl
    return source.with_name(source.stem + ".errors.jsonl")


def run_process(command, source):
    process = subprocess.run(
        command + [str(source)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )
    stdout = decode_output(process.stdout)
    stderr = decode_output(process.stderr)

    # O ambiente Python da execução pode adicionar mensagens externas
    # no stderr. O analisador, por contrato, produz diagnósticos em JSONL.
    # Mantemos somente as linhas JSON para a comparação.
    json_lines = [
        line for line in stderr.splitlines()
        if line.lstrip().startswith("{")
    ]
    stderr = "\n".join(json_lines)
    if json_lines:
        stderr += "\n"

    return process.returncode, stdout, stderr


def test_case(command, source, result_dir):
    expected = expected_path(source)
    errors = errors_path(source)

    if not expected.exists():
        print(f"[AVISO] Expected não encontrado: {expected.name}")
        return False

    returncode, stdout, stderr = run_process(command, source)

    token_file = result_dir / f"{source.name}.tokens.jsonl"
    error_file = result_dir / f"{source.name}.errors.jsonl"

    write_output(token_file, stdout)
    write_output(error_file, stderr)

    tokens_ok = compare(
        stdout,
        expected.read_text(encoding="utf-8"),
        f"tokens de {source.name}"
    )

    # Todo fixture possui expected; os inválidos também possuem errors.
    if errors.exists():
        errors_ok = compare(
            stderr,
            errors.read_text(encoding="utf-8"),
            f"erros de {source.name}"
        )
    else:
        errors_ok = stderr.strip() == ""

    if returncode != 0:
        print(f"  Programa terminou com código {returncode}.")

    return returncode == 0 and tokens_ok and errors_ok


def compile_c():
    compiler = shutil.which("gcc")
    if compiler is None:
        print("[ERRO] GCC não encontrado no PATH.")
        return None

    lexer = SRC / "lexer.c"
    executable = RESULTS / ("lexer.exe" if os.name == "nt" else "lexer")

    command = [
        compiler,
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-pedantic",
        str(lexer),
        "-o",
        str(executable),
    ]

    print("\nCompilando lexer.c...")
    process = subprocess.run(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )

    stdout = decode_output(process.stdout)
    stderr = decode_output(process.stderr)

    if stdout.strip():
        print(stdout)
    if stderr.strip():
        print(stderr)

    if process.returncode != 0:
        print("[ERRO] Falha na compilação do lexer.c.")
        return None

    print("[OK] lexer.c compilado com sucesso.")
    return executable


def clean_results():
    if RESULTS.exists():
        for child in RESULTS.iterdir():
            if child.is_dir():
                shutil.rmtree(child)
            else:
                child.unlink()
    RESULTS.mkdir(parents=True, exist_ok=True)


def test_python():
    print("\nPYTHON")
    print("-" * 60)

    lexer = SRC / "lexer.py"
    result_dir = RESULTS / "python"
    ok = True
    count = 0

    for source in sorted(PROGRAMS.glob("*.c")):
        count += 1
        result = test_case(
            [sys.executable, str(lexer)],
            source,
            result_dir
        )
        print(f"[{'OK' if result else 'ERRO'}] {source.name}")
        ok = ok and result

    for source in sorted(INVALIDOS.glob("*.minic")):
        count += 1
        result = test_case(
            [sys.executable, str(lexer)],
            source,
            result_dir
        )
        print(f"[{'OK' if result else 'ERRO'}] {source.name}")
        ok = ok and result

    # Há um fixture i06 no pacote do professor, mas não há o .minic correspondente.
    i06_expected = INVALIDOS / "i06_identificador_iniciado_por_digito.expected.jsonl"
    i06_source = INVALIDOS / "i06_identificador_iniciado_por_digito.minic"
    if i06_expected.exists() and not i06_source.exists():
        print("[AVISO] i06_identificador_iniciado_por_digito: expected/errors existem, mas o arquivo .minic não foi fornecido.")
        print("        Esse caso não pode ser executado automaticamente.")

    if count == 0:
        print("[ERRO] Nenhum teste encontrado.")
        return False

    return ok


def test_c():
    print("\nC")
    print("-" * 60)

    executable = compile_c()
    if executable is None:
        return False

    result_dir = RESULTS / "c"
    ok = True
    count = 0

    for source in sorted(PROGRAMS.glob("*.c")):
        count += 1
        result = test_case(
            [str(executable)],
            source,
            result_dir
        )
        print(f"[{'OK' if result else 'ERRO'}] {source.name}")
        ok = ok and result

    for source in sorted(INVALIDOS.glob("*.minic")):
        count += 1
        result = test_case(
            [str(executable)],
            source,
            result_dir
        )
        print(f"[{'OK' if result else 'ERRO'}] {source.name}")
        ok = ok and result

    if count == 0:
        print("[ERRO] Nenhum teste encontrado.")
        return False

    return ok


def main():
    print("=" * 60)
    print("TESTES DO ANALISADOR LÉXICO")
    print("=" * 60)
    print(f"\nProjeto: {ROOT}")
    print(f"Testes:  {TESTS}")
    print(f"Lexer:   {SRC}")

    clean_results()

    python_ok = test_python()
    c_ok = test_c()

    print("\n" + "=" * 60)
    print("RESULTADO FINAL")
    print("=" * 60)
    print(f"Python: {'TODOS OS TESTES OK' if python_ok else 'FALHOU'}")
    print(f"C:      {'TODOS OS TESTES OK' if c_ok else 'FALHOU'}")

    write_output(
        RESULTS / "resultado_testes.txt",
        "RESULTADO DOS TESTES DO ANALISADOR LÉXICO\n"
        "==========================================\n\n"
        f"Python: {'TODOS OS TESTES OK' if python_ok else 'FALHOU'}\n"
        f"C:      {'TODOS OS TESTES OK' if c_ok else 'FALHOU'}\n"
    )

    return 0 if python_ok and c_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
