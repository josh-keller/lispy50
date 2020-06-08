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

enum { LVAL_ERR, LVAL_INT, LVAL_DEC, LVAL_BOOL, LVAL_SYM, 
              LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR };

typedef lval*(*lbuiltin)(lenv*, lval*);

struct lval {
    int type;

    // Number, Symbol, and Error data
    union {
        long integer;
        double decimal;
        bool boolean;
        char* sym;
        char* err;
    } data;

    //Functions
    lbuiltin builtin;
    lenv* env;
    lval* formals;
    lval* body;

    // Expressions
    int count;
    lval** cell;
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

lval* lval_bool(bool boolean) {
    lval* b = malloc(sizeof(lval));
    b->type = LVAL_BOOL;
    b->data.boolean = boolean;
    return b;
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

lenv* lenv_new();

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

void lenv_del(lenv* e);

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

lenv* lenv_copy(lenv* e);

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

void lval_print(lval* v);

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

char* func_name(lval* func); 

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
        case LVAL_SEXPR: return "S-Expression";
        case LVAL_QEXPR: return "Q-Expression";
        default: return "Unknown";
    }
}

/* Lisp Environment */

struct lenv {
    lenv* par;
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
    e->par = NULL;
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

lval* builtin_env(lenv* e, lval* a);

/* Returns the value for a symbol in the environment */
lval* lenv_get(lenv* e, lval* k) {
    
    //Check if symbol is "env", in which case call the function
    if (strcmp(k->data.sym, "env") == 0) {
        return builtin_env(e, k);
    }

    /* Iterate over all items in environment */
    for (int i = 0; i < e->count; i++) {
        /* Check if the stored string matches the symbol string */
        /* If it does, return a copy of the value */
        if (strcmp(e->syms[i], k->data.sym) == 0) {
            return lval_copy(e->vals[i]);
        }
    }
    /* If no symbol found, search parent or return error */
    if (e->par) {
        return lenv_get(e->par, k);
    }
    else {
        return lval_err("Unbound Symbol '%s'", k->data.sym);
    }
}

/* Copies an environment */
lenv* lenv_copy(lenv* e) {
    lenv* n = malloc(sizeof(lenv));
    n->par = e->par;
    n->count = e->count;
    n->syms = malloc(sizeof(char*) * n->count);
    n->vals = malloc(sizeof(lval*) * n->count);
    for (int i = 0; i < n->count; i++) {
       n->syms[i] = malloc(strlen(e->syms[i]) + 1);
       strcpy(n->syms[i], e->syms[i]);
       n->vals[i] = lval_copy(e->vals[i]);
    }
    return n;
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
/* Global variable definition */
void lenv_def(lenv* e, lval* k, lval* v) {
    // Iterate until no parent
    while (e->par) {
        e = e->par;
    }

    lenv_put(e, k, v);
}
 

/* Builtins */

#define LASSERT(args, cond, fmt, ...) \
    if (!(cond)) { lval* err = lval_err(fmt, ##__VA_ARGS__); lval_del(args); return err; }

#define LASSERT_TYPE(func, args, index, expect) \
    LASSERT(args, args->cell[index]->type == expect, \
        "Function '%s' passed incorrect type for argument %i. Got %s, Expected %s.", \
        func, index, ltype_name(args->cell[index]->type), ltype_name(expect))

#define LASSERT_NUM(func, args, num) \
    LASSERT(args, args->count == num, \
        "Function '%s' passed incorrect number of arguments. Got %i, Expected %i.", \
        func, args->count, num)

#define LASSERT_NOT_EMPTY(func, args, index) \
    LASSERT(args, args->cell[index]->count != 0, \
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

lval* lval_call(lenv* e, lval* f, lval* a) {
    // If Builtin, then call that function
    if (f->builtin) { 
        return f->builtin(e, a); 
    }

    // Record argument counts
    int given = a->count;
    int total = f->formals->count;

    // While arguments still remain to be processed
    while (a->count) {

        // If we've run out of formal arguments to bind
        if (f->formals->count == 0) {
            lval_del(a);
            return lval_err ("Function passed too many arguments. \
                Got %i, expected %i.", given, total);
        }

        // Pop the first symbol from the formals
        lval* sym = lval_pop(f->formals, 0);

        // Special case to deal with '&' symbol
        if (strcmp(sym->data.sym, "&") == 0) {

            // Ensure '&' is followed by another symbol
            if (f->formals->count != 1) {
                lval_del(a);
                return lval_err("Function format invalid."
                    "Symbol '&' not followed by single symbol.");
            }

            // Next formal should be bound to remaining arguments
            lval* nsym = lval_pop(f->formals, 0);
            lenv_put(f->env, nsym, builtin_list(e, a));
            lval_del(sym);
            lval_del(nsym);
            break;
        }

        //Pop the next argument from the list
        lval* val = lval_pop(a, 0);

        //Bind a copy into the function's environment
        lenv_put(f->env, sym, val);

        // Delete symbol and value
        lval_del(sym);
        lval_del(val);
    }

    // Argument list is now bound so can be cleaned up 
    lval_del(a);

    // If '&' remains in formal list bind to empty list
    if (f->formals->count > 0 &&
            strcmp(f->formals->cell[0]->data.sym, "&") == 0) {

        // Check to ensure that & is not pass invalidly
        if (f->formals->count != 2) {
            return lval_err("Function format invalid. "
                    "Symbol '&' not followed by single symbol.");
        }

        // Pop and delete the '&' symbol
        lval_del(lval_pop(f->formals, 0));

        // Pop the next symbol and create empty list
        lval* sym = lval_pop(f->formals, 0);
        lval* val = lval_qexpr();

        // Bind to environment and delete
        lenv_put(f->env, sym, val);
        lval_del(sym);
        lval_del(val);
    }
    // If all formals have been bound, evaluate
    if (f->formals->count == 0) {
        // Set environment parent to the evaluation environment
        f->env->par = e;

        // Evaluate and return
        return builtin_eval(f->env, lval_add(lval_sexpr(),
            lval_copy(f->body)));
    }
    else {
        return lval_copy(f);
    }

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
    LASSERT_TYPE("len", a, 0, LVAL_QEXPR);
    lval* x = lval_int(a->cell[0]->count);
    lval_del(a);
    return x;
}

/* Returns all of a Q-Expr except the final element */
lval* builtin_init(lenv* e, lval* a) {
    LASSERT_TYPE("init", a, 0, LVAL_QEXPR);
    LASSERT_NUM("init", a, 1);

    lval* x = lval_copy(a->cell[0]);

    lval* y = lval_pop(x, x->count - 1);
    lval_del(y);
    lval_del(a);
    return x;
}

/* Takes a value and Q-Expr and appends value to the front */
lval* builtin_cons(lenv* e, lval* a) {
    LASSERT_NUM("cons", a, 2);
    LASSERT_TYPE("cons", a, 1, LVAL_QEXPR);

    lval* q = lval_qexpr();
    lval_add(q, lval_pop(a, 0));

    q = lval_join(q, lval_take(a, 0));

    return q;
}

/* Prints out all the named values in an environment */
lval* builtin_env(lenv* e, lval* a) {
    lval* x = lval_qexpr();
    lval* y;

    for (int i = 0; i < e->count; i++) {
        y = lval_qexpr();
        lval_add(y, lval_sym(e->syms[i]));
        lval_add(y, lval_copy(e->vals[i]));
        lval_add(x, y);
    }

    return x;
}
        
lval* builtin_lambda(lenv* e, lval* a) {
    // Check that there are 2 arguments, both Q-Expr 
    LASSERT_NUM("\\", a, 2);
    LASSERT_TYPE("\\", a, 0, LVAL_QEXPR);
    LASSERT_TYPE("\\", a, 1, LVAL_QEXPR);
        
    // Check first Q-Expr contains only symbols */
    for (int i = 0; i < a->cell[0]->count; i++) {
        LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
                "Cannot define non-symbol. Got %s, Expected %s.",
                ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
    }

    //Pop first two arguments and pass them to lval_lambda */
    lval* formals = lval_pop(a, 0);
    lval* body = lval_pop(a, 0);
    lval_del(a);

    return lval_lambda(formals, body);
}


lval* builtin_exit(lenv* e, lval* a) {
    lval_del(a);
    return lval_sym("exit");
}

/* Evaluates basic math operations on integers */
lval* builtin_op_i(lenv* e, lval* a, char* op) {
    for (int i = 0; i < a->count; i++) {
        LASSERT_TYPE("builtin_op_i", a, i, LVAL_INT);
    }   

    // pop the first value into a temporary variable  
    lval* x = lval_pop(a, 0);

    // put the value into another variable? 
    if ((strcmp(op, "-") == 0) && a->count == 0) {
        x->data.integer = -x->data.integer;
    }
    
    while (a->count > 0) {  
        lval* y = lval_pop(a, 0);
        
        if (strcmp(op, "+") == 0) { x->data.integer += y->data.integer; continue; }
        if (strcmp(op, "-") == 0) { x->data.integer -= y->data.integer; continue; }
        if (strcmp(op, "*") == 0) { x->data.integer *= y->data.integer; continue; }
        if (strcmp(op, "/") == 0) {
            if (y->data.integer == 0) {
                lval_del(x); lval_del(y);
                x = lval_err("Division By Zero.");
                break;
            }
            x->data.integer /= y->data.integer;
            continue; 
        }
        if (strcmp(op, "%") == 0) {
            if (y->data.integer == 0) {
                lval_del(x); lval_del(y);
                x = lval_err("Division By Zero.");
                break;
            }
            x->data.integer %= y->data.integer;
            continue; 
        }   
        if (strcmp(op, "^") == 0) { 
            if (y->data.integer < 0) {
                lval_del(x); lval_del(y);
                x = lval_err("Unable to raise to negative power.");
                break;
            }
            x->data.integer = pow(x->data.integer, y->data.integer);
            continue; 
        }
        if (strcmp(op, "min") == 0) {
            if (y->data.integer < x->data.integer) { x->data.integer = y->data.integer; continue; }
        }
        if (strcmp(op, "max") == 0) {
            if (y->data.integer > x->data.integer) { x->data.integer = y->data.integer; continue; }
        }
        

        lval_del(y);
    }
    
    lval_del(a);
    return x;
}


/* Evaluates basic math operations on decimals */
lval* builtin_op_d(lenv* e, lval* a, char* op) {
    for (int i = 0; i < a->count; i++) {
        LASSERT_TYPE("builtin_op_d", a, i, LVAL_DEC);
    }   

    // pop the first value into a temporary variable  
    lval* x = lval_pop(a, 0);

    // put the value into another variable? 
    if ((strcmp(op, "-") == 0) && a->count == 0) {
        x->data.decimal = -x->data.decimal;
    }
    
    while (a->count > 0) {  
        lval* y = lval_pop(a, 0);
        
        if (strcmp(op, "+") == 0) { x->data.decimal += y->data.decimal; continue; }
        if (strcmp(op, "-") == 0) { x->data.decimal -= y->data.decimal; continue; }
        if (strcmp(op, "*") == 0) { x->data.decimal *= y->data.decimal; continue; }
        if (strcmp(op, "/") == 0) {
            if (y->data.decimal == 0) {
                lval_del(x); lval_del(y);
                x = lval_err("Division By Zero.");
                break;
            }
            x->data.decimal /= y->data.decimal;
            continue; 
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
            continue;            
        }
        if (strcmp(op, "min") == 0) {
            if (y->data.decimal < x->data.decimal) {
                x->data.decimal = y->data.decimal;
                continue;
            }
        }
        if (strcmp(op, "max") == 0) {
            if (y->data.decimal > x->data.decimal) { 
                x->data.decimal = y->data.decimal; 
                continue; 
            }
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
        if (a->cell[j]->type == LVAL_INT) {
            i = true;
        }
        else if (a->cell[j]->type == LVAL_DEC) {
            d = true;
        }
        else {
            lval *err = lval_err("Type error: expected Integer or Decimal, got %s.\n", ltype_name(a->cell[j]->type));
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

lval* builtin_cond_i(lenv* e, lval* v1, lval* v2, char* op) {
    
    lval* x = lval_bool(false);
    
    if (strcmp(op, "<") == 0) {
        x->data.boolean = v1->data.integer < v2->data.integer;
    }
    else if (strcmp(op, ">") == 0) {
        x->data.boolean = v1->data.integer > v2->data.integer;
    }
    else if (strcmp(op, ">=") == 0) {
        x->data.boolean = v1->data.integer >= v2->data.integer;
    }
    else if (strcmp(op, "<=") == 0) {
        x->data.boolean = v1->data.integer <= v2->data.integer;
    }
    else if (strcmp(op, "!=") == 0) {
        x->data.boolean = v1->data.integer != v2->data.integer;
    }

    lval_del(v1); lval_del(v2);
    return x;
}

lval* builtin_cond(lenv* e, lval* a, char* op) {
    
    // check that there are two values ****
    LASSERT_NUM(op, a, 2);

    lval* v1 = lval_pop(a, 0);
    lval* v2 = lval_take(a, 0);
    
    if (v1->type == LVAL_INT && v2->type == LVAL_INT) {
        return builtin_cond_i(e, v1, v2, op);
    }
    else {
        lval* e = lval_err("Incorrect types (%s, %s) passed to %s.",
                v1->type, v2->type, op);
        lval_del(v1); lval_del(v2);
        return e;
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

lval* builtin_lessthan(lenv* e, lval* a) {
    return builtin_cond(e, a, "<");
}

lval* builtin_greaterthan(lenv* e, lval* a) {
    return builtin_cond(e, a, ">");
}

bool lval_eq(lval* x, lval* y) {

    // If two lvals are not the same type, return false.
    if (x->type != y->type) {
        return false;
    }
    
    switch (x->type) {
        case LVAL_INT: return (x->data.integer == y->data.integer);
        case LVAL_DEC: return (x->data.decimal == y->data.decimal);
        case LVAL_BOOL: return (x->data.boolean == y->data.boolean);
        case LVAL_ERR: return (strcmp(x->data.err, y->data.err) == 0);
        case LVAL_SYM: return (strcmp(x->data.sym, y->data.sym) == 0);
        case LVAL_FUN: 
                   if (x->builtin || y-> builtin) {
                       return x->builtin == y->builtin;
                   } 
                   else {
                       return lval_eq(x->formals, y->formals) &&
                          lval_eq(x->body, y->body); 
                   }
        case LVAL_QEXPR:
        case LVAL_SEXPR:
            if (x->count != y->count) { return false; }
            
            for (int i = 0; i < x->count; i++) {
                if (!lval_eq(x->cell[i], y->cell[i])) {
                    return false;
                }
            }
            return true;
        default: return false;
    }
}

lval* builtin_comp(lenv* e, lval* a, char* op) {
    LASSERT_NUM("==", a, 2);
    lval* x = lval_pop(a, 0);
    lval* y = lval_pop(a, 0);
    bool b = lval_eq(x, y);
    lval_del(a); lval_del(x); lval_del(y);
    if (strcmp(op, "==") == 0) { return lval_bool(b); }
    if (strcmp(op, "!=") == 0) { return lval_bool(!b); }
    else { return lval_err("Incorrect operator passed to builtin_comp."); }
}

lval* builtin_equal(lenv* e, lval* a) {
    return builtin_comp(e, a, "==");
}

lval* builtin_notequal(lenv* e, lval* a) {
    return builtin_comp(e, a, "!=");
}

lval* builtin_lessorequal(lenv* e, lval* a) {
    return builtin_cond(e, a, "<=");
}

lval* builtin_greaterorequal(lenv* e, lval* a) {
    return builtin_cond(e, a, ">=");
}

lval* builtin_if(lenv* e, lval* a) {
    LASSERT_NUM("if", a, 3);
    LASSERT_TYPE("if", a, 0, LVAL_BOOL);
    LASSERT_TYPE("if", a, 1, LVAL_QEXPR);
    LASSERT_TYPE("if", a, 2, LVAL_QEXPR);

    lval* b = lval_pop(a, 0);

    if (b->data.boolean) {
        // Take first expression and evaluate it
        lval* s = lval_take(a, 0);
        s->type = LVAL_SEXPR;
        return lval_eval(e, s);
    }
    else {
        // Take the second expression and evaluate it
        lval* s = lval_take(a, 1);
        s->type = LVAL_SEXPR;
        return lval_eval(e, s);
    }
}

lval* builtin_xor(lenv* e, lval* a, char* op) {
    LASSERT_NUM("or", a, 2);
    LASSERT_TYPE("or", a, 0, LVAL_BOOL);
    LASSERT_TYPE("or", a, 1, LVAL_BOOL);

    lval* x = lval_pop(a, 0);
    lval* y = lval_take(a, 0);
    lval* b;

    if (strcmp(op, "||") == 0) {
        b = lval_bool(x->data.boolean || y->data.boolean);
    }
    else if (strcmp(op, "&&") == 0) {
        b = lval_bool(x->data.boolean && y->data.boolean);
    }
    lval_del(x); lval_del(y);
    return b;
}

lval* builtin_or(lenv* e, lval* a) {
    return builtin_xor(e, a, "||");
}

lval* builtin_and(lenv* e, lval* a) {
    return builtin_xor(e, a, "&&");
}

lval* builtin_not(lenv* e, lval* a) {
    LASSERT_NUM("not", a, 1);
    LASSERT_TYPE("not", a, 0, LVAL_BOOL);

    if (a->cell[0]->data.boolean) {
        lval_del(a);
        return lval_bool(0);
    }
    else {
        lval_del(a);
        return lval_bool(1);
    }
}

bool is_builtin(lenv* e, lval* a) {
    LASSERT(a, a->type == LVAL_SYM, "Function 'is_builtin' passed a non-function argument.");
    // Iterate to the first parent environment
    while (e->par) {
        e = e->par;
    }
    // Find the symbol in the environment and check if builtin == NULL
    for (int i = 0; i < e->count; i++) {
        if (strcmp(a->data.sym, e->syms[i]) == 0) {
            if (e->vals[i]->type == LVAL_FUN && e->vals[i]->builtin != NULL) {
                return true; // Symbol found and builtin was not NULL
            }
            else { return false; } // Symbol found and builtin NULL
        }
    }

    return false; // Symbol not found
}

lval* builtin_var(lenv* e, lval* a, char* func) {
    LASSERT_TYPE("def", a, 0, LVAL_QEXPR);
    
    /* First argument is symbol list */
    lval* syms = a->cell[0];
    
    /* Ensure all elements of first list are symbols */
    for (int i = 0; i < syms->count; i++) {
        LASSERT(a, (syms->cell[i]->type == LVAL_SYM),
            "Function '%s' cannot define non-symbol. "
            "Got %s, Expected %s.", func,
            ltype_name(syms->cell[i]->type), ltype_name(LVAL_SYM));
        if (is_builtin(e, syms->cell[i])) {
            char name[strlen(syms->cell[i]->data.sym) + 1];
            strcpy(name, syms->cell[i]->data.sym);
            lval_del(a);
            return lval_err("Invalid attempt to redefine builtin function %s.\n", name);
        }
    }
    
    /* Check correct number of symbols and values */
    LASSERT(a, (syms->count == a->count-1),
        "Function '%s' passed too many arguments for symbols. "
        "Got %i, Expected %i.", func, syms->count, a->count-1);
    
    // If 'def' define in global env. If 'put' define locally.
    for (int i = 0; i < syms->count; i++) {
        if (strcmp(func, "def") == 0) {
            lenv_def(e, syms->cell[i], a->cell[i+1]);
        }

        if (strcmp(func, "=") == 0) {
            lenv_put(e, syms->cell[i], a->cell[i+1]);
        }
    }
    
    lval_del(a);
    return lval_sexpr();
}

lval* builtin_def(lenv* e, lval* a) {
    return builtin_var(e, a, "def");
}

lval* builtin_put(lenv* e, lval* a) {
    return builtin_var(e, a, "=");
}


lval* builtin_fun(lenv* e, lval* a) {
    LASSERT_NUM("fun", a, 2);
    LASSERT_TYPE("fun", a, 0, LVAL_QEXPR);
    LASSERT_TYPE("fun", a, 1, LVAL_QEXPR);

    // Pop name off the first qexpr
    lval* name = lval_qexpr();
    name = lval_add(name, lval_pop(a->cell[0], 0));
    
    // First element of l_args (formals) is rest of first qexpr
    lval* l_args = lval_qexpr();
    l_args = lval_add(l_args, lval_pop(a, 0));
    // Add body of function to l_args
    l_args = lval_add(l_args, lval_take(a, 0));

    // Create lambda function
    lval* lambda = builtin_lambda(e, l_args);
    // Add name to lambda to pass to def
    lval* x = lval_qexpr();
    x = lval_add(x, name);
    x = lval_add(x, lambda);

    return builtin_def(e, x);
}


char* func_name(lval* func) {
        if (func->builtin == builtin_add) return "add";
        else if (func->builtin == builtin_sub) return "sub";
        else if (func->builtin == builtin_mul) return "mul";
        else if (func->builtin == builtin_div) return "div";
        else if (func->builtin == builtin_mod) return "mod";
        else if (func->builtin == builtin_pow) return "pow";
        else if (func->builtin == builtin_min) return "min";
        else if (func->builtin == builtin_max) return "max";
        else if (func->builtin == builtin_list) return "list";
        else if (func->builtin == builtin_head) return "head";
        else if (func->builtin == builtin_tail) return "tail";
        else if (func->builtin == builtin_eval) return "eval";
        else if (func->builtin == builtin_join) return "join";
        else if (func->builtin == builtin_cons) return "cons";
        else if (func->builtin == builtin_init) return "init";
        else if (func->builtin == builtin_len) return "len";
        else if (func->builtin == builtin_def) return "def";
        else if (func->builtin == builtin_env) return "env";
        else if (func->builtin == builtin_exit) return "exit";
        else if (func->builtin == builtin_lambda) return "lambda";
        else if (func->builtin == builtin_put) return "=";
        else if (func->builtin == builtin_lessthan) return "<";
        else if (func->builtin == builtin_greaterthan) return ">";
        else if (func->builtin == builtin_equal) return "==";
        else if (func->builtin == builtin_notequal) return "!=";
        else if (func->builtin == builtin_lessorequal) return "<=";
        else if (func->builtin == builtin_greaterorequal) return ">=";
        else if (func->builtin == builtin_if) return "if";
        else if (func->builtin == builtin_not) return "not";
        else if (func->builtin == builtin_or) return "or";
        else if (func->builtin == builtin_and) return "and";
        else if (func->builtin == builtin_fun) return "fun";
        else return "<function>";
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
    lval* k = lval_sym(name);
    lval* v = lval_fun(func);
    lenv_put(e, k, v);
    lval_del(k); lval_del(v);
}

void lenv_add_builtins(lenv* e) {

    lenv_add_builtin(e, "exit", builtin_exit);

    /* Variable and Lambda Functions */
    lenv_add_builtin(e, "def", builtin_def);
    lenv_add_builtin(e, "=", builtin_put);
    lenv_add_builtin(e, "env", builtin_env);
    lenv_add_builtin(e, "\\", builtin_lambda);
    lenv_add_builtin(e, "fun", builtin_fun);
    
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

    /* Conditional and Ordering Functions */
    lenv_add_builtin(e, "<", builtin_lessthan);
    lenv_add_builtin(e, ">", builtin_greaterthan);
    lenv_add_builtin(e, "==", builtin_equal);
    lenv_add_builtin(e, "!=", builtin_notequal);
    lenv_add_builtin(e, "<=", builtin_lessorequal);
    lenv_add_builtin(e, ">=", builtin_greaterorequal);
    lenv_add_builtin(e, "if", builtin_if);
    lenv_add_builtin(e, "not", builtin_not);
    lenv_add_builtin(e, "!", builtin_not);
    lenv_add_builtin(e, "||", builtin_or);
    lenv_add_builtin(e, "&&", builtin_and);
}

/* Evaluation */

lval* lval_eval_sexpr(lenv* e, lval* v) {
    
    // Evaluate each cell in the sexpr 
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]);
    }
    
    // If there are any errors after evaluation, return that error
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
    }
    
    // Return empty expression or single lval expression directly
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
    
    lval* result = lval_call(e, f, v);
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
    if (strstr(t->tag, "symbol")) { 
        if (strcmp(t->contents, "true") == 0) { return lval_bool(true); }
        else if (strcmp(t->contents, "false") == 0) { return lval_bool(false); }
        else { return lval_sym(t->contents); }
    }
    
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
            symbol : /[a-zA-Z0-9_+\\-*\\/\%^\\\\=<>!&|]+/ ;     \
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

    bool quit = false;

    while (!quit) {
    
        char* input = readline("lispy> ");
        add_history(input);
        
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            lval* x = lval_eval(e, lval_read(r.output));
            lval_println(x);
            if (x->type == LVAL_FUN && x->builtin == builtin_exit) {
                quit = true;
            }
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
