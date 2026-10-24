"""AST das etapas 2–4, construída pelo parser ascendente SLR(1)."""
from src.parser.ascendente import parse


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
    if k == 'for': return f'For({f(n.initializer)}, {f(n.left)}, {f(n.right)}, {f(n.body)})'
    if k in {'break', 'continue'}: return k.capitalize() + '()'
    return 'EmptyStmt'
