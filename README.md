# AP1 — Analisador Léxico da Linguagem MINIC

Projeto desenvolvido para a disciplina de **Compiladores**, correspondente à **Etapa 1 do Projeto Final** e à avaliação da **AP1**.

O projeto consiste no desenvolvimento de um analisador léxico para a linguagem **MINIC**, responsável por identificar os elementos léxicos presentes no código-fonte, gerar os respectivos tokens e detectar possíveis erros léxicos.

---

## Integrantes

| Nome | RA |
|---|---|
| Beatriz Gabrielly Batista de Almeida | 2400276 |
| Ana Clara T. Coser | 2401456 |
| Vinicius Henrique Santos | 2400306 |

---

## 1. Objetivo

O analisador léxico corresponde à primeira etapa do processo de compilação.

Sua função é receber um código-fonte escrito na linguagem MINIC e transformá-lo em uma sequência de tokens, identificando os elementos léxicos presentes no programa.

O analisador desenvolvido deve:

- reconhecer palavras reservadas;
- reconhecer identificadores;
- reconhecer literais inteiros;
- reconhecer literais reais;
- reconhecer literais de caracteres;
- reconhecer literais de cadeia;
- reconhecer operadores;
- reconhecer delimitadores;
- ignorar espaços em branco;
- ignorar comentários de linha;
- ignorar comentários de bloco;
- informar a linha e a coluna de cada token;
- informar atributos dos tokens quando aplicável;
- reconhecer o fim do arquivo;
- identificar e reportar erros léxicos.

Esta etapa corresponde à **Etapa 1 do Projeto Final da disciplina de Compiladores** e à avaliação da **AP1**.

---

## 2. Tecnologias utilizadas

- Python 3
- C
- GCC
- Git
- GitHub

---

## 3. Estrutura do projeto

```text
Ap1-Compiladores/
│
├── README.md
│
└── Minic/
    │
    ├── src/
    │   └── lexer/
    │       ├── lexer.py
    │       └── lexer.c
    │
    └── tests/
        ├── casos-invalidos/
        │   ├── i01_simbolo_desconhecido.minic
        │   ├── i01_simbolo_desconhecido.expected.jsonl
        │   ├── i01_simbolo_desconhecido.errors.jsonl
        │   ├── ...
        │
        ├── casos-programas-c/
        │   ├── c01_fibonacci.c
        │   ├── c01_fibonacci.c.expected.jsonl
        │   ├── c02_primos.c
        │   ├── c02_primos.c.expected.jsonl
        │   ├── c03_media_vetor.c
        │   ├── c03_media_vetor.c.expected.jsonl
        │   ├── c04_menu_interativo.c
        │   ├── c04_menu_interativo.c.expected.jsonl
        │   ├── c05_controle_temperatura.c
        │   └── c05_controle_temperatura.c.expected.jsonl
        │
        └── check_fixtures.py