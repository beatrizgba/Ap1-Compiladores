# Otimizações que implementamos

Em `-O0`, preservamos a tradução direta da AST. Em `-O1`, aplicamos propagação global, simplificações, CSE local e eliminação conservadora de código morto. A ordem pode repetir até oito vezes para aproveitar ramos removidos e cópias que ficaram mortas; as análises internas de fluxo calculam ponto fixo antes de transformar. O limite externo restringe trabalho e não autoriza transformações sem prova.

## CFG e análise global

Identificamos líderes: primeira instrução, rótulos e instruções depois de salto/retorno. Cada intervalo define um bloco básico. `--cfg` mostra seus sucessores, dominadores e laços naturais. Um salto `a → h` é aresta de retorno se h domina a; coletamos os predecessores da cauda até o cabeçalho para formar o laço natural.

As análises de constantes, inicialização definida e liveness operam nos pontos de instrução do mesmo CFG, equivalente a compor suas transferências por bloco. Não confundimos fatos de apenas um predecessor com fatos válidos na junção inteira.

### Propagação de constantes — forward

Representamos cada ponto como inalcançável ou um mapa de constantes conhecidas; a ausência de uma variável no mapa de um ponto alcançável significa valor desconhecido. Na junção, mantemos `x=c` somente se todos os predecessores alcançáveis concordarem.

```text
IN[p] = meet(OUT[q], para q predecessor alcançável de p)
OUT[p] = transferencia(instrucao[p], IN[p])
```

Uma definição mata o fato anterior do destino. `CALL`, `STORE` e `READ` invalidam conservadoramente os fatos; não propagamos valores de globais como constantes estáveis entre chamadas. Repetimos as transferências até não haver mudança. Os testes incluem diamantes com ramos diferentes, valores iguais nos dois ramos e variáveis modificadas no back edge.

Depois de substituir operandos conhecidos, dobramos operações inteiras/booleanas quando não falham. Não dobramos divisão por zero ou overflow. Condicionais constantes viram salto incondicional ou continuação; removemos blocos que ficaram inalcançáveis em uma nova análise.

### Liveness — backward

```text
OUT[p] = união(IN[s], para s sucessor de p)
IN[p] = USE[p] ∪ (OUT[p] − DEF[p])
```

Repetimos até ponto fixo. Uma definição pode ser morta e ainda precisar executar porque pode falhar ou causar efeito. Por isso, não removemos indiscriminadamente aritmética, chamadas, I/O, alocações ou acessos a vetores.

Também calculamos inicialização definida por interseção dos predecessores. `LOCAL` mata a inicialização anterior; parâmetros e definições realizadas inicializam o destino. Só eliminamos cópias/conversões/not mortos quando a leitura de seu operando é comprovadamente segura. Assim, `int x; int y=x;` continua falhando mesmo se y nunca for usado.

## Passes locais

| Transformação | Condição que usamos |
|---|---|
| Folding inteiro/booleano | Operandos constantes, operação definida, resultado no intervalo |
| `x+0`, `x-0`, `x*1`, `0+x`, `1*x` | Tipo int; mantemos avaliações que possam falhar |
| Propagação de constantes/cópias | Fato válido no ponto; redefinição e efeitos invalidam associações |
| CSE local | Mesmo operador/tipo e mesmos valores dos operandos no bloco |
| Código inalcançável | Nenhum caminho desde a entrada do procedimento |
| Definições mortas | Resultado não vivo e operação comprovadamente sem efeito/erro |

A CSE usa representantes de cópias para reconhecer a mesma expressão apesar dos snapshots dos operandos. Redefinir um operando, seu representante ou o resultado invalida a expressão disponível. Chamadas, leituras e stores limpam o cache. Não fazemos CSE de `LOAD`, pois um alias pode ter alterado o elemento.

Exemplo: O01 calcula `a=2+3; b=a*4; print(b)`. Propagamos 5 e 20, retiramos definições mortas e conservamos `print 20`. No caso adicional de CSE com dois valores lidos, reutilizamos `a*b` sem presumir que a ou b sejam constantes.

## Limites intencionais

- Não aplicamos `x*0 → 0`, reordenação de ponto flutuante, eliminação geral de leituras de vetor ou remoção de operações potencialmente falháveis.
- O08 elimina a multiplicação 6×7 no corpo por propagação/dobra de constantes. Não implementamos LICM geral nem movemos uma divisão para antes de um laço que talvez execute zero vezes.
- Identificamos dominância e laços naturais para evidência/análise. Não geramos SIMD, fusão, desenrolamento nem redução geral de força. O09 admite explicitamente não aplicar essa transformação.
- Não usamos uma medição isolada como prova de correção ou promessa de ganho. Comparamos saída, stderr e status em -O0/-O1, inclusive falhas dinâmicas; isso é evidência experimental, não prova universal.

Os 30 casos fornecidos, as regressões finais e a execução do assembly estão em `tests/validar_ir.py`. O benchmark reproduzível está em [benchmarks.md](benchmarks.md).
