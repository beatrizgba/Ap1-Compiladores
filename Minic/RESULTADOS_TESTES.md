# Resultados de validação das três etapas

Execução em 10/10/2026, Linux, Python 3.12.14, GCC 14.2.0 e Flex 2.6.4.
Não houve execução nativa em Windows. A saída UTF-8 Python foi verificada
também com stream CP1252 e tradução de LF para CRLF simulados; os bytes
permaneceram idênticos. O C configura saída binária no Windows, e os scripts
Bash têm checkout LF explícito em `.gitattributes`.

| Verificação | Python | C |
|---|---:|---:|
| Scanner da etapa 1, entradas disponíveis | 10/10 | 10/10 |
| Parser: entradas válidas | 25/25 aceitas | 25/25 aceitas |
| Parser: entradas inválidas | 25/25 rejeitadas, sem AST | 25/25 rejeitadas, sem AST |
| AST das entradas válidas, ignorando apenas espaços | 24/25 iguais à referência | 24/25 iguais à referência |
| Semântico: gabaritos e scripts originais | 20/20 | 20/20 |
| Regressões adicionais | 29/29 | 29/29 |

As saídas e códigos de retorno C/Python coincidiram nos 50 testes do parser,
20 testes semânticos e 29 regressões. Os semânticos retornam 0 para aceitação
e 1 para rejeição; entradas inválidas do parser retornam 1 sem imprimir AST.

O caso sintático 24 tem um parêntese de fechamento excedente no arquivo
`tests/parser/casos/24_la_o_com_express_o_complexa/ast.esperada.txt` original.
O compilador gera a AST correspondente com parênteses balanceados. O teste
registra essa única divergência e verifica que remover exatamente um `)` da
referência resulta na mesma árvore. O gabarito fornecido permanece intacto.

O fixture léxico i06 contém expected/errors, mas não o arquivo `.minic` de
entrada. Ele não foi executado; os 10/10 correspondem aos casos disponíveis.
O verificador de fixtures examinou 491 tokens esperados, incluindo o gabarito
sem entrada. Isso não equivale a executar o i06.

Foram executados os 20 casos semânticos, os 50 sintáticos e as 26 regressões
armazenadas em JSON com AddressSanitizer e UndefinedBehaviorSanitizer: **96
execuções sem falhas de memória, vazamentos ou comportamento indefinido**.
As três regressões geradas em tempo de execução (256 argumentos, escopos
profundos e CRLF) também passaram na execução normal.

O scanner C foi regenerado duas vezes com resultado idêntico. Os quatro
fontes da etapa 1 (`lexer.py`, `lexer.l`, `lexer.c`, `lex.yy.c`) não foram
alterados. A compilação normal dos novos componentes passou com
`-std=c11 -Wall -Wextra -Wpedantic -Werror`, tanto nos pontos de entrada
usados pelo professor quanto na compilação dos módulos separados.

## Reproduzir

Na pasta `Minic`:

```bash
make test
./testes_semanticos_py.sh minic.py tests/semantico
./testes_semanticos_c.sh minic.c tests/semantico
```

Sem `make`, execute `python3 tests/validar_compilador.py`. Ele compila os
binários C necessários em `build`, executa os scripts originais e valida as
três etapas, a AST e as regressões. Requer Python 3, GCC e Bash.

Os gabaritos semânticos enviados têm CRLF entre diagnósticos e não têm newline
final. A saída do compilador conserva esse formato para a comparação exata;
nenhum texto, código, posição ou gabarito foi ajustado para fazer os testes
passarem. A regra `-text` em `.gitattributes` conserva os bytes desses arquivos
durante commit e checkout.

Estes resultados validam os conjuntos indicados. Não demonstram implementação
de todas as construções da linguagem C, nem substituem as demais exigências
de avaliação do professor.
