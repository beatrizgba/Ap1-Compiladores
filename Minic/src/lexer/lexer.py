import json
import sys
from pathlib import Path

KEYWORDS = {
    "int": "INT", "float": "FLOAT", "bool": "BOOL", "char": "CHAR",
    "void": "VOID", "if": "IF", "else": "ELSE", "while": "WHILE",
    "for": "FOR", "return": "RETURN", "break": "BREAK",
    "continue": "CONTINUE", "true": "TRUE", "false": "FALSE",
    "print": "PRINT", "read": "READ",
}

TWO_CHAR_TOKENS = {
    "==": "EQ", "!=": "NE", "<=": "LE", ">=": "GE",
    "&&": "AND", "||": "OR",
}

ONE_CHAR_TOKENS = {
    "=": "ASSIGN", "<": "LT", ">": "GT", "+": "PLUS", "-": "MINUS",
    "*": "STAR", "/": "SLASH", "%": "PERCENT", "!": "NOT",
    "(": "LPAREN", ")": "RPAREN", "[": "LBRACKET", "]": "RBRACKET",
    "{": "LBRACE", "}": "RBRACE", ";": "SEMICOLON", ",": "COMMA",
    ".": "DOT",
}

def token(token, lexeme, attribute, line, column):
    return {"token": token, "lexeme": lexeme, "attribute": attribute,
            "line": line, "column": column}

def error(error_type, lexeme, line, column):
    return {"error": error_type, "lexeme": lexeme,
            "line": line, "column": column}

def emit(obj, stream):
    print(json.dumps(obj, ensure_ascii=False, separators=(",", ":")), file=stream)

def decode_literal(raw):
    value = raw[1:-1]
    escapes = {
        "\\n": "\n", "\\r": "\r", "\\t": "\t",
        "\\\\": "\\", '\\"': '"', "\\'": "'",
    }
    for old, new in escapes.items():
        value = value.replace(old, new)
    return value

def tokenize(code, token_stream=sys.stdout, error_stream=sys.stderr):
    i, line, column = 0, 1, 1
    n = len(code)

    def advance(text):
        nonlocal line, column
        if "\n" in text:
            line += text.count("\n")
            column = len(text.rsplit("\n", 1)[1]) + 1
        else:
            column += len(text)

    while i < n:
        start_line, start_column = line, column
        ch = code[i]

        if ch in " \t\r":
            j = i + 1
            while j < n and code[j] in " \t\r":
                j += 1
            advance(code[i:j]); i = j; continue

        if ch == "\n":
            advance("\n"); i += 1; continue

        if code.startswith("//", i):
            j = code.find("\n", i)
            if j == -1: j = n
            advance(code[i:j]); i = j; continue

        if code.startswith("/*", i):
            end = code.find("*/", i + 2)
            if end == -1:
                text = code[i:]
                emit(error("UNTERMINATED_BLOCK_COMMENT", text,
                           start_line, start_column), error_stream)
                advance(text); i = n; continue
            text = code[i:end + 2]
            advance(text); i = end + 2; continue

        if ch == '"':
            j, escaped, closed = i + 1, False, False
            while j < n and code[j] != "\n":
                if code[j] == '"' and not escaped:
                    closed = True
                    break
                if code[j] == "\\" and not escaped:
                    escaped = True
                else:
                    escaped = False
                j += 1
            if not closed:
                text = code[i:j]
                emit(error("UNTERMINATED_STRING_LITERAL", text,
                           start_line, start_column), error_stream)

                # O diagnóstico registra todo o trecho até a quebra de linha,
                # mas os delimitadores que aparecem depois do texto inválido
                # continuam sendo analisados normalmente.
                resume = j
                for k in range(i + 1, j):
                    if code[k] in ");":
                        resume = k
                        break

                consumed = code[i:resume]
                advance(consumed)
                i = resume
                continue
            text = code[i:j + 1]
            emit(token("STRING_LIT", text, decode_literal(text),
                       start_line, start_column), token_stream)
            advance(text); i = j + 1; continue

        if ch == "'":
            j, escaped, closed = i + 1, False, False
            while j < n and code[j] not in "\n\r;,)]}":
                if code[j] == "'" and not escaped:
                    closed = True
                    break
                if code[j] == "\\" and not escaped:
                    escaped = True
                else:
                    escaped = False
                j += 1
            if not closed:
                text = code[i:j]
                emit(error("UNTERMINATED_CHAR_LITERAL", text,
                           start_line, start_column), error_stream)
                if j < n and code[j] == ";":
                    advance(code[i:j + 1]); i = j + 1
                else:
                    advance(text); i = j
                continue
            text = code[i:j + 1]
            emit(token("CHAR_LIT", text, decode_literal(text),
                       start_line, start_column), token_stream)
            advance(text); i = j + 1; continue

        if ch.isdigit():
            j = i
            while j < n and code[j].isdigit():
                j += 1

            if j < n and code[j] == "." and (
                j + 1 >= n or not code[j + 1].isdigit()
            ):
                malformed = code[i:j + 1]
                emit(error("MALFORMED_REAL_LITERAL", malformed,
                           start_line, start_column), error_stream)
                text = code[i:j]
                emit(token("INT_LIT", text, int(text),
                           start_line, start_column), token_stream)
                advance(text); i = j; continue

            if j < n and (code[j].isalpha() or code[j] == "_"):
                k = j
                while k < n and (code[k].isalnum() or code[k] == "_"):
                    k += 1
                malformed = code[i:k]
                emit(error("INVALID_IDENTIFIER", malformed,
                           start_line, start_column), error_stream)

            if j < n and code[j] == "." and j + 1 < n and code[j + 1].isdigit():
                k = j + 1
                while k < n and code[k].isdigit():
                    k += 1
                text = code[i:k]
                emit(token("FLOAT_LIT", text, float(text),
                           start_line, start_column), token_stream)
                advance(text); i = k; continue

            text = code[i:j]
            emit(token("INT_LIT", text, int(text),
                       start_line, start_column), token_stream)
            advance(text); i = j; continue

        if ch.isalpha() or ch == "_":
            j = i + 1
            while j < n and (code[j].isalnum() or code[j] == "_"):
                j += 1
            text = code[i:j]
            tok = KEYWORDS.get(text, "IDENT")
            attr = None if tok != "IDENT" else text
            emit(token(tok, text, attr, start_line, start_column), token_stream)
            advance(text); i = j; continue

        two = code[i:i + 2]
        if two in TWO_CHAR_TOKENS:
            emit(token(TWO_CHAR_TOKENS[two], two, None,
                       start_line, start_column), token_stream)
            advance(two); i += 2; continue

        if ch in ONE_CHAR_TOKENS:
            emit(token(ONE_CHAR_TOKENS[ch], ch, None,
                       start_line, start_column), token_stream)
            advance(ch); i += 1; continue

        emit(error("UNKNOWN_SYMBOL", ch, start_line, start_column), error_stream)
        advance(ch); i += 1

    emit(token("EOF", "", None, line, column), token_stream)

def main():
    if len(sys.argv) != 2:
        print("Uso: python lexer.py <arquivo>", file=sys.stderr)
        return 1
    path = Path(sys.argv[1])
    try:
        code = path.read_text(encoding="utf-8")
    except FileNotFoundError:
        print(f"Erro: arquivo '{path}' não encontrado.", file=sys.stderr)
        return 1
    tokenize(code)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
