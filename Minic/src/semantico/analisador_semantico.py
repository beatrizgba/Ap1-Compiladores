"""Análise da mesma AST usada na etapa 2; acumula erros independentes."""
from dataclasses import dataclass


@dataclass
class Symbol:
    node: object
    typ: str
    function: bool = False


class Analyzer:
    def __init__(self, program, source):
        self.program, self.source = program, source
        self.scopes = [{}]
        self.errors = []
        self.function = None

    def text(self, n): return self.source[n.token.start:n.end].strip()

    def error(self, code, node, message):
        self.errors.append((node.line, node.column, code, message))

    def lookup(self, name):
        for scope in reversed(self.scopes):
            if name in scope: return scope[name]
        return None

    @staticmethod
    def typ(n): return n.type_name + ('[]' if n.array else '')

    @staticmethod
    def compatible(target, actual):
        return target == actual or (target == 'float' and actual == 'int')

    def declare(self, n):
        scope = self.scopes[-1]
        old = scope.get(n.value)
        if old:
            self.error('SEM002', n, f'“{n.value}” já declarado neste escopo; declaração anterior na linha {old.node.line}, coluna {old.node.column} (tipo {old.typ}).')
        else:
            scope[n.value] = Symbol(n, self.typ(n), n.kind == 'function')
        if n.type_name == 'void' and n.kind != 'function':
            self.error('SEM004', n, f'“{n.value}” não pode ter tipo void.')

    def analyze(self):
        for n in self.program.children:
            if n.kind in {'decl', 'function'}: self.declare(n)
        for n in self.program.children:
            if n.kind == 'function':
                self.function = n
                self.scopes.append({})
                for param in n.params: self.declare(param)
                for child in n.body.children: self.statement(child)
                if n.type_name != 'void' and not self.returns(n.body):
                    hint = 'há um caminho que alcança o fim do corpo.'
                    if n.body.children:
                        last = n.body.children[-1]
                        if last.kind == 'if' and last.otherwise is None and last.left.kind == 'id':
                            hint = f'o ramo em que “{last.left.value}” é falso alcança o fim do corpo.'
                    self.error('SEM011', n, f'A função “{n.value}” pode terminar sem retornar {n.type_name}; {hint}')
                self.scopes.pop()
                self.function = None
            elif n.kind == 'decl': self.check_decl(n)
            else: self.statement(n)
        self.errors.sort(key=lambda e: (e[0], e[1]))
        return self.errors

    def check_decl(self, n):
        if n.array:
            size_type = self.expression(n.size)
            if size_type != 'error' and (size_type != 'int' or self.constant(n.size) is None or self.constant(n.size) <= 0):
                self.error('SEM006', n.size, f'Tamanho do vetor “{n.value}” deve ser uma constante inteira positiva.')
        if n.initializer:
            actual = self.expression(n.initializer, 'inicialização')
            if actual != 'error' and (n.array or not self.compatible(n.type_name, actual)):
                self.assignment_error(n, n.initializer, actual)

    def assignment_error(self, target, expr, actual):
        self.error('SEM003', expr, f'Não é possível atribuir {actual} a {self.typ(target) if target.kind == "decl" else self.expression(target)} sem conversão permitida (destino “{target.value if target.kind == "decl" else self.text(target)}”; expressão “{self.text(expr)}”).')

    def statement(self, n):
        if n.kind == 'block':
            self.scopes.append({})
            for child in n.children: self.statement(child)
            self.scopes.pop()
        elif n.kind == 'decl':
            self.declare(n)
            self.check_decl(n)
        elif n.kind == 'expr': self.expression(n.left, 'comando' if n.left.kind == 'call' else 'expressão')
        elif n.kind in {'if', 'while'}:
            actual = self.expression(n.left, 'condição')
            if actual not in {'bool', 'error'}:
                self.error('SEM005', n.left, f'Condição de {n.kind} deve ter tipo bool; recebeu {actual} (expressão “{self.text(n.left)}”).')
            self.statement(n.body)
            if n.otherwise: self.statement(n.otherwise)
        elif n.kind == 'return':
            if self.function is None:
                self.error('SEM009', n, 'return fora de uma função.')
                return
            target = self.function.type_name
            actual = self.expression(n.left, 'retorno') if n.left else 'void'
            if actual != 'error' and (not self.compatible(target, actual) or (target == 'void' and n.left is not None)):
                suffix = f'; conversão implícita de {actual} para {target} não permitida.'
                self.error('SEM009', n.left or n, f'Retorno {actual} incompatível com o tipo {target} da função “{self.function.value}”{suffix}')

    def expression(self, n, context='expressão'):
        if n.kind == 'literal': return n.type_name
        if n.kind == 'id':
            sym = self.lookup(n.value)
            if sym is None:
                self.error('SEM001', n, f'Identificador “{n.value}” não declarado neste escopo.')
                return 'error'
            if sym.function:
                self.error('SEM004', n, f'“{n.value}” é função e não pode ser usada como variável.')
                return 'error'
            return sym.typ
        if n.kind == 'index':
            base = self.expression(n.left)
            index = self.expression(n.right)
            if index not in {'int', 'error'}:
                self.error('SEM006', n.right, f'Índice do vetor “{self.text(n.left)}” deve ser int; recebeu {index} (expressão “{self.text(n.right)}”).')
            if base == 'error' or index == 'error': return 'error'
            if not base.endswith('[]'):
                self.error('SEM006', n.left, f'“{self.text(n.left)}” não é um vetor.')
                return 'error'
            return base[:-2] if index == 'int' else 'error'
        if n.kind == 'call':
            sym = self.lookup(n.value)
            args = [self.expression(a, 'argumento') for a in n.children]
            if sym is None:
                self.error('SEM001', n, f'Identificador “{n.value}” não declarado neste escopo.')
                return 'error'
            if not sym.function:
                self.error('SEM007', n, f'“{n.value}” não é uma função.')
                return 'error'
            params = sym.node.params
            if len(args) != len(params):
                self.error('SEM007', n, f'“{n.value}” espera {len(params)} argumentos, mas recebeu {len(args)}.')
            for i, (arg, param) in enumerate(zip(args, params)):
                target = self.typ(param)
                if arg != 'error' and not self.compatible(target, arg):
                    expr = n.children[i]
                    self.error('SEM008', expr, f'Argumento {i+1} de “{n.value}”: esperado {target}, recebido {arg} (expressão “{self.text(expr)}”).')
            if sym.typ == 'void' and context != 'comando':
                use = 'expressão de atribuição' if context == 'atribuição' else context
                self.error('SEM012', n, f'Função “{n.value}” não produz valor (retorno void) e não pode ser usada como {use}.')
                return 'error'
            return sym.typ
        if n.kind == 'assign':
            left = self.expression(n.left)
            right = self.expression(n.right, 'atribuição')
            if n.left.kind not in {'id', 'index'} or left.endswith('[]'):
                description = f'o literal inteiro “{n.left.value}”' if n.left.kind == 'literal' and n.left.type_name == 'int' else f'“{self.text(n.left)}”'
                self.error('SEM013', n.left, f'Destino de atribuição não é atribuível; {description} não designa uma variável ou elemento de vetor.')
                return 'error'
            if left == 'error' or right == 'error': return 'error'
            if not self.compatible(left, right): self.assignment_error(n.left, n.right, right)
            return left
        if n.kind == 'unary':
            actual = self.expression(n.right)
            if actual == 'error': return 'error'
            allowed = {'bool'} if n.value == '!' else {'int', 'float'}
            if actual not in allowed:
                self.error('SEM004', n, f'Operador “{n.value}” não aceita operando {actual}.')
                return 'error'
            return actual
        if n.kind == 'binary':
            a, b = self.expression(n.left), self.expression(n.right)
            if 'error' in {a, b}: return 'error'
            numeric = a in {'int', 'float'} and b in {'int', 'float'}
            op = n.value
            valid = numeric
            if op == '%': valid = a == b == 'int'
            if op in {'&&', '||'}: valid = a == b == 'bool'
            if op in {'==', '!='}: valid = numeric or (a == b and a in {'bool', 'char'})
            if not valid:
                self.error('SEM004', n, f'Operador “{op}” não aceita operandos {a} e {b}.')
                return 'error'
            if op in {'<', '<=', '>', '>=', '==', '!=', '&&', '||'}: return 'bool'
            return 'float' if 'float' in {a, b} else 'int'
        return 'error'

    @staticmethod
    def returns(n):
        if n.kind == 'return': return True
        if n.kind == 'block': return any(Analyzer.returns(child) for child in n.children)
        if n.kind == 'if': return n.otherwise is not None and Analyzer.returns(n.body) and Analyzer.returns(n.otherwise)
        return False

    @staticmethod
    def constant(n):
        if n.kind == 'literal' and n.type_name == 'int': return int(n.value)
        if n.kind == 'unary' and n.value in {'+', '-'}:
            value = Analyzer.constant(n.right)
            return None if value is None else (-value if n.value == '-' else value)
        return None


def analyze(program, source): return Analyzer(program, source).analyze()


def render_diagnostics(errors):
    lines = [f'{code} — linha {line}, coluna {column}: {message}' for line, column, code, message in errors]
    total = len(errors)
    lines.append(f'Análise semântica concluída: {total} {"erro" if total == 1 else "erros"}; programa {"rejeitado" if total else "aceito"}.')
    # Os gabaritos enviados usam CRLF entre diagnósticos e nenhum newline final.
    return '\r\n'.join(lines)
