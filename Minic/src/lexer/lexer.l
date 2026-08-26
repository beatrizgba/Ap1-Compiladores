%option noyywrap
%option noinput
%option nounput

%x BLOCK_COMMENT STRING_LITERAL CHAR_LITERAL

%{
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int current_line = 1;
static int current_column = 1;
static int token_line = 1;
static int token_column = 1;
static FILE *input_file = NULL;

static char *buffer = NULL;
static size_t buffer_len = 0;
static size_t buffer_cap = 0;

static void json_string(FILE *out, const char *s, size_t n) {
    fputc('"', out);
    for (size_t i = 0; i < n; ++i) {
        switch ((unsigned char)s[i]) {
            case '"': fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\n': fputs("\\n", out); break;
            case '\r': fputs("\\r", out); break;
            case '\t': fputs("\\t", out); break;
            default: fputc((unsigned char)s[i], out);
        }
    }
    fputc('"', out);
}

static void print_token(const char *type, const char *lexeme, size_t len,
                        const char *attr, int has_attr, int line, int col) {
    printf("{\"token\":\"%s\",\"lexeme\":", type);
    json_string(stdout, lexeme, len);
    printf(",\"attribute\":");
    if (has_attr) json_string(stdout, attr, strlen(attr));
    else printf("null");
    printf(",\"line\":%d,\"column\":%d}\n", line, col);
}

static void print_int(const char *s, size_t len, int line, int col) {
    char buf[128];
    size_t m = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
    memcpy(buf, s, m);
    buf[m] = '\0';
    printf("{\"token\":\"INT_LIT\",\"lexeme\":");
    json_string(stdout, s, len);
    printf(",\"attribute\":%ld,\"line\":%d,\"column\":%d}\n",
           strtol(buf, NULL, 10), line, col);
}

static void print_float(const char *s, size_t len, int line, int col) {
    char buf[128];
    size_t m = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
    memcpy(buf, s, m);
    buf[m] = '\0';
    const char *dot = strchr(buf, '.');
    int decimals = dot ? (int)strlen(dot + 1) : 0;
    printf("{\"token\":\"FLOAT_LIT\",\"lexeme\":");
    json_string(stdout, s, len);
    printf(",\"attribute\":%.*f,\"line\":%d,\"column\":%d}\n",
           decimals, strtod(buf, NULL), line, col);
}

static void print_error(const char *type, const char *lexeme, size_t len,
                        int line, int col) {
    fprintf(stderr, "{\"error\":\"%s\",\"lexeme\":", type);
    json_string(stderr, lexeme, len);
    fprintf(stderr, ",\"line\":%d,\"column\":%d}\n", line, col);
}

static const char *keyword(const char *s, size_t n) {
    static const char *words[][2] = {
        {"int","INT"}, {"float","FLOAT"}, {"bool","BOOL"}, {"char","CHAR"},
        {"void","VOID"}, {"if","IF"}, {"else","ELSE"}, {"while","WHILE"},
        {"for","FOR"}, {"return","RETURN"}, {"break","BREAK"},
        {"continue","CONTINUE"}, {"true","TRUE"}, {"false","FALSE"},
        {"print","PRINT"}, {"read","READ"}
    };
    for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); ++i) {
        if (strlen(words[i][0]) == n && strncmp(words[i][0], s, n) == 0)
            return words[i][1];
    }
    return NULL;
}

static void advance_text(const char *s, size_t n) {
    size_t i = 0;
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\n') {
            ++current_line;
            current_column = 1;
            ++i;
        } else {
            if (c < 0x80) {
                ++i;
            } else if ((c & 0xE0) == 0xC0 && i + 1 < n) {
                i += 2;
            } else if ((c & 0xF0) == 0xE0 && i + 2 < n) {
                i += 3;
            } else if ((c & 0xF8) == 0xF0 && i + 3 < n) {
                i += 4;
            } else {
                ++i;
            }
            ++current_column;
        }
    }
}

static void begin_token(void) {
    token_line = current_line;
    token_column = current_column;
}

static void buffer_reset(void) {
    buffer_len = 0;
    if (buffer) buffer[0] = '\0';
}

static void buffer_append(const char *s, size_t n) {
    if (n == 0) return;
    if (buffer_len + n + 1 > buffer_cap) {
        size_t new_cap = buffer_cap ? buffer_cap : 256;
        while (new_cap < buffer_len + n + 1) new_cap *= 2;
        char *tmp = realloc(buffer, new_cap);
        if (!tmp) {
            fprintf(stderr, "Erro: memória insuficiente.\n");
            exit(1);
        }
        buffer = tmp;
        buffer_cap = new_cap;
    }
    memcpy(buffer + buffer_len, s, n);
    buffer_len += n;
    buffer[buffer_len] = '\0';
}

static char *literal_attribute(const char *raw, size_t len) {
    if (len < 2) {
        char *empty = malloc(1);
        if (!empty) exit(1);
        empty[0] = '\0';
        return empty;
    }
    size_t n = len - 2;
    char *out = malloc(n + 1);
    if (!out) exit(1);
    size_t j = 0;
    for (size_t i = 0; i < n; ++i) {
        if (raw[i + 1] == '\\' && i + 2 < len - 1) {
            char e = raw[++i + 1];
            switch (e) {
                case 'n': out[j++] = '\n'; break;
                case 'r': out[j++] = '\r'; break;
                case 't': out[j++] = '\t'; break;
                case '\\': out[j++] = '\\'; break;
                case '"': out[j++] = '"'; break;
                case '\'': out[j++] = '\''; break;
                default: out[j++] = e; break;
            }
        } else {
            out[j++] = raw[i + 1];
        }
    }
    out[j] = '\0';
    return out;
}

static void sanitize_buffer_crlf(void) {
    size_t j = 0;
    for (size_t i = 0; i < buffer_len; ++i) {
        if (buffer[i] != '\r') {
            buffer[j++] = buffer[i];
        }
    }
    buffer_len = j;
    if (buffer) buffer[buffer_len] = '\0';
}

static void finish_buffered_block_comment(void) {
    sanitize_buffer_crlf();
    print_error("UNTERMINATED_BLOCK_COMMENT", buffer, buffer_len,
                token_line, token_column);
    advance_text(buffer, buffer_len);
    buffer_reset();
}

static void report_unterminated_string(void) {
    sanitize_buffer_crlf();
    print_error("UNTERMINATED_STRING_LITERAL", buffer, buffer_len,
                token_line, token_column);
    advance_text(buffer, buffer_len);
    buffer_reset();
    BEGIN(0);
}

static void report_unterminated_char(void) {
    sanitize_buffer_crlf();
    print_error("UNTERMINATED_CHAR_LITERAL", buffer, buffer_len,
                token_line, token_column);
    advance_text(buffer, buffer_len);
    buffer_reset();
    BEGIN(0);
}

%}

%%

<INITIAL>[ \t\r]+        { advance_text(yytext, yyleng); }
<INITIAL>\n              { advance_text(yytext, yyleng); }

<INITIAL>"//"[^\n]*     { advance_text(yytext, yyleng); }

<INITIAL>"/*"           {
                            begin_token();
                            buffer_reset();
                            buffer_append(yytext, yyleng);
                            BEGIN(BLOCK_COMMENT);
                          }

<BLOCK_COMMENT>"*/"     {
                            buffer_append(yytext, yyleng);
                            advance_text(buffer, buffer_len);
                            buffer_reset();
                            BEGIN(INITIAL);
                          }
<BLOCK_COMMENT>[^*\r\n]+ {
                            buffer_append(yytext, yyleng);
                          }
<BLOCK_COMMENT>"*"      {
                            buffer_append(yytext, yyleng);
                          }
<BLOCK_COMMENT>\r?\n    {
                            buffer_append(yytext, yyleng);
                          }
<BLOCK_COMMENT><<EOF>>   {
                            finish_buffered_block_comment();
                            print_token("EOF", "", 0, NULL, 0, current_line, current_column);
                            BEGIN(INITIAL);
                            yyterminate();
                          }

<INITIAL>"\""           {
                            begin_token();
                            buffer_reset();
                            buffer_append(yytext, yyleng);
                            BEGIN(STRING_LITERAL);
                          }

<STRING_LITERAL>\"      {
                            buffer_append(yytext, yyleng);
                            char *attr = literal_attribute(buffer, buffer_len);
                            print_token("STRING_LIT", buffer, buffer_len, attr, 1,
                                        token_line, token_column);
                            free(attr);
                            advance_text(buffer, buffer_len);
                            buffer_reset();
                            BEGIN(INITIAL);
                          }

<STRING_LITERAL>\\.     {
                            buffer_append(yytext, yyleng);
                          }

<STRING_LITERAL>[^"\\\r\n]+ {
                            buffer_append(yytext, yyleng);
                          }

<STRING_LITERAL>\r?\n   {
                            report_unterminated_string();
                            advance_text("\n", 1);
                          }

<STRING_LITERAL><<EOF>> {
                            report_unterminated_string();
                            print_token("EOF", "", 0, NULL, 0, current_line, current_column);
                            yyterminate();
                          }

<INITIAL>"'"             {
                            begin_token();
                            buffer_reset();
                            buffer_append(yytext, yyleng);
                            BEGIN(CHAR_LITERAL);
                          }

<CHAR_LITERAL>'          {
                            buffer_append(yytext, yyleng);
                            char *attr = literal_attribute(buffer, buffer_len);
                            print_token("CHAR_LIT", buffer, buffer_len, attr, 1,
                                        token_line, token_column);
                            free(attr);
                            advance_text(buffer, buffer_len);
                            buffer_reset();
                            BEGIN(INITIAL);
                          }

<CHAR_LITERAL>\\.        {
                            buffer_append(yytext, yyleng);
                          }

<CHAR_LITERAL>[^'\\\r\n;]+ {
                            buffer_append(yytext, yyleng);
                          }

<CHAR_LITERAL>";"        {
                            report_unterminated_char();
                            yyless(0);
                          }

<CHAR_LITERAL>\r?\n      {
                            report_unterminated_char();
                            advance_text("\n", 1);
                          }

<CHAR_LITERAL><<EOF>>    {
                            report_unterminated_char();
                            print_token("EOF", "", 0, NULL, 0, current_line, current_column);
                            yyterminate();
                          }

<INITIAL>[0-9]+\.[^0-9]  {
                            print_error("MALFORMED_REAL_LITERAL", yytext, yyleng - 1, current_line, current_column);
                            print_int(yytext, yyleng - 2, current_line, current_column);
                            advance_text(yytext, yyleng - 2);
                            print_token("DOT", ".", 1, NULL, 0, current_line, current_column);
                            advance_text(".", 1);
                            yyless(yyleng - 1);
                          }

<INITIAL>[0-9]+[A-Za-z_][A-Za-z0-9_]* {
                            print_error("INVALID_IDENTIFIER", yytext, yyleng, current_line, current_column);
                            size_t j = 0;
                            while (j < (size_t)yyleng && isdigit((unsigned char)yytext[j])) ++j;
                            print_int(yytext, j, current_line, current_column);
                            advance_text(yytext, j);
                            yyless((int)j);
                          }

<INITIAL>[0-9]+"."[0-9]+ {
                            print_float(yytext, yyleng, current_line, current_column);
                            advance_text(yytext, yyleng);
                          }

<INITIAL>[0-9]+          {
                            print_int(yytext, yyleng, current_line, current_column);
                            advance_text(yytext, yyleng);
                          }

<INITIAL>[A-Za-z_][A-Za-z0-9_]* {
                            const char *tok = keyword(yytext, yyleng);
                            if (tok) {
                                print_token(tok, yytext, yyleng, NULL, 0,
                                            current_line, current_column);
                            } else {
                                print_token("IDENT", yytext, yyleng, yytext, 1,
                                            current_line, current_column);
                            }
                            advance_text(yytext, yyleng);
                          }

<INITIAL>"=="            { print_token("EQ", yytext, 2, NULL, 0, current_line, current_column); advance_text(yytext,2); }
<INITIAL>"!="            { print_token("NE", yytext, 2, NULL, 0, current_line, current_column); advance_text(yytext,2); }
<INITIAL>"<="            { print_token("LE", yytext, 2, NULL, 0, current_line, current_column); advance_text(yytext,2); }
<INITIAL>">="            { print_token("GE", yytext, 2, NULL, 0, current_line, current_column); advance_text(yytext,2); }
<INITIAL>"&&"            { print_token("AND", yytext, 2, NULL, 0, current_line, current_column); advance_text(yytext,2); }
<INITIAL>"||"            { print_token("OR", yytext, 2, NULL, 0, current_line, current_column); advance_text(yytext,2); }

<INITIAL>"="             { print_token("ASSIGN",yytext,1,NULL,0,current_line,current_column); advance_text(yytext,1); }
<INITIAL>"<"             { print_token("LT",yytext,1,NULL,0,current_line,current_column); advance_text(yytext,1); }
<INITIAL>">"             { print_token("GT",yytext,1,NULL,0,current_line,current_column); advance_text(yytext,1); }
<INITIAL>"+"             { print_token("PLUS",yytext,1,NULL,0,current_line,current_column); advance_text(yytext,1); }
<INITIAL>"-"             { print_token("MINUS",yytext,1,NULL,0,current_line,current_column); advance_text(yytext,1); }
<INITIAL>"*"             { print_token("STAR",yytext,1,NULL,0,current_line,current_column); advance_text(yytext,1); }
<INITIAL>"/"             { print_token("SLASH",yytext,1,NULL,0,current_line,current_column); advance_text(yytext,1); }
<INITIAL>"%"             { print_token("PERCENT",yytext,1,NULL,0,current_line,current_column); advance_text(yytext,1); }
<INITIAL>"!"             { print_token("NOT",yytext,1,NULL,0,current_line,current_column); advance_text(yytext,1); }
<INITIAL>"("             { print_token("LPAREN",yytext,1,NULL,0,current_line,current_column); advance_text(yytext,1); }
<INITIAL>")"             { print_token("RPAREN",yytext,1,NULL,0,current_line,current_column); advance_text(yytext,1); }
<INITIAL>"["             { print_token("LBRACKET",yytext,1,NULL,0,current_line,current_column); advance_text(yytext,1); }
<INITIAL>"]"             { print_token("RBRACKET",yytext,1,NULL,0,current_line,current_column); advance_text(yytext,1); }
<INITIAL>"{"             { print_token("LBRACE",yytext,1,NULL,0,current_line,current_column); advance_text(yytext,1); }
<INITIAL>"}"             { print_token("RBRACE",yytext,1,NULL,0,current_line,current_column); advance_text(yytext,1); }
<INITIAL>";"             { print_token("SEMICOLON",yytext,1,NULL,0,current_line,current_column); advance_text(yytext,1); }
<INITIAL>","             { print_token("COMMA",yytext,1,NULL,0,current_line,current_column); advance_text(yytext,1); }
<INITIAL>"."             { print_token("DOT",yytext,1,NULL,0,current_line,current_column); advance_text(yytext,1); }

<INITIAL>(.|\n)          { print_error("UNKNOWN_SYMBOL",yytext,1,current_line,current_column); advance_text(yytext,1); }

<INITIAL><<EOF>>          {
                            print_token("EOF", "", 0, NULL, 0,
                                        current_line, current_column);
                            yyterminate();
                          }

%%

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Uso: lexer <arquivo>\n");
        return 1;
    }

    input_file = fopen(argv[1], "rb");
    if (!input_file) {
        fprintf(stderr, "Erro: arquivo '%s' não encontrado.\n", argv[1]);
        return 1;
    }

    yyin = input_file;
    yylex();

    free(buffer);
    fclose(input_file);
    return 0;
}