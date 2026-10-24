#ifndef MINIC_COMMON_H
#define MINIC_COMMON_H
#include <stddef.h>
#include <stdio.h>

typedef struct Allocation { void *data; struct Allocation *next; } Allocation;
typedef struct Token { char *kind, *text; int line, column; size_t start, end; } Token;
typedef enum { N_PROGRAM, N_FUNCTION, N_PARAM, N_DECL, N_BLOCK, N_IF, N_WHILE,
               N_RETURN, N_EXPR, N_EMPTY, N_LITERAL, N_ID, N_CALL, N_INDEX,
               N_ASSIGN, N_BINARY, N_UNARY, N_BREAK, N_CONTINUE, N_FOR } Kind;
typedef struct Symbol Symbol;
enum { FLOW_NORMAL=1, FLOW_RETURN=2, FLOW_BREAK=4, FLOW_CONTINUE=8 };
typedef struct Node {
    Kind kind; Token *token; size_t end;
    char *value, *type_name;
    struct Node *children, *next, *left, *right, *body, *otherwise;
    struct Node *size, *initializer, *params;
    int array;
    /* Anotações semânticas preservadas para a próxima etapa. */
    const char *semantic_type, *category, *conversion_to, *expected_type;
    Symbol *symbol;
    long long array_size;
    int has_array_size, assignable, loop_depth, flow;
    const char *ir_place;
    int global_scope;
    size_t ir_begin,ir_end;
} Node;
struct Symbol { Node *node; const char *typ; int function; Symbol *next; };
typedef struct Diagnostic { char *code, *message; int line, column; size_t order; } Diagnostic;
typedef struct Context {
    char *source;
    size_t source_length;
    Allocation *allocations;
    int trace_parser, final_mode;
    Token *tokens; size_t count, capacity;
    char *stage, *failure; int failure_line, failure_column;
    char *syntax_report;int syntax_count;
    Diagnostic *errors; size_t error_count, error_capacity;
} Context;

void *allocate(Context *ctx, size_t size);
void configure_output(void);
char *copy_text(Context *ctx, const char *text, size_t length);
char *format_text(Context *ctx, const char *format, ...);
char *source_text(Context *ctx, Node *node);
size_t source_position(Context *ctx, int line, int column);
void context_free(Context *ctx);
int read_source(Context *ctx, const char *path);
void print_failure(Context *ctx);
void add_error(Context *ctx, const char *code, Node *node, const char *message);
void print_semantics(Context *ctx);
int scan_source(Context *ctx);
Node *parse_program(Context *ctx);
void print_ast(Node *node, FILE *out);
void analyze_semantics(Context *ctx, Node *program);
#endif
