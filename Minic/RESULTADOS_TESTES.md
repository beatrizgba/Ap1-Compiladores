# Resultados de validação das três etapas

Executamos os testes em 10/10/2026, no Linux, com Python 3.12.14, GCC 14.2.0
e Flex 2.6.4. Não executamos os testes diretamente no Windows. Também
verificamos a saída UTF-8 Python com stream CP1252 e tradução de LF para
CRLF simulados e confirmamos que os bytes permaneceram idênticos.
Configuramos a saída C em modo binário no Windows e definimos finais de
linha LF para os scripts Bash em `.gitattributes`.

| Verificação | Python | C |
|---|---:|---:|
| Scanner da etapa 1, entradas disponíveis | 10/10 | 10/10 |
| Parser: entradas válidas | 25/25 aceitas | 25/25 aceitas |
| Parser: entradas inválidas | 25/25 rejeitadas, sem AST | 25/25 rejeitadas, sem AST |
| AST das entradas válidas, ignorando apenas espaços | 24/25 iguais à referência | 24/25 iguais à referência |
| Semântico: gabaritos e scripts originais | 20/20 | 20/20 |
| Regressões adicionais | 29/29 | 29/29 |

Comparamos as saídas e os códigos de retorno das versões C e Python e
confirmamos que coincidiram nos 50 testes do parser, 20 testes semânticos
e 29 regressões. Os semânticos retornam 0 para aceitação
e 1 para rejeição; entradas inválidas do parser retornam 1 sem imprimir AST.

Identificamos um parêntese de fechamento excedente no caso sintático 24, no arquivo
`tests/parser/casos/24_la_o_com_express_o_complexa/ast.esperada.txt` original.
Nosso compilador gera a AST correspondente com parênteses balanceados.
Registramos essa única divergência no teste e verificamos que remover
exatamente um `)` da referência resulta na mesma árvore. Preservamos o
gabarito fornecido, sem alterar seu arquivo.

Encontramos os arquivos expected/errors do caso léxico i06, mas não o arquivo
`.minic` de entrada. Por isso, não executamos esse caso; os 10/10 correspondem
aos casos disponíveis. Com o verificador, examinamos 491 tokens esperados,
incluindo o gabarito sem entrada. Isso não equivale a executar o i06.

Executamos os 20 casos semânticos, os 50 sintáticos e as 26 regressões
armazenadas em JSON com AddressSanitizer e UndefinedBehaviorSanitizer: **96
execuções sem falhas de memória, vazamentos ou comportamento indefinido**.
Também executamos as três regressões geradas em tempo de execução
(256 argumentos, escopos profundos e CRLF), que passaram na execução normal.

Regeneramos o scanner C duas vezes e obtivemos resultados idênticos.
Preservamos os quatro arquivos da etapa 1 (`lexer.py`, `lexer.l`, `lexer.c`,
`lex.yy.c`). Compilamos os novos componentes com
`-std=c11 -Wall -Wextra -Wpedantic -Werror`, sem erros ou avisos, tanto nos
pontos de entrada usados pelo professor quanto nos módulos separados.

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
final. Mantivemos esse formato na saída do compilador para a comparação exata.
Não ajustamos nenhum texto, código, posição ou gabarito para fazer os testes
passarem. Definimos a regra `-text` em `.gitattributes` para conservar os bytes
desses arquivos durante commit e checkout.

Com esses testes, validamos os conjuntos indicados. Essa validação não comprova
a implementação de todas as construções da linguagem C, nem substitui as
demais exigências de avaliação do professor.
