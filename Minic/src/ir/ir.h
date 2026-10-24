#ifndef MINIC_IR_H
#define MINIC_IR_H
#include "../common.h"
typedef struct IRInstruction { const char *op,*d,*a,*b,*typ; } IRInstruction;
typedef struct IR { Context *ctx; IRInstruction *code; size_t count,capacity; } IR;
IR *generate_ir(Context *ctx,Node *program);
const char *ir_emit(IR *ir,const char *op,const char *d,const char *a,const char *b,const char *typ);
void print_ir(IR *ir,FILE *out);
int validate_ir(IR *ir);
void optimize_ir(IR *ir);
int execute_ir(IR *ir);
void print_assembly(IR *ir,FILE *out);
int execute_assembly(IR *assembly);
IR *read_assembly(Context *ctx);
void print_dependencies(Context *ctx,Node *program,FILE *out);
void print_attributes(Context *ctx,Node *program,FILE *out);
void print_symbols(Context *ctx,Node *program,FILE *out);
void print_cfg(IR *ir,FILE *out);
#endif
