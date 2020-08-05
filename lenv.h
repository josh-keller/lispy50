#ifndef lenv_h
#define lenv_h

#include "lval.h"

struct lenv {
    lenv* par;
    int count;
    char** syms;
    lval** vals;
};

void lenv_put(lenv* e, lval* k, lval* v);
void lenv_def(lenv* e, lval* k, lval* v);
lval* lenv_get(lenv* e, lval* k);

#endif

