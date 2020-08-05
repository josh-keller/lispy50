#include "lenv.h"
#include "lval.h"

/***************************************************
 *  Lisp Environment 
 ***************************************************/

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
    /* Check if symbol is "env", in which case call the function */
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
 * if the symbol exists, changes the value */
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
    /* Iterate until no parent */
    while (e->par) {
        e = e->par;
    }
    lenv_put(e, k, v);
}
