# Benchmark reproduzível

Medimos separadamente a compilação MINIC → assembly e a execução do assembly salvo, usando `examples/benchmark.c`. O programa lê n e soma 6×7 em n iterações. Conferimos saída `42*n`, stderr vazio e status 0 antes e durante cada medição.

Usamos entradas **20, 200 e 2000**, um aquecimento e cinco execuções medidas por configuração. Reportamos a mediana. O tempo de execução é o tempo do processo `--exec`: inclui carregar o assembly e iniciar o interpretador/VM; não é uma medição isolada de instruções da CPU. O tempo de compilação não inclui construir o compilador C com GCC.

## Ambiente desta medição

Executamos a medição no ambiente de desenvolvimento Linux x86-64, Python 3.12.14 e GCC 14.2.0. Isso não é uma medição do notebook Windows. O JSON mantém a identificação completa; o CSV preserva as 12 configurações medidas. O fonte é identificado por SHA-256.

- [Dados completos](resultados/benchmark.csv).
- [Ambiente e método](resultados/ambiente-benchmark.json).

| Implementação | n | -O0: mediana (ms) | -O1: mediana (ms) | Saída nos dois modos |
|---|---:|---:|---:|---:|
| Python | 20 | 23,912 | 23,785 | 840 |
| Python | 200 | 26,233 | 26,334 | 8400 |
| Python | 2000 | 39,551 | 37,632 | 84000 |
| C | 20 | 0,818 | 0,788 | 840 |
| C | 200 | 0,880 | 0,677 | 8400 |
| C | 2000 | 2,288 | 1,878 | 84000 |

Nos dois compiladores, a IR caiu de **37 para 31 instruções** e o assembly de **68 para 54 instruções**. Esses são tamanhos estáticos do programa gerado, não contagens dinâmicas das iterações. A multiplicação constante no corpo deixou de ser executada como multiplicação.

Os casos menores mostram que reduzir código não garante menor tempo total: inicialização do processo e variação experimental têm peso grande. Não concluímos uma aceleração universal nem comparamos a velocidade das linguagens como se os ambientes fossem idênticos em custo de inicialização.

## Reproduzir no VS Code

No PowerShell, dentro de `Minic`, com Python/GCC no PATH:

```powershell
python tools/benchmark.py --repeticoes 5
```

Os novos dados ficam em `build/benchmark`. O script não substitui estes arquivos registrados nem o Markdown automaticamente. Para mudar o diretório:

```powershell
python tools/benchmark.py --repeticoes 5 --saida build/meu-benchmark
```

Recomendamos fechar tarefas pesadas e repetir a medição. A equivalência de resultados é conferida antes de qualquer conclusão sobre tempo.
