#ifndef lval_h
#define lval_h

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include "mpc.h"

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;
typedef lval*(*lbuiltin)(lenv*, lval*);

/* Lisp Value */

enum { LVAL_ERR, LVAL_INT, LVAL_DEC, LVAL_BOOL, LVAL_STR,
        LVAL_SYM, LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR };


struct lval {
    int type;

    /* Number, Symbol, and Error data */
    union {
        long integer;
        double decimal;
        bool boolean;
        char* str;
        char* sym;
        char* err;
    } data;

    /* Functions */
    lbuiltin builtin;
    lenv* env;
    lval* formals;
    lval* body;

    /* Expressions */
    int count;
    lval** cell;
};

lenv* lenv_new();
void lenv_del(lenv* e);
lenv* lenv_copy(lenv* e);
char* func_name(lval* func); 
lval* lval_err(char* fmt, ...);
void lval_del(lval* v);
char* ltype_name(int t);
lval* lval_str(char* s);
lval* lval_sym(char* s);
lval* lval_int(long x);
lval* lval_dec(double x);
lval* lval_bool(bool boolean);
lval* lval_qexpr(void);
lval* lval_sexpr(void);
lval* lval_fun(lbuiltin func);
lval* lval_lambda(lval* formals, lval* body);
lval* lval_pop(lval* v, int i);
lval* lval_take(lval* v, int i);
lval* lval_copy(lval* v);
lval* lval_add(lval* v, lval* x);
lval* lval_join(lval* x, lval* y);
void lval_print(lval* v);
void lval_println(lval* v);

#endif
