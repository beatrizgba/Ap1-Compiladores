# Regras do nosso MINIC

Implementamos o frontend em três etapas: scanner, parser e análise semântica.
Usamos as regras da Aula 15 e os casos oficiais do projeto para definir o
comportamento em C e Python. Nesta etapa, ainda não geramos código de máquina
nem representação intermediária.

## Sintaxe e escopos

Reconhecemos tipos `int`, `float`, `bool`, `char` e `void`; declarações e
inicializadores; vetores unidimensionais; funções com corpo; parâmetros escalares
ou vetores; blocos; chamadas; expressões; atribuições; `if/else`, `while`,
`return`, `break` e `continue`.

Coletamos as declarações globais antes de analisar os corpos. Assim, permitimos
recursão e chamadas a funções definidas depois. Em funções, parâmetros e
variáveis do bloco principal compartilham o mesmo escopo. Permitimos
sombreamento em blocos internos e restauramos o símbolo externo ao sair.
Nomes repetidos no mesmo escopo geram diagnóstico com a posição da declaração
anterior. Usamos identificadores ASCII, conforme as regras Flex da etapa 1.

Mantivemos a exigência de corpo de função do caso sintático 48: `int f();`
é rejeitado. Não acrescentamos ponteiros, structs, casts, `for` nem vetores
multidimensionais à gramática fornecida. Reconhecer uma palavra no scanner
não implica que ela seja um comando da gramática das etapas seguintes.

## Tipos, conversões e atribuições

Centralizamos a compatibilidade de tipos. Permitimos somente a promoção
numérica implícita `int → float`; não convertemos `float → int`, números para
`bool` ou `char` para números. Condições e operadores lógicos exigem `bool`.
Operadores aritméticos exigem números; `%` exige dois inteiros; relacionais
produzem `bool`; igualdade compara números compatíveis ou valores do mesmo
tipo `bool`/`char`. Chamadas verificam categoria, quantidade e tipos dos
argumentos. Funções `void` só podem ser chamadas como comandos.

Atribuições são comandos: não aceitamos `return (x = 2)`, `f(x = 2)` ou
`if (b = true)`. Mantivemos a extensão **exigida pelo caso semântico oficial
09**: `a = b = 3` é um comando válido. Nesse contexto específico, a atribuição
interna fornece o valor para o próximo destino; isso não permite usar a cadeia
como valor de outra expressão. A gramática e os testes oficiais prevalecem
quando adotam uma extensão ao núcleo didático da apostila.

## Vetores

Guardamos o tipo dos elementos e o tamanho conhecido do vetor na AST e no
símbolo resolvido. Exigimos tamanho inteiro constante positivo e índice `int`.
O acesso a um elemento pode ser destino de atribuição; o vetor completo não.

Calculamos expressões constantes inteiras com `+`, `-`, `*`, `/`, `%` e sinais
unários, respeitando o intervalo inteiro de 64 bits. Não usamos resultados que
estourem esse intervalo ou dividam por zero para provar tamanhos ou índices.
A divisão constante inteira trunca em direção a zero nas duas versões.

Rejeitamos índices constantes negativos e índices maiores ou iguais ao tamanho
conhecido. Índices variáveis dependem da execução. Parâmetros `int a[]` não
fixam tamanho: verificamos o tipo do elemento e índices negativos constantes,
mas não supomos um limite superior. A compatibilidade de parâmetros vetor
exige o mesmo tipo de elemento; dimensões conhecidas continuam registradas.

## Controle de fluxo e retornos

Verificamos a profundidade dos laços: `break` e `continue` fora de um laço geram
`SEM010`. Cada `return` é comparado com o tipo da função, e funções não `void`
precisam retornar em todos os caminhos, segundo uma análise conservadora.

Guardamos as possibilidades de fluxo de cada comando: continuar normalmente,
retornar, interromper ou continuar um laço. Um `while` pode executar zero vezes
e não garante retorno sozinho. Um `return` escrito depois de um `break` ou
`continue` no mesmo caminho não prova retorno daquele caminho. Ainda analisamos
código inalcançável para verificar nomes, tipos e contexto.

## AST anotada e diagnósticos

Preservamos a estrutura e a saída sintática da etapa 2. Ao analisar a mesma
árvore, preenchemos os atributos `semantic_type`, `symbol`, `category`,
`conversion_to`, `expected_type`, `array_size`, `assignable`, `loop_depth` e
`flow`. A próxima etapa recebe os tipos, as referências às declarações, as
promoções necessárias e os contextos, sem precisar reconstruir o programa.
Literais mantêm seu lexema e seu tipo primitivo; tamanhos constantes ficam
convertidos no atributo `array_size`.

Usamos `error` como tipo sentinela para suprimir diagnósticos de tipo derivados
de nomes que já falharam na resolução. Acumulamos até 100 diagnósticos e os
ordenamos por linha e coluna. Mantivemos as mensagens e os bytes exigidos pelos
20 gabaritos semânticos originais. Para limites de vetores usamos `SEM006`,
para contexto de laço `SEM010` e para atribuição usada como valor `SEM004`.

## Testes e referências

Validamos os casos oficiais, regressões adicionais, o programa integrado da
Aula 15 e as anotações internas em C e Python. O verificador também confirma
que a análise não altera a árvore sintática. As anotações ficam disponíveis
pela API dos módulos; a saída dos comandos oficiais permanece compatível com
os gabaritos.

As particularidades dos scripts e dos gabaritos fornecidos estão documentadas
em [Scripts originais do professor](tests/scripts_professor/README.md).
