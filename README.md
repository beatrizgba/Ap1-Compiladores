# AP 1 - Analisador Léxico (Linguagem MINIC)

Este repositório contém a implementação da **Etapa 1** do projeto de construção de um compilador para a linguagem **MINIC**, desenvolvida para a disciplina de Compiladores.

O objetivo desta etapa é realizar a análise léxica, identificando os tokens válidos da linguagem (como palavras reservadas, identificadores, literais, operadores e delimitadores) e reportando erros para símbolos ou formações inválidas.

---

## 👥 Integrantes do Grupo

- **Ana Clara T. Coser** — RA: 2401456
- **Beatriz Gabrielly Batista de Almeida** — RA: 2400276
- **Vinicius Henrique Santos** — RA: 2400306

---

## 📁 Estrutura do Repositório

```text
.
├── src/
│   └── lexer/
│       ├── lexer.py             # Analisador Léxico em Python
│       └── lexer.l              # Especificação Flex/C do Analisador Léxico
├── tests/
│   ├── casos-invalidos/       # Fixtures de casos com erros léxicos (.minic e .jsonl)
│   ├── casos-programas-c/     # Fixtures de programas válidos em C/MINIC (.c e .jsonl)
│   ├── resultados/            # Arquivos de saída gerados pela suíte de teste (.jsonl)
│   │   └── python/            # Saídas em JSONL geradas nos testes de Python
│   ├── check_fixtures.py      # Script oficial de verificação das fixtures
│   └── run_tests.py           # Script de execução e relatório da bateria de testes
├── .gitignore
└── README.md