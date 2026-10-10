# Resultados de validação das três etapas

| Verificação | Python | C |
|---|---:|---:|
| Scanner da etapa 1, entradas disponíveis | 10/10 | 10/10 |
| Parser: entradas válidas | 25/25 aceitas | 25/25 aceitas |
| Parser: entradas inválidas | 25/25 rejeitadas, sem AST | 25/25 rejeitadas, sem AST |
| AST das entradas válidas, ignorando apenas espaços | 24/25 iguais à referência | 24/25 iguais à referência |
| Semântico: gabaritos e scripts originais | 20/20 | 20/20 |
| Regressões adicionais | 62/62 | 62/62 |
| AST anotada: tipos, símbolos, conversões e contextos | Validada | Validada |
