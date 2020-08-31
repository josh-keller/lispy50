#include "builtins.h"

lval* lval_eval(lenv* e, lval* v);

/* Takes arguments and returns a Q-Expression containing those arguments */
lval* builtin_list(lenv* e, lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

/* Returns the first element of a Q-Expression */
lval* builtin_head(lenv* e, lval* a) {
    LASSERT_NUM("head", a, 1);
    LASSERT(a, ((a->cell[0]->type == LVAL_QEXPR) || (a->cell[0]->type == LVAL_STR)),
            "Incorrect type passed to head. Expected Q-Expr or String, got %s.",
            ltype_name(a->cell[0]->type));

    lval* v;

    if (a->cell[0]->type == LVAL_STR) {
        char* s = malloc(sizeof(char) * 2);
        s[0] = a->cell[0]->data.str[0];
        s[1] = '\0';
        v = lval_str(s);
        lval_del(a);
        free(s);
    }
    else {
        LASSERT_NOT_EMPTY("head", a, 0);
        v = lval_take(a, 0);  
        while (v->count > 1) { lval_del(lval_pop(v, 1)); }
    }

    return v;
}

/* Returns all but the first element of a Q-Expression */
lval* builtin_tail(lenv* e, lval* a) {
    LASSERT_NUM("tail", a, 1);
    LASSERT(a, ((a->cell[0]->type == LVAL_QEXPR) || (a->cell[0]->type == LVAL_STR)),
            "Incorrect type passed to head. Expected Q-Expr or String, got %s.",
            ltype_name(a->cell[0]->type));

    lval* v;
    if (a->cell[0]->type == LVAL_STR) {
        char* s = malloc(strlen(a->cell[0]->data.str));
        int i = 0;
        while (a->cell[0]->data.str[i + 1] != '\0') {
            s[i] = a->cell[0]->data.str[i + 1];
            i++;
        }
        s[i] = '\0';
        v = lval_str(s);
        lval_del(a);
        free(s);
    }
    else {
        LASSERT_NOT_EMPTY("tail", a, 0);
        v = lval_take(a, 0);  
        lval_del(lval_pop(v, 0));
    }
    
    return v;
}

/* Reads in and converts a string to a Q-Expr */
lval* builtin_read(lenv* e, lval* a) {
    LASSERT_NUM("read", a, 1);
    LASSERT_TYPE("read", a, 0, LVAL_STR);

    lval* q = lval_qexpr();
    lval* s = lval_take(a, 0);
    
    lval_add(q, lval_sym(s->data.str));
    lval_del(s);
    return q;
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
    /* If Builtin, then call that function */
    if (f->builtin) { 
        return f->builtin(e, a); 
    }

    /* Record argument counts */
    int given = a->count;
    int total = f->formals->count;

    /* While arguments still remain to be processed */
    while (a->count) {

        /* If we've run out of formal arguments to bind */
        if (f->formals->count == 0) {
            lval_del(a);
            return lval_err ("Function passed too many arguments. \
                Got %i, expected %i.", given, total);
        }

        /* Pop the first symbol from the formals */
        lval* sym = lval_pop(f->formals, 0);

        /* Special case to deal with '&' symbol */
        if (strcmp(sym->data.sym, "&") == 0) {

            /* Ensure '&' is followed by another symbol */
            if (f->formals->count != 1) {
                lval_del(a);
                return lval_err("Function format invalid."
                    "Symbol '&' not followed by single symbol.");
            }

            /* Next formal should be bound to remaining arguments */
            lval* nsym = lval_pop(f->formals, 0);
            lenv_put(f->env, nsym, builtin_list(e, a));
            lval_del(sym);
            lval_del(nsym);
            break;
        }

        /* Pop the next argument from the list */
        lval* val = lval_pop(a, 0);

        /* Bind a copy into the function's environment */
        lenv_put(f->env, sym, val);

        /* Delete symbol and value */
        lval_del(sym);
        lval_del(val);
    }

    /* Argument list is now bound so can be cleaned up  */
    lval_del(a);

    /* If '&' remains in formal list bind to empty list */
    if (f->formals->count > 0 &&
            strcmp(f->formals->cell[0]->data.sym, "&") == 0) {

        /* Check to ensure that & is not pass invalidly */
        if (f->formals->count != 2) {
            return lval_err("Function format invalid. "
                    "Symbol '&' not followed by single symbol.");
        }

        /* Pop and delete the '&' symbol */
        lval_del(lval_pop(f->formals, 0));

        /* Pop the next symbol and create empty list */
        lval* sym = lval_pop(f->formals, 0);
        lval* val = lval_qexpr();

        /* Bind to environment and delete */
        lenv_put(f->env, sym, val);
        lval_del(sym);
        lval_del(val);
    }
    /* If all formals have been bound, evaluate */
    if (f->formals->count == 0) {
        /* Set environment parent to the evaluation environment */
        f->env->par = e;

        /* Evaluate and return */
        return builtin_eval(f->env, lval_add(lval_sexpr(),
            lval_copy(f->body)));
    }
    else {
        return lval_copy(f);
    }
}

/* Takes a string lval and concatenates the second string onto the first */
lval* join_string(lval* x, lval* y) {
    x->data.str = (char *) realloc(x->data.str, 
            strlen(x->data.str) + strlen(y->data.str) + 1);
    strcat(x->data.str, y->data.str);
    lval_del(y);
    return x;
}

/* Takes one or more Q-Expressions or strings and an lval with them joined together */
lval* builtin_join(lenv* e, lval* a) {
    /* check that all members of expression are string or q-expr*/
    bool str = false, qex = false;
    for (int i = 0; i < a->count; i++) {
        if (a->cell[i]->type == LVAL_STR) {
            str = true;
        }
        else if (a->cell[i]->type == LVAL_QEXPR) {
            qex = true;
        }
        else {
            LASSERT(a, false, "Join expected string or Q-Expr, got %s", 
                    ltype_name(a->cell[i]->type));
        }
        /* Check that args are only one type */ 
        LASSERT(a, (!(str && qex)), "Join cannot join Q-Expr with String.");
    }

    lval* x = lval_pop(a, 0);
    
    if (qex) {
        while (a->count) {
            lval* y = lval_pop(a, 0);
            x = lval_join(x, y);
        }
    }
    else {
        while (a->count) {
            lval* y = lval_pop(a, 0);
            x = join_string(x, y);
        }
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
    /* Check that there are 2 arguments, both Q-Expr  */
    LASSERT_NUM("\\", a, 2);
    LASSERT_TYPE("\\", a, 0, LVAL_QEXPR);
    LASSERT_TYPE("\\", a, 1, LVAL_QEXPR);
        
    /* Check first Q-Expr contains only symbols */ 
    for (int i = 0; i < a->cell[0]->count; i++) {
        LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
                "Cannot define non-symbol. Got %s, Expected %s.",
                ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
    }

    /* Pop first two arguments and pass them to lval_lambda */
    lval* formals = lval_pop(a, 0);
    lval* body = lval_pop(a, 0);
    lval_del(a);

    return lval_lambda(formals, body);
}

lval* builtin_exit(lenv* e, lval* a) {
    lval_del(a);
    return lval_sym("exit");
}

/* Converts all cells from int to dec (as neccessary) */
lval* int_to_dec(lval* a) {
    /* check that all members of expression are integer or decimal */
    bool i = false, d = false;
    for (int j = 0; j < a->count; j++) {
        if (a->cell[j]->type == LVAL_INT) {
            i = true;
        }
        else if (a->cell[j]->type == LVAL_DEC) {
            d = true;
        }
        else {
            lval *err = lval_err("Expected Integer or Decimal, got %s.\n", ltype_name(a->cell[j]->type));
            lval_del(a);
            return err; 
        }
    }

    /* If all values are the same type, return lval without any conversions */
    if ((i && !d) || (d && !i)) {
        return a;
    }

    lval* b = lval_sexpr();

    while (a->count) {
        lval* x = lval_pop(a, 0);
        if (x->type == LVAL_DEC) {
            lval_add(b, x);
        } else if (x->type == LVAL_INT) {
            lval* y = lval_dec((double) x->data.integer);
            lval_del(x);
            lval_add(b, y);
        } else {
            char* type = ltype_name(x->type);
            lval_del(x); lval_del(a); lval_del(b);
            return lval_err("Expected type dec or int, got %s.", type);
        }
    }
    lval_del(a);
    return b;
}

/* Evaluates basic math operations on integers */
lval* builtin_op_i(lenv* e, lval* a, char* op) {
    for (int i = 0; i < a->count; i++) {
        LASSERT_TYPE("builtin_op_i", a, i, LVAL_INT);
    }   

    /* pop the first value into a temporary variable   */
    lval* x = lval_pop(a, 0);

    /* Check for unary negation */
    if ((strcmp(op, "-") == 0) && a->count == 0) {
        x->data.integer = -x->data.integer;
    }
    
    while (a->count > 0) {  
        lval* y = lval_pop(a, 0);
        if (strcmp(op, "+") == 0) { x->data.integer += y->data.integer;}
        else if (strcmp(op, "-") == 0) { x->data.integer -= y->data.integer;}
        else if (strcmp(op, "*") == 0) { x->data.integer *= y->data.integer;}
        else if (strcmp(op, "/") == 0) {
            if (y->data.integer == 0) {
                lval_del(x); lval_del(y);
                x = lval_err("Division By Zero.");
                break;
            }
            x->data.integer /= y->data.integer;
        }
        else if (strcmp(op, "%") == 0) {
            if (y->data.integer == 0) {
                lval_del(x); lval_del(y);
                x = lval_err("Division By Zero.");
                break;
            }
            x->data.integer %= y->data.integer;
        }   
        else if (strcmp(op, "min") == 0) {
            if (y->data.integer < x->data.integer) { x->data.integer = y->data.integer;}
        }
        else if (strcmp(op, "max") == 0) {
            if (y->data.integer > x->data.integer) { x->data.integer = y->data.integer;}
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
        if (strcmp(op, "+") == 0) { x->data.decimal += y->data.decimal;}
        else if (strcmp(op, "-") == 0) { x->data.decimal -= y->data.decimal;}
        else if (strcmp(op, "*") == 0) { x->data.decimal *= y->data.decimal;}
        else if (strcmp(op, "/") == 0) {
            if (y->data.decimal == 0) {
                lval_del(x); lval_del(y);
                x = lval_err("Division By Zero.");
                break;
            }
            x->data.decimal /= y->data.decimal;
        }
        else if (strcmp(op, "%") == 0) {
            lval_del(x); lval_del(y);
            x = lval_err("Mod is invalid operation on decimal.");
            break;
        }   
        else if (strcmp(op, "min") == 0) {
            if (y->data.decimal < x->data.decimal) {
                x->data.decimal = y->data.decimal;
            }
        }
        else if (strcmp(op, "max") == 0) {
            if (y->data.decimal > x->data.decimal) { 
                x->data.decimal = y->data.decimal; 
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
    a = int_to_dec(a);

    if (a->type == LVAL_ERR) {
        return a;
    }
    
    if (a->cell[0]->type == LVAL_INT) {
        return builtin_op_i(e, a, op);
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

lval* builtin_cond_d(lenv* e, lval* v1, lval* v2, char* op) {
    lval* x = lval_bool(false);
    
    if (strcmp(op, "<") == 0) {
        x->data.boolean = v1->data.decimal < v2->data.decimal;
    }
    else if (strcmp(op, ">") == 0) {
        x->data.boolean = v1->data.decimal > v2->data.decimal;
    }
    else if (strcmp(op, ">=") == 0) {
        x->data.boolean = v1->data.decimal >= v2->data.decimal;
    }
    else if (strcmp(op, "<=") == 0) {
        x->data.boolean = v1->data.decimal <= v2->data.decimal;
    }
    else if (strcmp(op, "!=") == 0) {
        x->data.boolean = v1->data.decimal != v2->data.decimal;
    }

    lval_del(v1); lval_del(v2);
    return x;
}

lval* builtin_cond(lenv* e, lval* a, char* op) {
    
    /* check that there are two values */
    LASSERT_NUM(op, a, 2);
    
    a = int_to_dec(a);
    if (a->type == LVAL_ERR) {
        return a;
    }

    lval* v1 = lval_pop(a, 0);
    lval* v2 = lval_take(a, 0);
    
    if (v1->type == LVAL_INT) {
        return builtin_cond_i(e, v1, v2, op);
    }
    else {
        return builtin_cond_d(e, v1, v2, op);
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
    // Check if each argument is either int or dec
    LASSERT_NUM("pow", a, 2);
    lval* b, *exp, *temp;
    if (a->cell[0]->type == LVAL_INT) {
        temp = lval_pop(a, 0);
        b = lval_dec((double) temp->data.integer);
        lval_del(temp);
    } else if (a->cell[0]->type == LVAL_DEC) {
        b = lval_pop(a, 0);
    } else {
        char *type = ltype_name(a->cell[0]->type);
        lval_del(a);
        return lval_err("Expect integer or decimal. Got %s.", type);
    }
    
    if (a->cell[0]->type == LVAL_INT) {
        temp = lval_take(a, 0);
        exp = lval_dec((double) temp->data.integer);
        lval_del(temp);
    } else if (a->cell[0]->type == LVAL_DEC) {
        exp = lval_take(a, 0);
    } else {
        char *type = ltype_name(a->cell[0]->type);
        lval_del(a);
        return lval_err("Expect integer or decimal. Got %s.", type);
    }
    
    lval* ans = lval_dec(pow(b->data.decimal, exp->data.decimal));
    lval_del(b); lval_del(exp);

    return ans;
}

lval* builtin_lessthan(lenv* e, lval* a) {
    return builtin_cond(e, a, "<");
}

lval* builtin_greaterthan(lenv* e, lval* a) {
    return builtin_cond(e, a, ">");
}

bool lval_eq(lval* x, lval* y) {

    /* If two lvals are not the same type, return false. */
    if (x->type != y->type) {
        return false;
    }
    
    switch (x->type) {
        case LVAL_INT: return (x->data.integer == y->data.integer);
        case LVAL_DEC: return (x->data.decimal == y->data.decimal);
        case LVAL_BOOL: return (x->data.boolean == y->data.boolean);
        case LVAL_ERR: return (strcmp(x->data.err, y->data.err) == 0);
        case LVAL_SYM: return (strcmp(x->data.sym, y->data.sym) == 0);
        case LVAL_STR: return (strcmp(x->data.str, y->data.str) == 0);
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
        /* Take first expression and evaluate it */
        lval* s = lval_take(a, 0);
        s->type = LVAL_SEXPR;
        return lval_eval(e, s);
    }
    else {
        /* Take the second expression and evaluate it */
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
    /* Iterate to the first parent environment */
    while (e->par) {
        e = e->par;
    }
    /* Find the symbol in the environment and check if builtin == NULL */
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
    
    /* If 'def' define in global env. If 'put' define locally. */
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

    /* Pop name off the first qexpr */
    lval* name = lval_qexpr();
    name = lval_add(name, lval_pop(a->cell[0], 0));
    
    /* First element of l_args (formals) is rest of first qexpr */
    lval* l_args = lval_qexpr();
    l_args = lval_add(l_args, lval_pop(a, 0));
    /* Add body of function to l_args */
    l_args = lval_add(l_args, lval_take(a, 0));

    /* Create lambda function */
    lval* lambda = builtin_lambda(e, l_args);
    /* Add name to lambda to pass to def */
    lval* x = lval_qexpr();
    x = lval_add(x, name);
    x = lval_add(x, lambda);

    return builtin_def(e, x);
}

lval* lval_read(mpc_ast_t* t);

lval* builtin_load(lenv* e, lval* a) {
    LASSERT_NUM("load", a, 1);
    LASSERT_TYPE("load", a, 0, LVAL_STR);

    /* Parse file given by string name */
    mpc_result_t r;
    if (mpc_parse_contents(a->cell[0]->data.str, Lispy, &r)) {
        
        /* Read contents */
        lval* expr = lval_read(r.output);
        mpc_ast_delete(r.output);

        /* Evaluate each expression */
        while (expr->count) {
            lval* x = lval_eval(e, lval_pop(expr, 0));
            /* If evaluation is an error, print it */
            if (x->type == LVAL_ERR) { lval_println(x); }
            lval_del(x);
        }

        /* Delete expression and arguments */
        lval_del(expr);
        lval_del(a);

        /* Return empty list */
        return lval_sexpr();
    } else {
        /* Get parse error as a string */
        char* err_msg = mpc_err_string(r.error);
        mpc_err_delete(r.error);

        /* Create new error message using it */
        lval* err = lval_err("Could not load Library %s", err_msg);

        /* Cleanup and return error. */
        free(err_msg);
        lval_del(a);
        return err;
    }
}

lval* builtin_print(lenv* e, lval* a) {
    for (int i = 0; i < a->count; i++) {
        lval_print(a->cell[i]);
        putchar(' ');
    }
    putchar('\n');
    lval_del(a);

    return lval_sexpr();
}

lval* builtin_error(lenv* e, lval* a) {
    LASSERT_NUM("error", a, 1);
    LASSERT_TYPE("error", a, 0, LVAL_STR);

    /* Construct Error from first argument */
    lval* err = lval_err(a->cell[0]->data.str);

    /* Delete arguments and return */
    lval_del(a);
    return err;
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
        else if (func->builtin == builtin_read) return "read";
        else if (func->builtin == builtin_join) return "join";
        else if (func->builtin == builtin_cons) return "cons";
        else if (func->builtin == builtin_init) return "init";
        else if (func->builtin == builtin_len) return "len";
        else if (func->builtin == builtin_def) return "def";
        else if (func->builtin == builtin_env) return "env";
        else if (func->builtin == builtin_load) return "load";
        else if (func->builtin == builtin_exit) return "exit";
        else if (func->builtin == builtin_print) return "print";
        else if (func->builtin == builtin_error) return "error";
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
    lenv_add_builtin(e, "or", builtin_or);
    lenv_add_builtin(e, "&&", builtin_and);
    lenv_add_builtin(e, "and", builtin_and);
    
    /* String functions */
    lenv_add_builtin(e, "load", builtin_load);
    lenv_add_builtin(e, "print", builtin_print);
    lenv_add_builtin(e, "error", builtin_error);
    lenv_add_builtin(e, "read", builtin_read);
}


/********************************************************************
 *  Evaluation 
 ********************************************************************/

lval* lval_eval_sexpr(lenv* e, lval* v) {
    /* Evaluate each cell in the sexpr  */
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]);
    }
    
    /* If there are any errors after evaluation, return that error */
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
    }
    
    /* Return empty expression or single lval expression directly */
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


