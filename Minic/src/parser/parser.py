"""Parser comum às etapas 2 e 3, independente de verificações semânticas."""
from src.common import Node, CompileError

TYPES = {'int', 'float', 'bool', 'char', 'void'}
PRECEDENCE = {'||': 1, '&&': 2, '==': 3, '!=': 3, '<': 4, '<=': 4,
              '>': 4, '>=': 4, '+': 5, '-': 5, '*': 6, '/': 6, '%': 6}


class Parser:
    def __init__(self, tokens):
        self.tokens, self.pos = tokens, 0

    @property
    def current(self): return self.tokens[self.pos]

    def take(self):
        token = self.current
        if token.kind != 'EOF': self.pos += 1
        return token

    def match(self, text):
        if self.current.text == text:
            return self.take()
        return None

    def fail(self, message):
        raise CompileError('SINTATICO', self.current, message)

    def expect(self, text):
        token = self.match(text)
        if token is None: self.fail(f"esperado '{text}'")
        return token

    def identifier(self):
        if self.current.kind != 'IDENT': self.fail('identificador esperado')
        return self.take()

    def typ(self):
        if self.current.text not in TYPES: self.fail('tipo esperado')
        return self.take()

    def parse(self):
        start = self.current
        children = []
        while self.current.kind != 'EOF':
            children.extend(self.declaration(True) if self.current.text in TYPES else [self.statement()])
        return Node('program', start, self.current.end, children=children)

    def declaration(self, global_scope=False):
        typ = self.typ()
        name = self.identifier()
        if global_scope and self.match('('):
            params = []
            if self.current.text == 'void' and self.tokens[self.pos + 1].text == ')': self.take()
            elif self.current.text != ')':
                while True:
                    pt = self.typ()
                    pn = self.identifier()
                    array = bool(self.match('['))
                    if array: self.expect(']')
                    params.append(Node('param', pn, pn.end, value=pn.text, type_name=pt.text, array=array))
                    if not self.match(','): break
            self.expect(')')
            # Na gramática enviada, uma função precisa ter corpo: caso 48.
            body = self.block()
            return [Node('function', typ, body.end, value=name.text, type_name=typ.text, params=params, body=body)]
        result = []
        while True:
            array = bool(self.match('['))
            size = self.expression() if array else None
            if array: self.expect(']')
            initializer = self.expression() if self.match('=') else None
            result.append(Node('decl', name, self.tokens[self.pos - 1].end, value=name.text,
                               type_name=typ.text, array=array, size=size, initializer=initializer))
            if not self.match(','): break
            name = self.identifier()
        self.expect(';')
        return result

    def block(self):
        start = self.expect('{')
        children = []
        while self.current.text != '}':
            if self.current.kind == 'EOF': self.fail("esperado '}'")
            children.extend(self.declaration() if self.current.text in TYPES else [self.statement()])
        return Node('block', start, self.take().end, children=children)

    def statement(self):
        start = self.current
        if start.text == '{': return self.block()
        if self.match('if'):
            self.expect('(')
            cond = self.expression()
            self.expect(')')
            body = self.statement()
            other = self.statement() if self.match('else') else None
            return Node('if', start, (other or body).end, left=cond, body=body, otherwise=other)
        if self.match('while'):
            self.expect('(')
            cond = self.expression()
            self.expect(')')
            body = self.statement()
            return Node('while', start, body.end, left=cond, body=body)
        if self.match('return'):
            expr = None if self.current.text == ';' else self.expression()
            return Node('return', start, self.expect(';').end, left=expr)
        if self.match(';'): return Node('empty', start, start.end)
        expr = self.expression()
        return Node('expr', start, self.expect(';').end, left=expr)

    def expression(self):
        left = self.binary(1)
        if self.match('='):
            right = self.expression()
            return Node('assign', left.token, right.end, left=left, right=right)
        return left

    def binary(self, minimum):
        left = self.unary()
        while PRECEDENCE.get(self.current.text, 0) >= minimum:
            op = self.take()
            right = self.binary(PRECEDENCE[op.text] + 1)
            left = Node('binary', left.token, right.end, value=op.text, left=left, right=right)
        return left

    def unary(self):
        if self.current.text in {'!', '-', '+'}:
            op = self.take()
            right = self.unary()
            return Node('unary', op, right.end, value=op.text, right=right)
        return self.primary()

    def primary(self):
        start = self.current
        types = {'INT_LIT': 'int', 'FLOAT_LIT': 'float', 'CHAR_LIT': 'char', 'STRING_LIT': 'string', 'TRUE': 'bool', 'FALSE': 'bool'}
        if start.kind in types:
            self.take()
            return Node('literal', start, start.end, value=start.text, type_name=types[start.kind])
        if self.match('('):
            value = self.expression()
            self.expect(')')
            return value
        if start.kind == 'IDENT' or start.kind in {'PRINT', 'READ'}:
            self.take()
            value = Node('id', start, start.end, value=start.text)
            if self.match('('):
                args = []
                if self.current.text != ')':
                    while True:
                        args.append(self.expression())
                        if not self.match(','): break
                end = self.expect(')').end
                value = Node('call', start, end, value=start.text, children=args)
            while self.match('['):
                index = self.expression()
                value = Node('index', start, self.expect(']').end, left=value, right=index)
            return value
        self.fail('expressão esperada')


def parse(tokens): return Parser(tokens).parse()


def render_ast(n):
    if n is None: return 'NULL'
    k = n.kind
    f = render_ast
    if k == 'program': return 'Program(' + ', '.join(map(f, n.children)) + ')'
    if k == 'function':
        params = ','.join(p.type_name + ' ' + p.value + ('[]' if p.array else '') for p in n.params)
        return f'Function({n.type_name} {n.value}({params}) {f(n.body)})'
    if k == 'block': return 'Block(' + ', '.join(map(f, n.children)) + ')'
    if k == 'decl':
        suffix = ' size=' + f(n.size) if n.array else ''
        if n.initializer: suffix += ' = ' + f(n.initializer)
        return f'VarDecl({n.type_name} {n.value}{suffix})'
    if k == 'literal': return f'Lit({"real" if n.type_name == "float" else n.type_name},{n.value})'
    if k == 'id': return f'Id({n.value})'
    if k == 'call': return 'Call(Id(' + n.value + ')' + ''.join(',' + f(a) for a in n.children) + ')'
    if k == 'index': return f'Index({f(n.left)},{f(n.right)})'
    if k == 'binary': return f'Binary({n.value}, {f(n.left)}, {f(n.right)})'
    if k == 'unary': return f'Unary({n.value},{f(n.right)})'
    if k == 'assign': return f'Assign({f(n.left)}, {f(n.right)})'
    if k == 'expr': return f'ExprStmt({f(n.left)})'
    if k == 'return': return f'Return({f(n.left)})'
    if k == 'if': return f'If({f(n.left)}, {f(n.body)}, {f(n.otherwise)})'
    if k == 'while': return f'While({f(n.left)}, {f(n.body)})'
    return 'EmptyStmt'
