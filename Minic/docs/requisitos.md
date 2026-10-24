# Conferência dos materiais da etapa final

Usamos a especificação MINIC 1.0, as apostilas e slides das Aulas 16, 17, 18 e 19 e os 30 casos de IR fornecidos pelo professor. Preservamos os casos e os scripts em `tests/ir/originais`.

| Material / observação | Evidência no projeto |
|---|---|
| Retorno da entrega anterior: parser ascendente | Driver SLR(1) em C e Python; 146 estados, 87 produções, tabela ACTION/GOTO; `--trace-parser` |
| Aula 16: atributos e SDD/SDT | Equações em `sdd.md`, ações nas reduções e na tradução da AST; `--sdd` |
| Aula 16: grafo e ordem de avaliação | `--deps` gera DOT da árvore concreta; `--sdd` mostra ordem topológica verificada |
| Aula 17: IR 3AC e temporários | Quádruplas tipadas; `--ir`; validação de rótulos, chamadas e definições de temporários |
| Aula 17: execução e chamadas | Backend MINIC-VM de pilha; frames independentes, recursão, parâmetros e retorno; `--target=asm`, `--exec`, `--run` |
| Especificação: construções mínimas | Tipos, globais, vetores, funções, escopos, if/else, while, for, break/continue, print/read |
| Especificação: erros por fase | Opções finais retornam 1/2/3/4/5/6; recuperação sintática com limite de 10 erros |
| Aula 18: blocos básicos e CFG | Partição em líderes, entradas/saídas e arestas; `--cfg` |
| Aula 18: transformação local | Dobra inteira, identidades seguras e CSE com invalidação de valores |
| Aula 18: análise global | Propagação forward de constantes até ponto fixo e liveness backward; remoção conservadora de definições mortas |
| Aula 19: dominância e laços naturais | Dominadores e arestas de retorno no CFG; laços naturais identificados no DOT |
| Aula 19: integração e corretude | Casos oficiais e regressões com execução -O0/-O1, comparando stdout, stderr e status |
| Etapa final: resultados e apresentação | `RESULTADOS_TESTES.md`, `tools/benchmark.py`, `benchmarks.md`, `apresentacao.md` |

## Contrato dos testes fornecidos

O README da suíte pede equivalência estrutural/semântica e admite nomes e serializações diferentes. Os scripts Bash, porém, chamam apenas `minic arquivo.c`, sem `--ir` nem `-O1`, e comparam literalmente com arquivos `.txt` que contêm explicações, IR de referência e observações.

Não emitimos a prosa dos gabaritos como saída do compilador, nem afirmamos 30/30 nesses scripts literais. Nosso `tests/validar_ir.py` usa os 30 fontes originais, verifica a fase dos erros, compara IR C/Python, verifica propriedades estruturais e executa os programas com resultados esperados independentes. Também salva/recarrega assembly e confronta -O0/-O1.

Os scripts `test_ir_python.sh` e `test_ir_c.sh` na raiz de Minic chamam esse verificador com a implementação escolhida; os scripts fornecidos continuam intactos em `tests/ir/originais`.

## Políticas explícitas

Mantivemos a chamada sem opções para os scripts semânticos anteriores. Ela não exige main e conserva os gabaritos e status 0/1 anteriores. Nas opções finais exigimos `int main()` sem parâmetros e habilitamos a rejeição de divisor constante zero e literal fora do intervalo. Usamos inteiros de 64 bits com erro de overflow, reais double finitos, índices em células, curto-circuito e avaliação da esquerda para a direita. As conversões implícitas adotadas continuam limitadas a int para float; char para int é uma recomendação da especificação que não adotamos implicitamente.

Não implementamos assembly nativo, LLVM, SIMD, fusão/desenrolamento de laços ou redução de força de indução. O09 é opcional na própria suíte e é validado por equivalência. O08 perde a multiplicação constante no corpo por propagação/dobra; não apresentamos isso como uma implementação geral de LICM. A escolha de uma VM atende às alternativas de alvo previstas na especificação.
