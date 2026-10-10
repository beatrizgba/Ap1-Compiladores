"""Contrato comum às três etapas: tokens, AST e diagnósticos."""
from dataclasses import dataclass, field


@dataclass
class Token:
    kind: str
    text: str
    line: int
    column: int
    start: int
    end: int


@dataclass
class Node:
    kind: str
    token: Token
    end: int
    value: str = ''
    type_name: str = ''
    children: list = field(default_factory=list)
    left: 'Node | None' = None
    right: 'Node | None' = None
    body: 'Node | None' = None
    otherwise: 'Node | None' = None
    size: 'Node | None' = None
    initializer: 'Node | None' = None
    params: list = field(default_factory=list)
    array: bool = False

    @property
    def line(self): return self.token.line

    @property
    def column(self): return self.token.column


class CompileError(Exception):
    def __init__(self, stage, token, message):
        self.stage, self.token, self.message = stage, token, message
        super().__init__(message)

    def render(self):
        return f'ERRO {self.stage} linha {self.token.line}, coluna {self.token.column}: {self.message}'
