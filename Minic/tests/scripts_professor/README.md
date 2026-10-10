# Scripts originais do professor

Preservamos nesta pasta os scripts de scanner e parser enviados pelo professor,
sem alterar seu conteúdo. Os dois scripts semânticos originais ficam na raiz
de `Minic`, com os nomes exigidos para a entrega.

## Parser

Os scripts originais comparam a saída textual, incluindo espaços internos.
Os gabaritos válidos usam espaçamentos diferentes entre si. Por isso, nosso
verificador compara as árvores ignorando espaços, sem mudar os gabaritos.

Nos casos inválidos, `ast.esperada.txt` contém a orientação “NÃO HÁ AST: o
parser deve rejeitar a entrada.”. Os scripts originais comparam essa frase com
o diagnóstico produzido. O README dos testes, porém, pede rejeição, status
diferente de zero e diagnóstico sintático, permitindo mensagens diferentes.
Nos scripts do projeto seguimos esses critérios, usando `resultado.esperado.txt`
para distinguir aceitação de rejeição.

O caso 24 tem um parêntese de fechamento excedente no gabarito. Verificamos que
a AST produzida é balanceada e que remover exatamente esse parêntese da
referência resulta na mesma árvore. Registramos a ressalva no resultado;
preservamos o arquivo original. Esse caso não tem igualdade textual exata.

Na pasta `Minic`, executamos as versões adaptadas do projeto com:

```bash
bash testar_parser_python.sh tests/parser parser.py
bash testar_parser_c.sh tests/parser parser.c
```

Essas versões verificam os 50 casos com a ressalva explícita do caso 24.
Se executarmos os scripts originais sem adaptação, suas comparações literais
não representam esse mesmo critério de aprovação. Não tratamos a aprovação
no verificador do projeto como aprovação literal nos scripts originais.

## Scanner

O caso i06 tem gabarito, mas seu arquivo de entrada não foi fornecido.
Executamos os 10 casos disponíveis; não inventamos uma entrada para declarar
aprovação do i06. Os scripts originais contam a ausência como aviso e podem
retornar status 1 mesmo com os 10 casos disponíveis aprovados.

Os scripts inicializam `PASSED/FAILED/SKIPPED`, mas usam `PASS/FAIL/WARN`;
o script C também chama `fail` em uma falha de compilação sem definir essa
função. Mantivemos os originais para permitir conferir essas diferenças.

As entradas `scanner.py` e `scanner.c` na raiz de `Minic` usam os scanners
originais da etapa 1. Configuramos UTF-8 no Python, saída binária C no Windows
e a declaração POSIX necessária para compilação C11 no Linux, sem editar os
quatro fontes originais da etapa 1.

Para conferir os scripts originais com essas entradas:

```bash
bash tests/scripts_professor/test_scanner_python.sh scanner.py tests
bash tests/scripts_professor/test_scanner_c.sh scanner.c tests
```

O teste completo compara tokens e diagnósticos, salva as saídas em `build`
e mantém os resultados históricos da etapa 1.
