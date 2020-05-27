#include "mpc.h"
#include <stdbool.h>

#ifdef _WIN32

static char buffer[2048];

char* readline(char* prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char* cpy = malloc(strlen(buffer)+1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy)-1] = '\0';
    return cpy;
}

void add_history(char* unused) {}

#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

/* Forward Declarations */

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

/* Lisp Value */

enum { LVAL_ERR, LVAL_INT, LVAL_DEC,  LVAL_SYM, 
              LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR };

typedef lval*(*lbuiltin)(lenv*, lval*);

struct lval {
    int type;
    int count;

    union {
        long integer;
        double decimal;
        char* err;
        char* sym;
        lbuiltin fun;
        lval** cell;
    } data;
};

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
    v->data.fun = func;
    return v;
}

lval* lval_sexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->data.cell = NULL;
    return v;
}

lval* lval_qexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->data.cell = NULL;
    return v;
}

/* Recursively deletes an lval */
void lval_del(lval* v) {

    switch (v->type) {
        case LVAL_INT: break;
        case LVAL_DEC: break;
        case LVAL_FUN: break;
        case LVAL_ERR: free(v->data.err); break;
        case LVAL_SYM: free(v->data.sym); break;
        case LVAL_QEXPR:
        case LVAL_SEXPR:
            for (int i = 0; i < v->count; i++) {
                lval_del(v->data.cell[i]);
            }
            free(v->data.cell);
        break;
    }
    
    free(v);
}

/* Returns a copy of the lval passed as an argument */
lval* lval_copy(lval* v) {

    lval* x = malloc(sizeof(lval));
    x->type = v->type;
    
    switch (v->type) {
        
        /* Copy Functions and Numbers Directly */
        case LVAL_FUN: x->data.fun = v->data.fun; break; 
        case LVAL_DEC: x->data.decimal = v->data.decimal; break;
        case LVAL_INT: x->data.integer = v->data.integer; break;
        
        /* Copy Strings using malloc and strcpy */
        case LVAL_ERR:
            x->data.err = malloc(strlen(v->data.err) + 1);
            strcpy(x->data.err, v->data.err); break;
            
        case LVAL_SYM:
            x->data.sym = malloc(strlen(v->data.sym) + 1);
            strcpy(x->data.sym, v->data.sym); break;
        
        /* Copy Lists by copying each sub-expression */
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count = v->count;
            x->data.cell = malloc(sizeof(lval*) * x->count);
            for (int i = 0; i < x->count; i++) {
                x->data.cell[i] = lval_copy(v->data.cell[i]);
            }
        break;
    }
    
    return x;
}

/* Adds an element (x) to lval v */
lval* lval_add(lval* v, lval* x) {
    v->count++;
    v->data.cell = realloc(v->data.cell, sizeof(lval*) * v->count);
    v->data.cell[v->count-1] = x;
    return v;
}

/* Joins lval y to lval x, deleting y */
lval* lval_join(lval* x, lval* y) {  
    for (int i = 0; i < y->count; i++) {
        x = lval_add(x, y->data.cell[i]);
    }
    free(y->data.cell);
    free(y);  
    return x;
}

/* Pops and returns the ith element of an lval */
lval* lval_pop(lval* v, int i) {
    lval* x = v->data.cell[i];  
    memmove(&v->data.cell[i], &v->data.cell[i+1],
        sizeof(lval*) * (v->count-i-1));  
    v->count--;  
    v->data.cell = realloc(v->data.cell, sizeof(lval*) * v->count);
    return x;
}

/* Returns the ith element of an lval, deleting the rest */
lval* lval_take(lval* v, int i) {
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

void lval_print(lval* v);

/* Prints an expression */
void lval_print_expr(lval* v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {
        lval_print(v->data.cell[i]);    
        if (i != (v->count-1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

/* Prints an lval */
void lval_print(lval* v) {
    switch (v->type) {
        case LVAL_FUN:   printf("<function>"); break;
        case LVAL_INT:   printf("%li", v->data.integer); break;
        case LVAL_DEC:   printf("%f", v->data.decimal); break;
        case LVAL_ERR:   printf("Error: %s", v->data.err); break;
        case LVAL_SYM:   printf("%s", v->data.sym); break;
        case LVAL_SEXPR: lval_print_expr(v, '(', ')'); break;
        case LVAL_QEXPR: lval_print_expr(v, '{', '}'); break;
    }
}

void lval_println(lval* v) { lval_print(v); putchar('\n'); }

/* Returns the string name of an lval type */
char* ltype_name(int t) {
    switch(t) {
        case LVAL_FUN: return "Function";
        case LVAL_INT: return "Number";
        case LVAL_DEC: return "Decimal";
        case LVAL_ERR: return "Error";
        case LVAL_SYM: return "Symbol";
        case LVAL_SEXPR: return "S-Expression";
        case LVAL_QEXPR: return "Q-Expression";
        default: return "Unknown";
    }
}

/* Lisp Environment */

struct lenv {
    int count;
    char** syms;
    lval** vals;
};

/* Creates a new environment */
lenv* lenv_new(void) {

    /* Initialize struct */
    lenv* e = malloc(sizeof(lenv));
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
    
}

/* Deletes the environment */
void lenv_del(lenv* e) {
    
    /* Iterate over all items in environment deleting them */
    for (int i = 0; i < e->count; i++) {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    
    /* Free allocated memory for lists */
    free(e->syms);
    free(e->vals);
    free(e);
}

/* Returns the value for a symbol in the environment */
lval* lenv_get(lenv* e, lval* k) {
    
    /* Iterate over all items in environment */
    for (int i = 0; i < e->count; i++) {
        /* Check if the stored string matches the symbol string */
        /* If it does, return a copy of the value */
        if (strcmp(e->syms[i], k->data.sym) == 0) {
            return lval_copy(e->vals[i]);
        }
    }
    /* If no symbol found return error */
    return lval_err("Unbound Symbol '%s'", k->data.sym);
}

/* Puts a symbol and a value into the environment or
 * if the sybol exists, changes the value */
void lenv_put(lenv* e, lval* k, lval* v) {
    
    /* Iterate over all items in environment */
    /* This is to see if variable already exists */
    for (int i = 0; i < e->count; i++) {
    
        /* If variable is found delete item at that position */
        /* And replace with variable supplied by user */
        if (strcmp(e->syms[i], k->data.sym) == 0) {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }
    
    /* If no existing entry found allocate space for new entry */
    e->count++;
    e->vals = realloc(e->vals, sizeof(lval*) * e->count);
    e->syms = realloc(e->syms, sizeof(char*) * e->count);
    
    /* Copy contents of lval and symbol string into new location */
    e->vals[e->count-1] = lval_copy(v);
    e->syms[e->count-1] = malloc(strlen(k->data.sym)+1);
    strcpy(e->syms[e->count-1], k->data.sym);
}

/* Builtins */

#define LASSERT(args, cond, fmt, ...) \
    if (!(cond)) { lval* err = lval_err(fmt, ##__VA_ARGS__); lval_del(args); return err; }

#define LASSERT_TYPE(func, args, index, expect) \
    LASSERT(args, args->data.cell[index]->type == expect, \
        "Function '%s' passed incorrect type for argument %i. Got %s, Expected %s.", \
        func, index, ltype_name(args->data.cell[index]->type), ltype_name(expect))

#define LASSERT_NUM(func, args, num) \
    LASSERT(args, args->count == num, \
        "Function '%s' passed incorrect number of arguments. Got %i, Expected %i.", \
        func, args->count, num)

#define LASSERT_NOT_EMPTY(func, args, index) \
    LASSERT(args, args->data.cell[index]->count != 0, \
        "Function '%s' passed {} for argument %i.", func, index);


lval* lval_eval(lenv* e, lval* v);

/* Takes arguments and returns a Q-Expression containing those arguments */
lval* builtin_list(lenv* e, lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

/* Returns the first element of a Q-Expression */
lval* builtin_head(lenv* e, lval* a) {
    LASSERT_NUM("head", a, 1);
    LASSERT_TYPE("head", a, 0, LVAL_QEXPR);
    LASSERT_NOT_EMPTY("head", a, 0);
    
    lval* v = lval_take(a, 0);  
    while (v->count > 1) { lval_del(lval_pop(v, 1)); }
    return v;
}

/* Returns all but the first element of a Q-Expression */
lval* builtin_tail(lenv* e, lval* a) {
    LASSERT_NUM("tail", a, 1);
    LASSERT_TYPE("tail", a, 0, LVAL_QEXPR);
    LASSERT_NOT_EMPTY("tail", a, 0);

    lval* v = lval_take(a, 0);  
    lval_del(lval_pop(v, 0));
    return v;
}

/* Takes a Q-Expr and evaluates it as if it were an S-Expr */
lval* builtin_eval(lenv* e, lval* a) {
    LASSERT_NUM("eval", a, 1);
    LASSERT_TYPE("eval", a, 0, LVAL_QEXPR);
    
    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);
}

/* Takes one or more Q-Expressions and returns a Q-Expr with them joined together */
lval* builtin_join(lenv* e, lval* a) {
    
    for (int i = 0; i < a->count; i++) {
        LASSERT_TYPE("join", a, i, LVAL_QEXPR);
    }
    
    lval* x = lval_pop(a, 0);
    
    while (a->count) {
        lval* y = lval_pop(a, 0);
        x = lval_join(x, y);
    }
    
    lval_del(a);
    return x;
}

/* Returns the number of elements in a Q-Expr */
lval* builtin_len(lenv* e, lval* a) {
    LASSERT_TYPE("len", a, 0, LVAL_QEXPR)
    lval* x = lval_int(a->data.cell[0]->count);
    lval_del(a);
    return x;
}

/* Returns all of a Q-Expr except the final element */
lval* builtin_init(lenv* e, lval* a) {
    LASSERT_TYPE("init", a, 0, LVAL_QEXPR)
    LASSERT_NUM("init", a, 1)

    lval* x = lval_copy(a->data.cell[0]);

    lval* y = lval_pop(x, x->count - 1);
    lval_del(y);
    lval_del(a);
    return x;
}

/* Takes a value and Q-Expr and appends value to the front */
lval* builtin_cons(lenv* e, lval* a) {
    LASSERT_NUM("cons", a, 2)
    LASSERT_TYPE("cons", a, 1, LVAL_QEXPR)

    lval* q = lval_qexpr();
    lval_add(q, lval_pop(a, 0));

    q = lval_join(q, lval_take(a, 0));

    return q;
}

/* Evaluates basic math operations on integers */
lval* builtin_op_i(lenv* e, lval* a, char* op) {
    for (int i = 0; i < a->count; i++) {
        LASSERT_TYPE("builtin_op_i", a, i, LVAL_INT)
    }   

    // pop the first value into a temporary variable  
    lval* x = lval_pop(a, 0);

    // put the value into another variable? 
    if ((strcmp(op, "-") == 0) && a->count == 0) {
        x->data.integer = -x->data.integer;
    }
    
    while (a->count > 0) {  
        lval* y = lval_pop(a, 0);
        
        if (strcmp(op, "+") == 0) { x->data.integer += y->data.integer; }
        if (strcmp(op, "-") == 0) { x->data.integer -= y->data.integer; }
        if (strcmp(op, "*") == 0) { x->data.integer *= y->data.integer; }
        if (strcmp(op, "/") == 0) {
            if (y->data.integer == 0) {
                lval_del(x); lval_del(y);
                x = lval_err("Division By Zero.");
                break;
            }
            x->data.integer /= y->data.integer;
        }
        if (strcmp(op, "%") == 0) {
            if (y->data.integer == 0) {
                lval_del(x); lval_del(y);
                x = lval_err("Division By Zero.");
                break;
            }
            x->data.integer %= y->data.integer;
        }   
        if (strcmp(op, "^") == 0) { 
            if (y->data.integer < 0) {
                lval_del(x); lval_del(y);
                x = lval_err("Unable to raise to negative power.");
                break;
            }
            x->data.integer = pow(x->data.integer, y->data.integer);
        }
        if (strcmp(op, "min") == 0) {
            if (y->data.integer < x->data.integer) { x->data.integer = y->data.integer; }
        }
        if (strcmp(op, "max") == 0) {
            if (y->data.integer > x->data.integer) { x->data.integer = y->data.integer; }
        }
        

        lval_del(y);
    }
    
    lval_del(a);
    return x;
}


/* Evaluates basic math operations on decimals */
lval* builtin_op_d(lenv* e, lval* a, char* op) {
    for (int i = 0; i < a->count; i++) {
        LASSERT_TYPE("builtin_op_d", a, i, LVAL_DEC)
    }   

    // pop the first value into a temporary variable  
    lval* x = lval_pop(a, 0);

    // put the value into another variable? 
    if ((strcmp(op, "-") == 0) && a->count == 0) {
        x->data.decimal = -x->data.decimal;
    }
    
    while (a->count > 0) {  
        lval* y = lval_pop(a, 0);
        
        if (strcmp(op, "+") == 0) { x->data.decimal += y->data.decimal; }
        if (strcmp(op, "-") == 0) { x->data.decimal -= y->data.decimal; }
        if (strcmp(op, "*") == 0) { x->data.decimal *= y->data.decimal; }
        if (strcmp(op, "/") == 0) {
            if (y->data.decimal == 0) {
                lval_del(x); lval_del(y);
                x = lval_err("Division By Zero.");
                break;
            }
            x->data.decimal /= y->data.decimal;
        }
        if (strcmp(op, "%") == 0) {
            lval_del(x); lval_del(y);
            x = lval_err("Mod is invalid operation on decimal.");
            break;
        }   
        if (strcmp(op, "^") == 0) { 
            if (y->data.decimal < 0) {
                lval_del(x); lval_del(y);
                x = lval_err("Unable to raise to negative power.");
                break;
            }
            x->data.decimal = pow(x->data.decimal, y->data.decimal);
        }
        if (strcmp(op, "min") == 0) {
            if (y->data.decimal < x->data.decimal) { x->data.decimal = y->data.decimal; }
        }
        if (strcmp(op, "max") == 0) {
            if (y->data.decimal > x->data.decimal) { x->data.decimal = y->data.decimal; }
        }
        

        lval_del(y);
    }
    
    lval_del(a);
    return x;
}


/* Evaluates basic math operations by converting (if necessary) 
 * and calling integer or decimal functions */
lval* builtin_op(lenv* e, lval* a, char* op) {
    // check that all members of expression are integer or decimal
    bool i = false, d = false;
    for (int j = 0; j < a->count; j++) {
        if (a->data.cell[j]->type == LVAL_INT) {
            i = true;
        }
        else if (a->data.cell[j]->type == LVAL_DEC) {
            d = true;
        }
        else {
            lval *err = lval_err("Type error: expected Integer or Decimal, got %s.\n", ltype_name(a->data.cell[j]->type));
            lval_del(a);
            return err; 
        }
    }
    
    if (i && !d) {
        return builtin_op_i(e, a, op);
    }
    
    // if sexpr contains both integers and decimals,
    // convert all integers and call lval_eval_d
    else if (i && d) {
        // create a new sexpr
        lval *s = lval_sexpr();
        
        // pop each value in a
        do {
           lval *v = lval_pop(a, 0);
           // if it is an integer, create a new lval_dec and cast the value
           if (v->type == LVAL_INT) {
               lval* temp = lval_dec((float) v->data.integer);
               lval_del(v);
               v = temp;
           }
           lval_add(s, v);
        } while (a->count > 0);

        lval_del(a);
        return builtin_op_d(e, s, op);
    }
    else {
        return builtin_op_d(e, a, op);
    }    
}


lval* builtin_add(lenv* e, lval* a) {
    return builtin_op(e, a, "+");
}

lval* builtin_sub(lenv* e, lval* a) {
    return builtin_op(e, a, "-");
}

lval* builtin_mul(lenv* e, lval* a) {
    return builtin_op(e, a, "*");
}

lval* builtin_div(lenv* e, lval* a) {
    return builtin_op(e, a, "/");
}

lval* builtin_mod(lenv* e, lval* a) {
    return builtin_op(e, a, "%");
}

lval* builtin_min(lenv* e, lval* a) {
    return builtin_op(e, a, "min");
}

lval* builtin_max(lenv* e, lval* a) {
    return builtin_op(e, a, "max");
}

lval* builtin_pow(lenv* e, lval* a) {
    return builtin_op(e, a, "^");
}

lval* builtin_def(lenv* e, lval* a) {

    LASSERT_TYPE("def", a, 0, LVAL_QEXPR);
    
    /* First argument is symbol list */
    lval* syms = a->data.cell[0];
    
    /* Ensure all elements of first list are symbols */
    for (int i = 0; i < syms->count; i++) {
        LASSERT(a, (syms->data.cell[i]->type == LVAL_SYM),
            "Function 'def' cannot define non-symbol. "
            "Got %s, Expected %s.",
            ltype_name(syms->data.cell[i]->type), ltype_name(LVAL_SYM));
    }
    
    /* Check correct number of symbols and values */
    LASSERT(a, (syms->count == a->count-1),
        "Function 'def' passed too many arguments for symbols. "
        "Got %i, Expected %i.",
        syms->count, a->count-1);
    
    /* Assign copies of values to symbols */
    for (int i = 0; i < syms->count; i++) {
        lenv_put(e, syms->data.cell[i], a->data.cell[i+1]);
    }
    
    lval_del(a);
    return lval_sexpr();
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
    lval* k = lval_sym(name);
    lval* v = lval_fun(func);
    lenv_put(e, k, v);
    lval_del(k); lval_del(v);
}

void lenv_add_builtins(lenv* e) {
    /* Variable Functions */
    lenv_add_builtin(e, "def", builtin_def);
    
    /* List Functions */
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);
    lenv_add_builtin(e, "cons", builtin_cons);
    lenv_add_builtin(e, "init", builtin_init);
    lenv_add_builtin(e, "len", builtin_len); 

    /* Mathematical Functions */
    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "add", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "sub", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "mul", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);
    lenv_add_builtin(e, "div", builtin_div);
    lenv_add_builtin(e, "%", builtin_mod);
    lenv_add_builtin(e, "mod", builtin_mod);
    lenv_add_builtin(e, "^", builtin_pow);
    lenv_add_builtin(e, "pow", builtin_pow);
    lenv_add_builtin(e, "min", builtin_min);
    lenv_add_builtin(e, "max", builtin_max);
}

/* Evaluation */

lval* lval_eval_sexpr(lenv* e, lval* v) {
    
    for (int i = 0; i < v->count; i++) {
        v->data.cell[i] = lval_eval(e, v->data.cell[i]);
    }
    
    for (int i = 0; i < v->count; i++) {
        if (v->data.cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
    }
    
    if (v->count == 0) { return v; }  
    if (v->count == 1) { return lval_take(v, 0); }
    
    /* Ensure first element is a function after evaluation */
    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_FUN) {
        lval* err = lval_err(
            "S-Expression starts with incorrect type. "
            "Got %s, Expected %s.",
            ltype_name(f->type), ltype_name(LVAL_FUN));
        lval_del(f); lval_del(v);
        return err;
    }
    
    /* If so call function to get result */
    lval* result = f->data.fun(e, v);
    lval_del(f);
    return result;
}

lval* lval_eval(lenv* e, lval* v) {
    if (v->type == LVAL_SYM) {
        lval* x = lenv_get(e, v);
        lval_del(v);
        return x;
    }
    if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }
    return v;
}

/* Reading */

lval* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    if (strchr(t->contents, '.') != NULL) {
        double d = strtof(t->contents, NULL);
        return errno != ERANGE ? lval_dec(d) : lval_err("Invalid number.");
    }
    else {
        long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE ? lval_int(x) : lval_err("Invalid Number.");
    }
    printf("\n");
}

lval* lval_read(mpc_ast_t* t) {
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }
    
    lval* x = NULL;
    if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); } 
    if (strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }
    if (strstr(t->tag, "qexpr"))  { x = lval_qexpr(); }
    
    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
        if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
        x = lval_add(x, lval_read(t->children[i]));
    }
    
    return x;
}

/* Main */

int main(int argc, char** argv) {
    
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr  = mpc_new("sexpr");
    mpc_parser_t* Qexpr  = mpc_new("qexpr");
    mpc_parser_t* Expr   = mpc_new("expr");
    mpc_parser_t* Lispy  = mpc_new("lispy");
    
    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                       \
            number : /-?(([0-9]*[.])?[0-9]+([.][0-9]*)?)/ ;     \
            symbol : /[a-zA-Z0-9_+\\-*\\/\%^\\\\=<>!&]+/ ;      \
            sexpr  : '(' <expr>* ')' ;                          \
            qexpr  : '{' <expr>* '}' ;                          \
            expr   : <number> | <symbol> | <sexpr> | <qexpr> ;  \
            lispy  : /^/ <expr>* /$/ ;                          \
        ",
        Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
    
    puts("Lispy Version 0.0.0.0.7");
    puts("Press Ctrl+c to Exit\n");
    
    lenv* e = lenv_new();
    lenv_add_builtins(e);
    
    while (1) {
    
        char* input = readline("lispy> ");
        add_history(input);
        
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            lval* x = lval_eval(e, lval_read(r.output));
            lval_println(x);
            lval_del(x);
            mpc_ast_delete(r.output);
        } else {    
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        
        free(input);
        
    }
    
    lenv_del(e);
    
    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
    
    return 0;
}
