# AP 1 - Analisador Léxico (Linguagem MINIC)

Este repositório contém a implementação da **Etapa 1** do projeto de construção de um compilador para a linguagem **MINIC**, desenvolvido para a disciplina de **Compiladores**.

O objetivo desta etapa é implementar um **analisador léxico**, responsável por identificar os tokens válidos da linguagem, como palavras reservadas, identificadores, literais, operadores e delimitadores, além de reportar erros relacionados a símbolos ou formações inválidas.

O projeto possui implementações do analisador léxico em **Python** e em **C utilizando Flex**, permitindo a validação dos resultados por meio de uma suíte de testes automatizados.

---

## 👥 Integrantes do Grupo

* **Ana Clara T. Coser** — RA: 2401456
* **Beatriz Gabrielly Batista de Almeida** — RA: 2400276
* **Vinicius Henrique Santos** — RA: 2400306

---

## 📁 Estrutura do Repositório

```text
.
├── src/
│   └── lexer/
│       ├── lexer.py          # Analisador Léxico em Python
│       ├── lexer.l           # Especificação do analisador em Flex
│       ├── lexer.c           # Código C gerado pelo Flex
│       └── lexer.exe         # Executável do analisador em C
│
├── tests/
│   ├── casos-invalidos/      # Casos com erros léxicos (.minic e .jsonl)
│   ├── casos-programas-c/    # Programas válidos em C/MINIC (.c e .jsonl)
│   ├── resultados/           # Arquivos de saída gerados pela suíte de testes
│   │   └── python/           # Saídas em JSONL dos testes em Python
│   ├── check_fixtures.py     # Script de verificação das fixtures
│   └── run_tests.py          # Script de execução dos testes
│
├── .gitignore
└── README.md
```

---

## 🛠️ Tecnologias Utilizadas

* Python
* C
* Flex / WinFlex
* GCC
* JSONL
* PowerShell

---

## ⚙️ Requisitos

Para executar o projeto, é necessário ter instalado:

* Python 3
* GCC
* WinFlex
* Ambiente compatível com a compilação de programas em C

No Windows, o GCC pode ser utilizado por meio do **MSYS2 UCRT64**.

---

## ▶️ Execução do Analisador em C

O arquivo `lexer.l` contém a especificação das regras léxicas. O código C utilizado pelo compilador é gerado pelo Flex.

A partir da raiz do projeto, execute:

```powershell
win_flex -o src/lexer/lexer.c src/lexer/lexer.l
```

Em seguida, compile o código gerado:

```powershell
gcc src/lexer/lexer.c -o src/lexer/lexer.exe
```

---

## 🧪 Execução dos Testes

A suíte de testes pode ser executada a partir da raiz do projeto com:

```powershell
python tests/run_tests.py
```

O script executa os casos de teste para as implementações em Python e C e compara os tokens produzidos com os resultados esperados.

---

## ✅ Resultado dos Testes

A implementação atual foi validada com os casos de teste fornecidos.

### Python

```text
Python: TODOS OS TESTES OK
```

### C

```text
C: TODOS OS TESTES OK
```

Foram aprovados os seguintes casos:

* `c01_fibonacci.c`
* `c02_primos.c`
* `c03_media_vetor.c`
* `c04_menu_interativo.c`
* `c05_controle_temperatura.c`
* `i01_simbolo_desconhecido.minic`
* `i02_comentario_nao_terminado.minic`
* `i03_caractere_nao_terminado.minic`
* `i04_cadeia_nao_terminada.minic`
* `i05_numero_real_malformado.minic`

O caso `i06_identificador_iniciado_por_digito` possui arquivos de resultado esperados, porém o arquivo `.minic` correspondente não foi fornecido na suíte de testes. Portanto, esse caso não pode ser executado automaticamente.

---

## 📌 Funcionamento

O analisador léxico recebe um código-fonte da linguagem MINIC e realiza a leitura caractere por caractere, agrupando os caracteres em unidades léxicas chamadas **tokens**.

Entre os elementos reconhecidos estão:

* Palavras reservadas
* Identificadores
* Números inteiros
* Números reais
* Literais de caracteres
* Literais de cadeia
* Operadores
* Delimitadores
* Comentários

Cada token identificado possui informações como:

* Tipo do token
* Lexema
* Atributo, quando aplicável
* Linha
* Coluna

Os resultados são produzidos no formato **JSONL**, facilitando a comparação automática entre a saída produzida pelo analisador e os resultados esperados.

---

## ❌ Tratamento de Erros Léxicos

O analisador também identifica formações inválidas, como:

* Símbolos desconhecidos
* Comentários não terminados
* Literais de caracteres não terminados
* Literais de cadeia não terminados
* Números reais malformados
* Outras construções incompatíveis com as regras léxicas da linguagem

Os erros são reportados juntamente com informações de localização no código-fonte, permitindo identificar a linha e a coluna em que o problema ocorreu.

---

## 📄 Formato de Saída

Os tokens são representados em formato JSONL. Um exemplo de saída é:

```json
{"token":"INT","lexeme":"int","attribute":null,"line":1,"column":1}
{"token":"IDENT","lexeme":"x","attribute":"x","line":1,"column":5}
{"token":"ASSIGN","lexeme":"=","attribute":null,"line":1,"column":7}
```

Cada linha representa um token identificado pelo analisador léxico.

---

## 📚 Etapa do Projeto

Este repositório corresponde à:

**AP 1 — Etapa 1: Analisador Léxico**

O analisador léxico constitui a primeira etapa do processo de compilação, sendo responsável por transformar o código-fonte em uma sequência de tokens que poderá ser utilizada pelas próximas etapas do compilador.
