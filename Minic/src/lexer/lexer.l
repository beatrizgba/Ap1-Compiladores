#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    memcpy(buf, s, m); buf[m] = '\0';
    printf("{\"token\":\"INT_LIT\",\"lexeme\":");
    json_string(stdout, s, len);
    printf(",\"attribute\":%ld,\"line\":%d,\"column\":%d}\n",
           strtol(buf, NULL, 10), line, col);
}

static void print_float(const char *s, size_t len, int line, int col) {
    char buf[128];
    size_t m = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
    memcpy(buf, s, m); buf[m] = '\0';
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
        {"int","INT"},{"float","FLOAT"},{"bool","BOOL"},{"char","CHAR"},
        {"void","VOID"},{"if","IF"},{"else","ELSE"},{"while","WHILE"},
        {"for","FOR"},{"return","RETURN"},{"break","BREAK"},
        {"continue","CONTINUE"},{"true","TRUE"},{"false","FALSE"},
        {"print","PRINT"},{"read","READ"}
    };
    for (size_t i = 0; i < sizeof(words)/sizeof(words[0]); ++i)
        if (strlen(words[i][0]) == n && strncmp(words[i][0], s, n) == 0)
            return words[i][1];
    return NULL;
}

static void advance(const char *s, size_t n, int *line, int *col) {
    for (size_t i = 0; i < n; ++i) {
        if (s[i] == '\n') { ++*line; *col = 1; }
        else ++*col;
    }
}

static int ident_start(unsigned char c) { return isalpha(c) || c == '_'; }
static int ident_char(unsigned char c) { return isalnum(c) || c == '_'; }

static char *load_file(const char *path, size_t *size) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    rewind(fp);
    if (n < 0) { fclose(fp); return NULL; }
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(fp); return NULL; }
    *size = fread(buf, 1, (size_t)n, fp);
    buf[*size] = '\0';
    fclose(fp);
    return buf;
}

static void tokenize(const char *s, size_t n) {
    size_t i = 0;
    int line = 1, col = 1;

    while (i < n) {
        int sl = line, sc = col;
        unsigned char c = (unsigned char)s[i];

        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance(s+i, 1, &line, &col); ++i; continue;
        }

        if (i+1 < n && s[i]=='/' && s[i+1]=='/') {
            size_t j=i+2;
            while (j<n && s[j]!='\n') ++j;
            advance(s+i, j-i, &line, &col); i=j; continue;
        }

        if (i+1 < n && s[i]=='/' && s[i+1]=='*') {
            size_t j=i+2;
            while (j+1<n && !(s[j]=='*' && s[j+1]=='/')) ++j;
            if (j+1>=n) {
                print_error("UNTERMINATED_BLOCK_COMMENT", s+i, n-i, sl, sc);
                advance(s+i, n-i, &line, &col); i=n; continue;
            }
            j += 2;
            advance(s+i, j-i, &line, &col); i=j; continue;
        }

        if (c == '"') {
            size_t j=i+1; int closed=0;
            while (j<n && s[j]!='\n') {
                if (s[j]=='"' && s[j-1]!='\\') { closed=1; break; }
                ++j;
            }
            if (!closed) {
                size_t resume = j;
                for (size_t k = i + 1; k < j; ++k) {
                    if (s[k] == ')' || s[k] == ';') {
                        resume = k;
                        break;
                    }
                }

                
                print_error("UNTERMINATED_STRING_LITERAL", s+i, j-i, sl, sc);
                advance(s+i, resume-i, &line, &col);
                i=resume;
                continue;
            }
            size_t len=j-i+1;
            char *attr=malloc(len);
            if (!attr) exit(1);
            if (len>1) { memcpy(attr,s+i+1,len-2); attr[len-2]='\0'; }
            print_token("STRING_LIT", s+i, len, attr, 1, sl, sc);
            free(attr);
            advance(s+i,len,&line,&col); i=j+1; continue;
        }

        if (c == '\'') {
            size_t j=i+1; int closed=0;
            while (j<n && s[j]!='\n' && s[j]!='\r' &&
                   s[j]!=';' && s[j]!=')' && s[j]!=']' && s[j]!='}') {
                if (s[j]=='\'') { closed=1; break; }
                ++j;
            }
            if (!closed) {
                size_t len=j-i;
                print_error("UNTERMINATED_CHAR_LITERAL", s+i, len, sl, sc);
                if (j<n && s[j]==';') {
                    advance(s+i, len+1, &line, &col);
                    i=j+1;
                } else {
                    advance(s+i, len, &line, &col);
                    i=j;
                }
                continue;
            }
            size_t len=j-i+1;
            char attr[8]={0};
            if (len>=3) {
                size_t m=len-2 < sizeof(attr)-1 ? len-2 : sizeof(attr)-1;
                memcpy(attr,s+i+1,m);
                attr[m]='\0';
            }
            print_token("CHAR_LIT",s+i,len,attr,1,sl,sc);
            advance(s+i,len,&line,&col); i=j+1; continue;
        }

        if (isdigit(c)) {
            size_t j=i;
            while (j<n && isdigit((unsigned char)s[j])) ++j;

            if (j<n && s[j]=='.' && (j+1>=n || !isdigit((unsigned char)s[j+1]))) {
                print_error("MALFORMED_REAL_LITERAL",s+i,j-i+1,sl,sc);
                print_int(s+i,j-i,sl,sc);
                advance(s+i,j-i,&line,&col); i=j; continue;
            }

            if (j<n && ident_start((unsigned char)s[j])) {
                size_t k=j+1;
                while (k<n && ident_char((unsigned char)s[k])) ++k;
                print_error("INVALID_IDENTIFIER",s+i,k-i,sl,sc);
            }

            if (j<n && s[j]=='.' && j+1<n && isdigit((unsigned char)s[j+1])) {
                size_t k=j+1;
                while (k<n && isdigit((unsigned char)s[k])) ++k;
                print_float(s+i,k-i,sl,sc);
                advance(s+i,k-i,&line,&col); i=k; continue;
            }

            print_int(s+i,j-i,sl,sc);
            advance(s+i,j-i,&line,&col); i=j; continue;
        }

        if (ident_start(c)) {
            size_t j=i+1;
            while (j<n && ident_char((unsigned char)s[j])) ++j;
            const char *tok=keyword(s+i,j-i);
            if (tok) print_token(tok,s+i,j-i,NULL,0,sl,sc);
            else {
                char *attr=malloc(j-i+1);
                if (!attr) exit(1);
                memcpy(attr,s+i,j-i); attr[j-i]='\0';
                print_token("IDENT",s+i,j-i,attr,1,sl,sc);
                free(attr);
            }
            advance(s+i,j-i,&line,&col); i=j; continue;
        }

        if (i+1<n) {
            const char *tok=NULL;
            if (s[i]=='='&&s[i+1]=='=') tok="EQ";
            else if (s[i]=='!'&&s[i+1]=='=') tok="NE";
            else if (s[i]=='<'&&s[i+1]=='=') tok="LE";
            else if (s[i]=='>'&&s[i+1]=='=') tok="GE";
            else if (s[i]=='&'&&s[i+1]=='&') tok="AND";
            else if (s[i]=='|'&&s[i+1]=='|') tok="OR";
            if (tok) {
                print_token(tok,s+i,2,NULL,0,sl,sc);
                advance(s+i,2,&line,&col); i+=2; continue;
            }
        }

        const char *tok=NULL;
        switch(c) {
            case '=':tok="ASSIGN";break; case '<':tok="LT";break;
            case '>':tok="GT";break; case '+':tok="PLUS";break;
            case '-':tok="MINUS";break; case '*':tok="STAR";break;
            case '/':tok="SLASH";break; case '%':tok="PERCENT";break;
            case '!':tok="NOT";break; case '(':tok="LPAREN";break;
            case ')':tok="RPAREN";break; case '[':tok="LBRACKET";break;
            case ']':tok="RBRACKET";break; case '{':tok="LBRACE";break;
            case '}':tok="RBRACE";break; case ';':tok="SEMICOLON";break;
            case ',':tok="COMMA";break; case '.':tok="DOT";break;
        }

        if (tok) print_token(tok,s+i,1,NULL,0,sl,sc);
        else print_error("UNKNOWN_SYMBOL",s+i,1,sl,sc);
        advance(s+i,1,&line,&col); ++i;
    }

    print_token("EOF","",0,NULL,0,line,col);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr,"Uso: lexer <arquivo>\n");
        return 1;
    }
    size_t size=0;
    char *code=load_file(argv[1],&size);
    if (!code) {
        fprintf(stderr,"Erro: arquivo '%s' não encontrado.\n",argv[1]);
        return 1;
    }
    tokenize(code,size);
    free(code);
    return 0;
}
