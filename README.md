# Compilador MINIC — etapas 1, 2 e 3

Projeto da disciplina de Compiladores, com versões em Python e C. As três
etapas compõem um único frontend: o scanner fornece tokens, o parser constrói
uma árvore sintática (AST), e o analisador semântico verifica essa mesma árvore.

Integrantes: Ana Clara T. Coser, Beatriz Gabrielly Batista de Almeida e
Vinicius Henrique Santos.

```mermaid
flowchart LR
    Fonte[Código MINIC] --> Scanner[Etapa 1: scanner]
    Scanner --> Tokens[Tokens com lexema e posição]
    Tokens --> Parser[Etapa 2: parser]
    Parser --> AST[Árvore sintática]
    AST --> Semantico[Etapa 3: analisador semântico]
    Semantico --> Resultado[Diagnósticos e resultado]
```

## Organização

```text
Minic/
├── parser.py / parser.c       # entrada da etapa 2
├── minic.py / minic.c         # entrada da etapa 3
├── Makefile
├── src/
│   ├── lexer/                # fontes originais da etapa 1, preservados
│   ├── scanner/              # adaptadores das mesmas regras da etapa 1
│   ├── parser/               # parser compartilhado entre etapas 2 e 3
│   ├── semantico/            # análise da AST, em C e Python
│   └── common.py / common.h / common.c
├── tools/gerar_scanner.py     # gera o scanner C a partir de lexer.l
├── testes_semanticos_py.sh    # script original do professor
├── testes_semanticos_c.sh     # script original do professor
└── tests/
    ├── casos-invalidos/      # fixtures léxicos
    ├── casos-programas-c/    # fixtures léxicos
    ├── parser/              # 50 casos sintáticos fornecidos
    ├── semantico/           # 20 casos e gabaritos fornecidos
    ├── regressao/           # casos adicionais
    └── validar_compilador.py # valida as três etapas e C × Python
```

## Preparar e executar

Python 3 não precisa de pacotes externos; PLY não é utilizado. A versão C
precisa de GCC. Os scripts oficiais precisam de Bash (no Windows, pode ser
usado Git Bash ou WSL). `make` facilita a compilação, mas é opcional. O C
gerado está versionado; Flex só é necessário ao alterar as regras léxicas.
Os pontos de entrada emitem UTF-8 sem tradução de quebras de linha; os
scripts `.sh` mantêm LF mesmo com Git configurado para CRLF no Windows.

Todos os comandos abaixo são executados na pasta `Minic`:

```bash
cd Minic
make
```

Sem `make`, os comandos simples também funcionam:

```bash
gcc -std=c11 -Wall -Wextra parser.c -o parser
gcc -std=c11 -Wall -Wextra minic.c -o minic
```

### Etapa 1 — scanner

```bash
python src/lexer/lexer.py tests/casos-programas-c/c01_fibonacci.c
gcc -std=c11 -D_POSIX_C_SOURCE=200809L src/lexer/lexer.c -o lexer
./lexer tests/casos-programas-c/c01_fibonacci.c
```

Os fontes e o formato JSONL originais dessa etapa foram mantidos. O adaptador
Python das etapas seguintes importa `tokenize` desse lexer. O adaptador C é
gerado das regras do mesmo `lexer.l`, substituindo apenas a emissão de JSONL
pela criação de tokens para o parser.

### Etapa 2 — parser

```bash
python parser.py tests/parser/casos/25_programa_integrado/codigo.c
./parser tests/parser/casos/25_programa_integrado/codigo.c
```

A saída é a AST textual. Erros léxicos ou sintáticos retornam status 1 sem
imprimir uma árvore parcial; falhas de entrada/uso retornam 2. A etapa 2 não
aplica regras semânticas: por exemplo, pode construir a árvore de `if (1)`,
mas a etapa 3 rejeita essa condição por não ser booleana.

### Etapa 3 — analisador semântico

```bash
python minic.py tests/semantico/04_vetores_e_parametro_array.c
./minic tests/semantico/04_vetores_e_parametro_array.c
./testes_semanticos_py.sh minic.py tests/semantico
./testes_semanticos_c.sh minic.c tests/semantico
```

Sucesso produz `Análise semântica concluída: 0 erros; programa aceito.` e status
0. Erros semânticos produzem os diagnósticos SEMxxx ordenados por posição, a
contagem final e status 1. A saída segue os bytes dos gabaritos originais:
CRLF entre diagnósticos e ausência de newline final.

## Verificar o projeto completo

```bash
make test
# alternativa sem make:
python tests/validar_compilador.py
```

O verificador compila os pontos de entrada usados pelo professor, compara C e
Python, executa os 50 testes sintáticos, os 20 semânticos, 29 regressões e os
10 casos disponíveis da etapa 1. As saídas novas ficam em `Minic/build`, sem
sobrescrever os resultados antigos versionados da etapa 1.

[Resultados e limitações dos gabaritos](Minic/RESULTADOS_TESTES.md): o caso
sintático 24 contém um parêntese excedente na referência; o fixture léxico i06
não tem fonte de entrada no material fornecido. Os arquivos originais dos
testes não foram corrigidos para ocultar essas limitações.

## Manter a continuidade entre as etapas

O contrato de tokens e nós está em `src/common.*`. O parser é único por
linguagem e é consumido pelos dois pontos de entrada. O semântico recebe a
AST; ele não relê o código com outro scanner nem reconstrói a sintaxe.

A gramática inclui variáveis e inicializadores, vetores unidimensionais,
parâmetros escalar/vetor, funções com corpo, blocos, `if/else`, `while`,
`return`, chamadas, atribuições e operadores. Reconhecer um token na etapa 1
não significa que uma construção esteja prevista na gramática das etapas
seguintes: ponteiros, structs, diretivas de pré-processador, `for`, `break` e
`continue` não fazem parte da gramática sintática fornecida. `int f();` é
rejeitado conforme o teste sintático 48; funções precisam de corpo.

O semântico verifica escopos com sombreamento, duplicações, chamadas e tipos,
promoção `int → float`, condições e operadores lógicos booleanos, índices
inteiros, parâmetros vetor, destinos atribuíveis, uso de void e cobertura de
retorno em funções não void. As condições não usam a regra de valores
escalares da linguagem C: MINIC exige `bool`.

Depois de alterar `src/lexer/lexer.l`, regenere o adaptador C:

```bash
make scanner
# ou: python tools/gerar_scanner.py
make test
```

Essa geração requer Flex 2.6.4 e GNU M4. O teste verifica a assinatura da
especificação para detectar um scanner C desatualizado. Alterações léxicas
precisam manter também a versão Python equivalente. Não edite manualmente o
arquivo `src/scanner/scanner.c` gerado.

O `Makefile` compila os módulos C separados. Os pontos de entrada também
suportam a compilação em um único comando exigida pelos scripts do professor.
O compilador implementa o frontend de análise; não gera executável nem código
de máquina do programa MINIC.
