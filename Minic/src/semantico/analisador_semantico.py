"""Análise da mesma AST usada na etapa 2; acumula erros independentes."""
from dataclasses import dataclass

NORMAL, RETURN, BREAK, CONTINUE = 1, 2, 4, 8
MIN_INT, MAX_INT = -(1 << 63), (1 << 63) - 1


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
        self.loop_depth = 0

    def text(self, n): return self.source[n.token.start:n.end].strip()

    def error(self, code, node, message):
        if len(self.errors) < 100:
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
            n.symbol = Symbol(n, self.typ(n), n.kind == 'function')
            scope[n.value] = n.symbol
        n.semantic_type = self.typ(n)
        n.category = 'function' if n.kind == 'function' else ('array' if n.array else n.kind)
        n.assignable = not n.array and n.kind != 'function' and n.type_name != 'void'
        if n.array and n.size:
            size = self.constant(n.size)
            n.array_size = size if size is not None and size > 0 else None
        if n.type_name == 'void' and n.kind != 'function':
            n.semantic_type = 'error'
            self.error('SEM004', n, f'“{n.value}” não pode ter tipo void.')

    def analyze(self):
        for n in self.program.children:
            if n.kind in {'decl', 'function'}: self.declare(n)
        for n in self.program.children:
            if n.kind == 'function':
                self.function = n
                self.scopes.append({})
                for param in n.params: self.declare(param)
                self.block(n.body, new_scope=False)
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
        self.program.semantic_type = 'error' if self.errors else 'void'
        self.program.category = 'program'
        return self.errors

    def check_decl(self, n):
        if n.array:
            n.size.expected_type = 'int'
            size_type = self.expression(n.size)
            if size_type != 'error' and (size_type != 'int' or self.constant(n.size) is None or self.constant(n.size) <= 0):
                self.error('SEM006', n.size, f'Tamanho do vetor “{n.value}” deve ser uma constante inteira positiva.')
        if n.initializer:
            n.initializer.expected_type = n.type_name
            actual = self.expression(n.initializer, 'inicialização')
            if actual != 'error' and (n.array or not self.compatible(n.type_name, actual)):
                self.assignment_error(n, n.initializer, actual)
            elif actual != 'error': self.promote(n.initializer, n.type_name, actual)

    def assignment_error(self, target, expr, actual):
        self.error('SEM003', expr, f'Não é possível atribuir {actual} a {self.typ(target) if target.kind == "decl" else self.expression(target)} sem conversão permitida (destino “{target.value if target.kind == "decl" else self.text(target)}”; expressão “{self.text(expr)}”).')

    def block(self, n, new_scope=True):
        n.semantic_type, n.category, n.loop_depth = 'void', 'command', self.loop_depth
        if new_scope: self.scopes.append({})
        n.flow = NORMAL
        for child in n.children:
            self.statement(child)
            if n.flow & NORMAL:
                n.flow = (n.flow & ~NORMAL) | child.flow
        if new_scope: self.scopes.pop()

    def statement(self, n):
        n.semantic_type, n.category, n.loop_depth, n.flow = 'void', 'command', self.loop_depth, NORMAL
        if n.kind == 'block': self.block(n)
        elif n.kind == 'decl':
            self.declare(n)
            self.check_decl(n)
        elif n.kind == 'expr': self.expression(n.left, 'comando')
        elif n.kind in {'if', 'while'}:
            n.left.expected_type = 'bool'
            actual = self.expression(n.left, 'condição')
            if actual not in {'bool', 'error'}:
                self.error('SEM005', n.left, f'Condição de {n.kind} deve ter tipo bool; recebeu {actual} (expressão “{self.text(n.left)}”).')
            if n.kind == 'while': self.loop_depth += 1
            self.statement(n.body)
            if n.kind == 'while': self.loop_depth -= 1
            if n.otherwise: self.statement(n.otherwise)
            n.flow = (NORMAL | (n.body.flow & RETURN)) if n.kind == 'while' else (n.body.flow | (n.otherwise.flow if n.otherwise else NORMAL))
        elif n.kind in {'break', 'continue'}:
            if not self.loop_depth:
                self.error('SEM010', n, f'{n.kind} só pode ocorrer dentro de um laço.')
            else: n.flow = BREAK if n.kind == 'break' else CONTINUE
        elif n.kind == 'return':
            n.flow = RETURN
            if self.function is None:
                n.semantic_type = 'error'
                if n.left: self.expression(n.left, 'retorno')
                self.error('SEM009', n, 'return fora de uma função.')
                return
            target = self.function.type_name
            n.expected_type = target
            if n.left: n.left.expected_type = target
            actual = self.expression(n.left, 'retorno') if n.left else 'void'
            if actual != 'error' and (not self.compatible(target, actual) or (target == 'void' and n.left is not None)):
                suffix = f'; conversão implícita de {actual} para {target} não permitida.'
                self.error('SEM009', n.left or n, f'Retorno {actual} incompatível com o tipo {target} da função “{self.function.value}”{suffix}')
            elif n.left and actual != 'error': self.promote(n.left, target, actual)

    def expression(self, n, context='expressão'):
        n.loop_depth = self.loop_depth
        result = self.expression_type(n, context)
        n.semantic_type = result
        if not n.category: n.category = 'value'
        if result == 'error': n.category, n.assignable = 'error', False
        return result

    @staticmethod
    def promote(n, target, actual):
        if target == 'float' and actual == 'int': n.conversion_to = 'float'

    def expression_type(self, n, context):
        if n.kind == 'literal':
            n.category = 'literal'
            return n.type_name
        if n.kind == 'id':
            sym = self.lookup(n.value)
            if sym is None:
                self.error('SEM001', n, f'Identificador “{n.value}” não declarado neste escopo.')
                return 'error'
            n.symbol, n.category = sym, sym.node.category
            n.array_size = sym.node.array_size
            n.assignable = sym.node.assignable
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
            if index != 'int': return 'error'
            n.symbol = n.left.symbol
            n.category, n.assignable = 'element', True
            value = self.constant(n.right)
            if value is not None and (value < 0 or (n.left.array_size is not None and value >= n.left.array_size)):
                size = str(n.left.array_size) if n.left.array_size is not None else 'desconhecido'
                self.error('SEM006', n.right, f'Índice {value} fora dos limites do vetor “{self.text(n.left)}” (tamanho {size}).')
                return 'error'
            return base[:-2]
        if n.kind == 'call':
            sym = self.lookup(n.value)
            args = [self.expression(a, 'argumento') for a in n.children]
            if sym is None:
                self.error('SEM001', n, f'Identificador “{n.value}” não declarado neste escopo.')
                return 'error'
            if not sym.function:
                self.error('SEM007', n, f'“{n.value}” não é uma função.')
                return 'error'
            n.symbol, n.category = sym, 'call'
            params = sym.node.params
            if len(args) != len(params):
                self.error('SEM007', n, f'“{n.value}” espera {len(params)} argumentos, mas recebeu {len(args)}.')
            for i, (arg, param) in enumerate(zip(args, params)):
                target = self.typ(param)
                n.children[i].expected_type = target
                if arg != 'error' and not self.compatible(target, arg):
                    expr = n.children[i]
                    self.error('SEM008', expr, f'Argumento {i+1} de “{n.value}”: esperado {target}, recebido {arg} (expressão “{self.text(expr)}”).')
                elif arg != 'error': self.promote(n.children[i], target, arg)
            if sym.typ == 'void' and context != 'comando':
                use = 'expressão de atribuição' if context == 'atribuição' else context
                self.error('SEM012', n, f'Função “{n.value}” não produz valor (retorno void) e não pode ser usada como {use}.')
                return 'error'
            return sym.typ
        if n.kind == 'assign':
            left = self.expression(n.left)
            # O caso oficial 09 exige a = b = 3 como comando.
            right_context = 'cadeia' if n.right.kind == 'assign' and context in {'comando', 'cadeia'} else 'atribuição'
            right = self.expression(n.right, right_context)
            if n.left.kind not in {'id', 'index'} or left.endswith('[]'):
                description = f'o literal inteiro “{n.left.value}”' if n.left.kind == 'literal' and n.left.type_name == 'int' else f'“{self.text(n.left)}”'
                self.error('SEM013', n.left, f'Destino de atribuição não é atribuível; {description} não designa uma variável ou elemento de vetor.')
                return 'error'
            if left == 'error' or right == 'error': return 'error'
            n.expected_type = n.right.expected_type = left
            if not self.compatible(left, right):
                self.assignment_error(n.left, n.right, right)
                return 'error'
            self.promote(n.right, left, right)
            if context not in {'comando', 'cadeia'}:
                self.error('SEM004', n, f'Atribuição não produz valor e não pode ser usada como {context}.')
                return 'error'
            n.category = 'command'
            return left if context == 'cadeia' else 'void'
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
            if numeric and 'float' in {a, b}:
                self.promote(n.left, 'float', a)
                self.promote(n.right, 'float', b)
            if op in {'<', '<=', '>', '>=', '==', '!=', '&&', '||'}: return 'bool'
            return 'float' if 'float' in {a, b} else 'int'
        return 'error'

    @staticmethod
    def returns(n):
        return n.flow == RETURN

    @staticmethod
    def constant(n):
        value = None
        if n.kind == 'literal' and n.type_name == 'int':
            try: value = int(n.value)
            except ValueError: return None
        if n.kind == 'unary' and n.value in {'+', '-'}:
            value = Analyzer.constant(n.right)
            if value is not None and n.value == '-': value = -value
        if n.kind == 'binary' and n.value in {'+', '-', '*', '/', '%'}:
            left, right = Analyzer.constant(n.left), Analyzer.constant(n.right)
            if left is None or right is None: return None
            if n.value == '+': value = left + right
            elif n.value == '-': value = left - right
            elif n.value == '*': value = left * right
            elif right == 0 or (left == MIN_INT and right == -1): return None
            else:
                quotient = (abs(left) // abs(right)) * (-1 if (left < 0) != (right < 0) else 1)
                value = quotient if n.value == '/' else left - quotient * right
        return value if value is not None and MIN_INT <= value <= MAX_INT else None


def analyze(program, source): return Analyzer(program, source).analyze()


def render_diagnostics(errors):
    lines = [f'{code} — linha {line}, coluna {column}: {message}' for line, column, code, message in errors]
    total = len(errors)
    lines.append(f'Análise semântica concluída: {total} {"erro" if total == 1 else "erros"}; programa {"rejeitado" if total else "aceito"}.')
    # Os gabaritos enviados usam CRLF entre diagnósticos e nenhum newline final.
    return '\r\n'.join(lines)
