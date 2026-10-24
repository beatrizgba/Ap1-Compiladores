# Compilador MINIC — projeto completo

Projeto da disciplina de Compiladores, com versões em Python e C. Integramos as quatro
etapas: o scanner fornece tokens, o parser ascendente constrói a AST, a análise
semântica verifica e anota essa árvore, e a etapa final gera IR, aplica
otimizações e produz código executável na nossa máquina virtual MINIC-VM.

Integrantes: Ana Clara T. Coser, Beatriz Gabrielly Batista de Almeida e
Vinicius Henrique Santos.

```mermaid
flowchart LR
    Fonte[Código MINIC] --> Scanner[Etapa 1: scanner]
    Scanner --> Tokens[Tokens com lexema e posição]
    Tokens --> Parser[Etapa 2: parser]
    Parser --> AST[Árvore sintática]
    AST --> Semantico[Etapa 3: analisador semântico]
    Semantico --> IR[IR de três endereços]
    IR --> Otimizador[Etapa 4: otimização -O0 ou -O1]
    Otimizador --> Backend[Assembly MINIC-VM]
    Backend --> Execucao[Execução na máquina de pilha]
```

## Organização

```text
Minic/
├── scanner.py / scanner.c     # entrada da etapa 1
├── parser.py / parser.c       # entrada da etapa 2
├── minic.py / minic.c         # entrada das etapas 3 e 4
├── Makefile
├── src/
│   ├── lexer/                # fontes originais da etapa 1, preservados
│   ├── scanner/              # adaptadores das mesmas regras da etapa 1
│   ├── parser/               # parser ascendente SLR, compartilhado
│   ├── semantico/            # análise da AST, em C e Python
│   ├── ir/                   # quádruplas, atributos e dependências
│   ├── optimizer/            # CFG, constantes, CSE e liveness
│   ├── codegen/              # backend e execução MINIC-VM
│   └── common.py / common.h / common.c
├── docs/                     # arquitetura, SDD, otimizações e apresentação
├── examples/                 # programas completos e entrada de exemplo
├── tools/                    # geradores de tabelas/scanner e benchmark
├── testar_parser_python.sh   # testes sintáticos do projeto
├── testar_parser_c.sh
├── testes_semanticos_py.sh    # script original do professor
├── testes_semanticos_c.sh     # script original do professor
├── test_ir_python.sh         # verificadores de IR do projeto
├── test_ir_c.sh
└── tests/
    ├── casos-invalidos/      # casos léxicos
    ├── casos-programas-c/    # casos léxicos
    ├── parser/              # 50 casos sintáticos fornecidos
    ├── semantico/           # 20 casos e gabaritos fornecidos
    ├── regressao/           # casos adicionais
    ├── scripts_professor/   # scripts originais de scanner e parser
    ├── ir/                  # 30 fontes/gabaritos originais e regressões finais
    ├── validar_ir.py         # IR, backend e otimização
    └── validar_compilador.py # valida o projeto completo e C × Python
```

## Preparar e executar no VS Code

### Passo 1 — instalar as ferramentas

Usamos **Python 3.10 ou superior** e **GCC**. A versão Python não precisa de
bibliotecas extras.

No Windows, instale o [Python](https://www.python.org/downloads/windows/) e o
[MSYS2](https://www.msys2.org/). Mantenha a pasta padrão do MSYS2: `C:\msys64`.
Depois, abra **MSYS2 UCRT64** pelo menu Iniciar e execute:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc diffutils
```

Esse comando instala o compilador C e a ferramenta que compara as saídas com
os gabaritos. O Bash e as demais ferramentas básicas vêm com o MSYS2.
Se já estiver tudo instalado, siga para o próximo passo.

### Passo 2 — abrir o projeto e o terminal

Abra a pasta `Ap1-Compiladores` no VS Code. Vá em **Terminal → Novo Terminal**
e escolha **PowerShell**. Entre na pasta do compilador:

```powershell
cd Minic
```

Execute os próximos comandos dentro de `Minic`. Se o terminal já estiver nessa
pasta, não precisa repetir `cd Minic`.

### Passo 3 — configurar os caminhos no terminal

No PowerShell, disponibilize o Python e as ferramentas do MSYS2:

```powershell
$env:Path = "$env:LOCALAPPDATA\Python\bin;C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:Path
```

O caminho do Python acima corresponde à instalação pelo gerenciador Python
no Windows. Se sua instalação estiver em outra pasta, use o diretório que
contém o `python.exe`. Para localizar a instalação, execute
`py -c "import sys; print(sys.executable)"`.

Esse ajuste vale apenas para o terminal atual. Repita-o ao abrir outro terminal.

### Passo 4 — conferir as ferramentas

```powershell
python --version
python3 --version
gcc --version
bash --version
bash -c "diff --version"
```

Todos devem mostrar suas versões. Os scripts do professor usam o comando
`python3`, por isso conferimos também esse nome. Se aparecer a mensagem da
Microsoft Store, confira se a instalação real do Python vem antes de
`Microsoft\WindowsApps` no PATH.

### Passo 5 — compilar, se for executar C manualmente

```powershell
gcc -std=c11 -Wall -Wextra scanner.c -o scanner
gcc -std=c11 -Wall -Wextra parser.c -o parser
gcc -std=c11 -Wall -Wextra minic.c -o minic
```

Os scripts de testes e o verificador completo compilam os programas C
automaticamente. Esse passo é necessário para os exemplos individuais em C.

No Linux, usamos as mesmas ferramentas e executamos os binários com `./scanner`,
`./parser` e `./minic`. O ajuste de PATH em PowerShell é específico do Windows.

## Etapa 1 — analisador léxico (scanner)

**O que fizemos:** criamos o scanner em Python e C. Ele lê o código e identifica
os tokens, como palavras reservadas, nomes de variáveis, números e operadores.
Também informa a linha e a coluna de cada token e aponta erros léxicos.

Por exemplo, em `int idade = 20;`, ele identifica `int`, `idade`, `=`, `20` e `;`.
Preservamos os arquivos originais dessa etapa e criamos as entradas `scanner.py`
e `scanner.c` para executá-los com saída consistente no terminal.

### Passo 1 — escolher um arquivo para analisar

Neste exemplo, usamos `tests/casos-programas-c/c01_fibonacci.c`.
Você pode substituir esse caminho pelo seu arquivo MINIC.

### Passo 2 — executar na versão escolhida

**Python:**

```powershell
python scanner.py tests/casos-programas-c/c01_fibonacci.c
```

**C:** depois da compilação indicada na preparação:

```powershell
.\scanner.exe tests/casos-programas-c/c01_fibonacci.c
```

### Passo 3 — conferir o resultado

O terminal mostra os tokens encontrados, um por linha, no formato JSON.
Se houver um erro léxico, ele também será informado.

## Etapa 2 — analisador sintático (parser)

**O que fizemos:** construímos o parser para usar os tokens da etapa 1 e verificar
se o código segue a gramática do MINIC. Quando a sintaxe está correta, ele monta
uma árvore sintática (AST), que representa a estrutura do programa. Usamos um
parser ascendente SLR(1), com pilha de estados, tabelas ACTION/GOTO e ações
semânticas nas reduções.

**Corrigimos a versão entregue anteriormente:** ajustamos a leitura dos tokens,
o reconhecimento de declarações, vetores, funções e expressões, a construção
da árvore e o tratamento de erros. Integramos as versões Python e C à etapa 1
e incluímos `break` e `continue`, conforme o conteúdo da análise semântica.

### Passo 1 — escolher um arquivo para analisar

Neste exemplo, usamos `tests/parser/casos/25_programa_integrado/codigo.c`.

### Passo 2 — executar na versão escolhida

**Python:**

```powershell
python parser.py tests/parser/casos/25_programa_integrado/codigo.c
```

**C:** depois da compilação indicada na preparação:

```powershell
.\parser.exe tests/parser/casos/25_programa_integrado/codigo.c
```

A chamada exigida pelo professor em Bash/Linux é `./parser codigo.c`.

### Passo 3 — conferir o resultado

Se a sintaxe estiver correta, o terminal mostra a árvore do programa.
Se houver um erro léxico ou sintático, mostra o erro e não imprime uma árvore
incompleta.

### Passo 4 — executar os testes sintáticos

```powershell
bash testar_parser_python.sh tests/parser parser.py
bash testar_parser_c.sh tests/parser parser.c
```

Esperamos **50/50**, com a ressalva indicada para o parêntese excedente no
**gabarito do caso 24**. Usamos scripts do projeto que conferem aceitação,
rejeição e estrutura da AST. Preservamos os scripts originais do professor
em `tests/scripts_professor` para consulta.

## Etapa 3 — analisador semântico

**O que fizemos:** desenvolvemos o analisador semântico em Python e C.
Ele usa a árvore da etapa 2 para verificar se o programa faz sentido segundo
as regras do MINIC. Implementamos as seguintes verificações:

- Uso de variáveis declaradas e nomes repetidos no mesmo escopo.
- Compatibilidade dos tipos e promoção permitida de `int` para `float`.
- Uso de vetores, índices inteiros e limites de índices constantes.
- Condições booleanas em `if` e `while`.
- Uso de `break` e `continue` dentro de laços.
- Quantidade e tipos dos argumentos das funções e os retornos necessários.

Também guardamos na AST os tipos, os símbolos resolvidos, as conversões e os
contextos necessários para a próxima etapa do compilador.

### Passo 1 — escolher um arquivo para analisar

Neste exemplo, usamos `tests/semantico/04_vetores_e_parametro_array.c`.

### Passo 2 — executar na versão escolhida

**Python:**

```powershell
python minic.py tests/semantico/04_vetores_e_parametro_array.c
```

**C:** depois da compilação indicada na preparação:

```powershell
.\minic.exe tests/semantico/04_vetores_e_parametro_array.c
```

### Passo 3 — conferir o resultado

Quando o programa é aceito, aparece:

```text
Análise semântica concluída: 0 erros; programa aceito.
```

Quando há erros semânticos, o terminal mostra os códigos dos erros, suas
posições no arquivo e a quantidade de erros encontrados.

### Passo 4 — executar os scripts do professor

No mesmo PowerShell, execute um comando por vez:

```powershell
bash testes_semanticos_py.sh minic.py tests/semantico
```

```powershell
bash testes_semanticos_c.sh minic.c tests/semantico
```

Esperamos, ao final de cada script:

```text
Resultado: 20/20 aprovados; 0 reprovados.
```

Alguns arquivos contêm erros de propósito. O teste passa quando o analisador
aponta exatamente os erros previstos no gabarito.

Em Bash, também podemos usar diretamente os comandos exigidos pelo professor:

```bash
./testes_semanticos_py.sh minic.py tests/semantico
./testes_semanticos_c.sh minic.c tests/semantico
```

## Etapa 4 — IR, otimização e execução

**O que fizemos:** traduzimos a AST anotada para quádruplas de três endereços,
implementamos otimizações locais e globais e geramos assembly para a **MINIC-VM**,
uma máquina de pilha didática. Implementamos funções e recursão com frames
independentes, vetores com limites verificados, `for`, `print` e `read`.

Também implementamos evidências da SDD: atributos, equações documentadas,
grafo de dependências e ordem topológica. O CFG mostra blocos básicos,
dominadores e laços naturais.

### Passo 1 — conferir a análise do programa completo

Dentro de `Minic`, no PowerShell do VS Code:

```powershell
python minic.py --check examples/fatorial.c
```

Esperamos a mensagem de programa aceito, sem erros.

A chamada sem opções (`python minic.py arquivo.c`) continua compatível com os
testes semânticos da etapa 3. Para usar a etapa final, escolha as opções abaixo;
nessas opções, exigimos `int main()` sem parâmetros.

### Passo 2 — visualizar a IR antes e depois das otimizações

```powershell
python minic.py --ir -O0 examples/fatorial.c
python minic.py --ir -O1 examples/fatorial.c
```

Cada linha mostra `OPERAÇÃO`, `DESTINO`, `OPERANDO 1`, `OPERANDO 2` e `TIPO`,
separados por tabulações. `-O0` desativa o otimizador e `-O1` ativa os passes
implementados. Temporários e rótulos podem diferir dos exemplos do professor;
conferimos a estrutura e o comportamento.

### Passo 3 — executar o programa

**Python:**

```powershell
python minic.py --run -O0 examples/fatorial.c
python minic.py --run -O1 examples/fatorial.c
```

**C:** após compilar `minic.c`, conforme a preparação:

```powershell
.\minic.exe --run -O0 examples/fatorial.c
.\minic.exe --run -O1 examples/fatorial.c
```

As quatro execuções devem mostrar **`120`**.

Para testar entrada de dados e vetores:

```powershell
Get-Content examples/entrada.txt | python minic.py --run examples/entrada_vetores.c
Get-Content examples/entrada.txt | .\minic.exe --run examples/entrada_vetores.c
```

Esperamos **`30`**, a soma dos cinco valores do arquivo.

### Passo 4 — salvar e executar o código gerado

```powershell
New-Item -ItemType Directory -Force build | Out-Null
python minic.py --target=asm -O1 examples/fatorial.c -o build/fatorial.mvm
python minic.py --exec build/fatorial.mvm
.\minic.exe --exec build/fatorial.mvm
```

O arquivo `.mvm` contém assembly da nossa máquina virtual. Podemos gerá-lo em
uma versão do compilador e executá-lo na outra. O alvo escolhido funciona no
Windows com as ferramentas da preparação; não exige Ubuntu.

### Passo 5 — visualizar as evidências das aulas

```powershell
python minic.py --trace-parser --check examples/fatorial.c
python minic.py --symbols examples/fatorial.c
python minic.py --sdd examples/fatorial.c
python minic.py --deps examples/fatorial.c -o build/dependencias.dot
python minic.py --cfg examples/entrada_vetores.c -o build/fluxo.dot
```

`--trace-parser` mostra shifts e reduções. `--sdd` mostra os atributos e uma
ordem de avaliação válida. Os arquivos DOT guardam os grafos e podem ser
visualizados com uma ferramenta compatível com Graphviz; ela é opcional.

### Passo 6 — executar os testes da etapa final

```powershell
python tests/validar_ir.py
```

Esse comando verifica as versões C e Python. Para escolher uma versão:

```powershell
bash test_ir_python.sh minic.py tests/ir/originais
bash test_ir_c.sh minic.c tests/ir/originais
```

Usamos verificadores do projeto porque os scripts de IR fornecidos fazem
comparações literais com arquivos que incluem texto explicativo. Preservamos
os originais e explicamos o critério em [Conferência dos materiais](Minic/docs/requisitos.md).
Não confundimos aprovação semântica dos 30 casos com aprovação nesses scripts
literais.

### Passo 7 — reproduzir o benchmark

```powershell
python tools/benchmark.py --repeticoes 5
```

Conferimos as saídas antes de medir. Os resultados locais ficam em
`build/benchmark/resultados.csv` e `ambiente.json`. A medição separa compilação
e execução do assembly; reportamos a mediana de cinco execuções, após um
aquecimento. O método e os resultados registrados estão em
[Benchmarks](Minic/docs/benchmarks.md).

## Testar o projeto completo no VS Code

### Passo 1 — executar o verificador

Com as ferramentas disponíveis e o terminal dentro de `Minic`, execute:

```powershell
python tests/validar_compilador.py
```

Ele compila os programas C e verifica as quatro etapas, os scripts de testes,
as regressões e as anotações da AST, comparando as versões Python e C.

### Passo 2 — conferir os resultados

Esperamos:

- Etapa 1: **10/10** em Python e C, considerando os arquivos disponíveis.
- Etapa 2: **25/25 válidos aceitos** e **25/25 inválidos rejeitados**.
- Etapa 3: **20/20** em Python e C.
- Testes adicionais: **62/62**, com resultados equivalentes entre as versões.
- AST anotada: tipos, símbolos, conversões e contextos conferidos em C e Python.
- IR: **30/30** casos fornecidos, conferidos por categoria, estrutura e comportamento.
- Etapa final: **70/70** regressões, executadas com e sem otimização.
- Backend: **20 programas válidos × 2 modos**, com assembly salvo e recarregado.
- Parser SLR, SDD, dependências acíclicas, símbolos, CFG e temporários conferidos.

A observação do **caso 24** é uma diferença no gabarito original. O aviso do
**i06** informa que seu arquivo de entrada não foi fornecido. Mantivemos essas
ressalvas visíveis e preservamos os arquivos originais.

### Passo 3 — confirmar o encerramento

Logo depois do verificador, execute no PowerShell:

```powershell
$LASTEXITCODE
```

Esperamos **`0`**, sem `Traceback` ou `[FALHA]` na execução.
Se aparecer `WinError 4551`, o Windows bloqueou um executável por uma política
de segurança; a parte bloqueada ainda não foi testada. Consulte o controle de
aplicativos do Windows para identificar a restrição.

Os testes salvam arquivos locais em `Minic/build`. Não atualizam
`RESULTADOS_TESTES.md` e não enviam mudanças ao GitHub automaticamente.

Configuramos o GitHub Actions para executar a suíte em Linux e Windows/MSYS2
a cada push. A aba **Actions** do repositório mostra o resultado dessas execuções.

Registramos os resultados em [Resultados dos testes](Minic/RESULTADOS_TESTES.md).
As decisões da linguagem estão em [Regras do MINIC](Minic/REGRAS_MINIC.md).

## Documentação do projeto

- [Conferência dos requisitos e materiais](Minic/docs/requisitos.md).
- [Arquitetura, contratos e máquina virtual](Minic/docs/arquitetura.md).
- [SDD, atributos e dependências](Minic/docs/sdd.md).
- [Otimizações e argumentos de corretude](Minic/docs/otimizacoes.md).
- [Benchmarks reproduzíveis](Minic/docs/benchmarks.md).
- [Roteiro da apresentação](Minic/docs/apresentacao.md).

Para listar as opções e os códigos de saída, consulte `python minic.py --help`
e o documento de arquitetura.
