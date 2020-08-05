#include "lval.h"

/* Lval Constructors */
lval* lval_int(long x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_INT;
    v->data.integer = x;
    return v;
}

lval* lval_dec(double x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_DEC;
    v->data.decimal = x;
    return v;
}

lval* lval_bool(bool boolean) {
    lval* b = malloc(sizeof(lval));
    b->type = LVAL_BOOL;
    b->data.boolean = boolean;
    return b;
}

lval* lval_str(char* s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_STR;
    v->data.str = malloc(strlen(s) + 1);
    strcpy(v->data.str, s);
    return v;
}

lval* lval_err(char* fmt, ...) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    
    /* Create a va list and initialize it */
    va_list va;
    va_start(va, fmt);
    
    /* Allocate 512 bytes of space */
    v->data.err = malloc(512);
    
    /* printf the error string with a maximum of 511 characters */
    vsnprintf(v->data.err, 511, fmt, va);
    
    /* Reallocate to number of bytes actually used */
    v->data.err = realloc(v->data.err, strlen(v->data.err)+1);
    
    /* Cleanup our va list */
    va_end(va);
    
    return v;
}

lval* lval_sym(char* s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->data.sym = malloc(strlen(s) + 1);
    strcpy(v->data.sym, s);
    return v;
}

lval* lval_fun(lbuiltin func) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->builtin = func;
    return v;
}

lval* lval_lambda(lval* formals, lval* body) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->builtin = NULL;
    v->env = lenv_new();
    v->formals = formals;
    v->body = body;
    return v;
}

lval* lval_sexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

lval* lval_qexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}


/* Recursively deletes an lval */
void lval_del(lval* v) {
    switch (v->type) {
        case LVAL_INT: break;
        case LVAL_DEC: break;
        case LVAL_BOOL: break;
        case LVAL_FUN: 
            if (!v->builtin) {
                lval_del(v->formals);
                lval_del(v->body);
                lenv_del(v->env);
            }
            break;
        case LVAL_ERR: free(v->data.err); break;
        case LVAL_SYM: free(v->data.sym); break;
        case LVAL_STR: free(v->data.str); break;
        case LVAL_QEXPR:
        case LVAL_SEXPR:
            for (int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }
            free(v->cell);
        break;
    }
    
    free(v);
}


/* Returns a copy of the lval passed as an argument */
lval* lval_copy(lval* v) {
    lval* x = malloc(sizeof(lval));
    x->type = v->type;
    
    switch (v->type) {
        /* Copy Numbers and Bools Directly */
        case LVAL_DEC: x->data.decimal = v->data.decimal; break;
        case LVAL_INT: x->data.integer = v->data.integer; break;
        case LVAL_BOOL: x->data.boolean = v->data.boolean; break; 
        case LVAL_FUN: 
            if (v->builtin) {
                x->builtin = v->builtin;
            }
            else {
                x->builtin = NULL;
                x->env = lenv_copy(v->env);
                x->formals = lval_copy(v->formals);
                x->body = lval_copy(v->body);
            }
            break;
        /* Copy Strings using malloc and strcpy */
        case LVAL_ERR:
            x->data.err = malloc(strlen(v->data.err) + 1);
            strcpy(x->data.err, v->data.err); break;
        case LVAL_STR:
            x->data.str = malloc(strlen(v->data.str) + 1);
            strcpy(x->data.str, v->data.str); break;
        case LVAL_SYM:
            x->data.sym = malloc(strlen(v->data.sym) + 1);
            strcpy(x->data.sym, v->data.sym); break;
        
        /* Copy Lists by copying each sub-expression */
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count = v->count;
            x->cell = malloc(sizeof(lval*) * x->count);
            for (int i = 0; i < x->count; i++) {
                x->cell[i] = lval_copy(v->cell[i]);
            }
        break;
    }
    return x;
}

/* Adds an element (x) to lval v */
lval* lval_add(lval* v, lval* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count-1] = x;
    return v;
}

/* Joins lval y to lval x, deleting y */
lval* lval_join(lval* x, lval* y) {  
    for (int i = 0; i < y->count; i++) {
        x = lval_add(x, y->cell[i]);
    }
    free(y->cell);
    free(y);  
    return x;
}

/* Pops and returns the ith element of an lval */
lval* lval_pop(lval* v, int i) {
    lval* x = v->cell[i];  
    memmove(&v->cell[i], &v->cell[i+1],
        sizeof(lval*) * (v->count-i-1));  
    v->count--;  
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    return x;
}

/* Returns the ith element of an lval, deleting the rest */
lval* lval_take(lval* v, int i) {
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

/* Prints an expression */
void lval_print_expr(lval* v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {
        lval_print(v->cell[i]);    
        if (i != (v->count-1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

void lval_print_str(lval* v) {
    /* Make a copy of string  */
    char* escaped = malloc(strlen(v->data.str) + 1);
    strcpy(escaped, v->data.str);
    /* Escape the string */
    escaped = mpcf_escape(escaped);
    /* Print between quotes */
    printf("\"%s\"", escaped);
    free(escaped);
}

/* Prints an lval */
void lval_print(lval* v) {
    switch (v->type) {
        case LVAL_FUN:   
            if (v->builtin) {
                printf("%s", func_name(v));
            }
            else {
                printf("(\\ ");
                lval_print(v->formals);
                putchar(' ');
                lval_print(v->body);
                putchar(')');
            }
            break;
        case LVAL_INT:   printf("%li", v->data.integer); break;
        case LVAL_DEC:   printf("%f", v->data.decimal); break;
        case LVAL_BOOL:  printf("%s", v->data.boolean ? "true" : "false"); break;
        case LVAL_ERR:   printf("Error: %s", v->data.err); break;
        case LVAL_SYM:   printf("%s", v->data.sym); break;
        case LVAL_STR:   lval_print_str(v); break;
        case LVAL_SEXPR: lval_print_expr(v, '(', ')'); break;
        case LVAL_QEXPR: lval_print_expr(v, '{', '}'); break;
    }
}

void lval_println(lval* v) {
    lval_print(v); 
    putchar('\n'); 
}


/* Returns the string name of an lval type */
char* ltype_name(int t) {
    switch(t) {
        case LVAL_FUN: return "Function";
        case LVAL_INT: return "Number";
        case LVAL_BOOL: return "Boolean";
        case LVAL_DEC: return "Decimal";
        case LVAL_ERR: return "Error";
        case LVAL_SYM: return "Symbol";
        case LVAL_STR: return "String";
        case LVAL_SEXPR: return "S-Expression";
        case LVAL_QEXPR: return "Q-Expression";
        default: return "Unknown";
    }
}

