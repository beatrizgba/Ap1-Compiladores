# Roteiro da apresentação do grupo

## 1. Objetivo e integração

Desenvolvemos um compilador MINIC em C e Python. Integramos scanner, parser, semântica, IR, otimização e geração/execução de código. Escolhemos a máquina de pilha MINIC-VM como alvo, conforme as alternativas da especificação.

Mostramos o fluxo do README e a organização dos módulos. Explicamos que o compilador C não chama Python; os testes usam Python como ferramenta de validação.

## 2. Parser ascendente e AST

Implementamos SLR(1) com 146 estados, 87 produções e tabelas ACTION/GOTO. As reduções sintetizam nós da AST. Associamos else ao if mais próximo e tentamos recuperação sintática nas opções finais.

```powershell
python minic.py --trace-parser --ast examples/fatorial.c
```

Apontamos um shift, uma redução de expressão e o accept. Explicamos a diferença entre estratégia ascendente e classificação de atributos.

## 3. Semântica, SDD e dependências

Resolvemos símbolos, tipos, retornos e contextos de laços antes de gerar código. A SDD especifica os atributos; nossas ações de redução/tradução executam essas regras. A AST guarda os resultados.

```powershell
python minic.py --symbols examples/fatorial.c
python minic.py --sdd examples/fatorial.c
python minic.py --deps examples/fatorial.c -o build/dependencias.dot
```

Mostramos `type`, `symbol`, `place`, `code`, uma dependência filho → pai e a ordem topológica. Explicamos que o grafo de atributos é diferente do CFG.

## 4. IR e backend

Geramos quádruplas tipadas com operações simples, temporários, rótulos, LOAD/STORE e chamadas. O backend transforma essas instruções em operações de pilha. Cada chamada tem seu próprio frame, o que permite recursão.

```powershell
python minic.py --ir -O0 examples/fatorial.c
python minic.py --target=asm examples/fatorial.c -o build/fatorial.mvm
python minic.py --exec build/fatorial.mvm
.\minic.exe --exec build/fatorial.mvm
```

Conferimos a saída 120 nas duas versões. Mostramos um trecho ADD → PUSH/PUSH/ADD/POP e explicamos a ordem dos argumentos e o retorno.

## 5. Otimização e fluxo de dados

Implementamos folding inteiro, identidades seguras, CSE local, propagação global de constantes e remoção de código morto com liveness. Calculamos ponto fixo e invalidamos fatos diante de efeitos. O CFG mostra dominadores e laços naturais.

```powershell
python minic.py --ir -O0 tests/ir/originais/testes/03-otimizacoes/O01/O01.c
python minic.py --ir -O1 tests/ir/originais/testes/03-otimizacoes/O01/O01.c
python minic.py --cfg examples/benchmark.c -o build/fluxo.dot
```

Explicamos por que não removemos uma divisão potencialmente falhável só porque seu resultado não é usado. Apresentamos as limitações: sem LICM geral, SIMD ou redução de força de indução.

## 6. Evidências e conclusão da demonstração

```powershell
python tests/validar_compilador.py
```

Mostramos os resultados das etapas anteriores, os 30 casos de IR e as 73 regressões finais. Explicamos o critério estrutural/semântico do README da suíte e a diferença para seus scripts literais. Mantivemos os gabaritos originais.

Apresentamos o benchmark com método, medianas e tamanhos estáticos da IR/VM. Mostramos que menos instruções não garante ganho no tempo total em todas as entradas. Encerramos demonstrando um programa com entrada:

```powershell
cmd /c "python minic.py --run -O1 examples/entrada_vetores.c < examples/entrada.txt"
```

Esperamos 30. Deixamos o manual no README e os detalhes de contrato/limitações na documentação de arquitetura.
