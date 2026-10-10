"""Adaptador da etapa 1; não mantém uma segunda implementação léxica."""
import io
import json
from src.common import Token, CompileError
from src.lexer.lexer import tokenize


def scan(source):
    out, errors = io.StringIO(), io.StringIO()
    tokenize(source, out, errors)
    lines = source.splitlines(keepends=True)
    offsets, offset = [0], 0
    for line in lines:
        offset += len(line)
        offsets.append(offset)

    def position(item):
        return min(len(source), offsets[min(item['line'] - 1, len(offsets) - 1)] + item['column'] - 1)

    diagnostics = errors.getvalue().splitlines()
    if diagnostics:
        item = json.loads(diagnostics[0])
        start = position(item)
        token = Token('ERROR', item['lexeme'], item['line'], item['column'], start, start)
        raise CompileError('LEXICO', token, item['error'])
    result = []
    for line in out.getvalue().splitlines():
        item = json.loads(line)
        start = position(item)
        token = Token(item['token'], item['lexeme'], item['line'], item['column'], start, start + len(item['lexeme']))
        # A especificação Flex da etapa 1 usa identificadores ASCII.
        if token.kind == 'IDENT' and not token.text.isascii():
            index = next(i for i, char in enumerate(token.text) if not char.isascii())
            bad = Token('ERROR', token.text[index], token.line, token.column + index,
                        token.start + index, token.start + index + 1)
            raise CompileError('LEXICO', bad, 'UNKNOWN_SYMBOL')
        result.append(token)
    return result
