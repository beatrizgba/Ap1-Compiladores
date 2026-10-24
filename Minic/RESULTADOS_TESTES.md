# Resultados de validação do projeto completo

| Verificação | Python | C |
|---|---:|---:|
| Scanner da etapa 1, entradas disponíveis | 10/10 | 10/10 |
| Parser: entradas válidas | 25/25 aceitas | 25/25 aceitas |
| Parser: entradas inválidas | 25/25 rejeitadas, sem AST | 25/25 rejeitadas, sem AST |
| AST das entradas válidas, ignorando apenas espaços | 24/25 iguais à referência | 24/25 iguais à referência |
| Semântico: gabaritos e scripts originais | 20/20 | 20/20 |
| Regressões adicionais do frontend | 62/62 | 62/62 |
| AST anotada: tipos, símbolos, conversões e contextos | Validada | Validada |
| IR: programas válidos e otimização | 20/20 | 20/20 |
| IR: rejeições na fase prevista | 10/10 | 10/10 |
| Regressões da etapa final, -O0/-O1 | 70/70 | 70/70 |
| Assembly salvo, recarregado e executado | 20 × 2 modos | 20 × 2 modos |
| Parser SLR, SDD, dependências e CFG | Validados | Validados |
