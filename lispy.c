#include "mpc.h"
#include "lval.h"
#include "lenv.h"
#include "builtins.h"
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


 

/***************************************************************
 * Reading 
 ***************************************************************/

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

lval* lval_read_str(mpc_ast_t* t) {
    /* Cut off the final quote character */
    t->contents[strlen(t->contents)-1] = '\0';
    /* Copy the string leaving out first quote character */
    char* unescaped = malloc(strlen(t->contents + 1) + 1);
    strcpy(unescaped, t->contents + 1);
    /* Pass through unescape function */
    unescaped = mpcf_unescape(unescaped);
    /* Construct a new lval using string */
    lval* s = lval_str(unescaped);
    /* Free and return */
    free(unescaped);
    return s;
}

lval* lval_read(mpc_ast_t* t) {
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "symbol")) { 
        if (strcmp(t->contents, "true") == 0) { return lval_bool(true); }
        else if (strcmp(t->contents, "false") == 0) { return lval_bool(false); }
        else { return lval_sym(t->contents); }
    }
    if (strstr(t->tag, "string")) { return lval_read_str(t); }

    lval* x = NULL;
    if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); } 
    if (strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }
    if (strstr(t->tag, "qexpr"))  { x = lval_qexpr(); }
    
    for (int i = 0; i < t->children_num; i++) {
        if (strstr(t->children[i]->tag, "comment")) { continue; }
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
        if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
        x = lval_add(x, lval_read(t->children[i]));
    }
    
    return x;
}


/****************************************************************
 * Main 
 ****************************************************************/

int main(int argc, char** argv) {
    
    Number = mpc_new("number");
    Symbol = mpc_new("symbol");
    String = mpc_new("string");
    Comment = mpc_new("comment");
    Sexpr  = mpc_new("sexpr");
    Qexpr  = mpc_new("qexpr");
    Expr   = mpc_new("expr");
    Lispy  = mpc_new("lispy");
    
    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                       \
            number : /-?(([0-9]*[.])?[0-9]+([.][0-9]*)?)/ ;     \
            symbol : /[a-zA-Z0-9_+\\-*\\/\%^\\\\=<>!&|]+/ ;     \
            string : /\"(\\\\.|[^\"])*\"/ ;                     \
            comment : /;.[^\\n\\r]*/ ;                          \
            sexpr  : '(' <expr>* ')' ;                          \
            qexpr  : '{' <expr>* '}' ;                          \
            expr   : <number> | <symbol> | <string> |           \
                     <comment> | <sexpr> | <qexpr> ;            \
            lispy  : /^/ <expr>* /$/ ;                          \
        ",
        Number, Symbol, String, Comment, Sexpr, Qexpr, Expr, Lispy);
    
    puts("Lispy Version 0.0.0.0.7");
    puts("Press Ctrl+c to Exit\n");
    
    lenv* e = lenv_new();
    lenv_add_builtins(e);

    bool quit = false;

    lval* s = builtin_load(e, lval_add(lval_sexpr(), lval_str("stdlib.lspy")));
    if (s->type == LVAL_ERR) {
        lval_println(s);
    }
    lval_del(s);

    if (argc == 1) {
        
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
    } 
    if (argc >= 2) {
        for (int i = 1; i < argc; i++) {
            lval* args = lval_add(lval_sexpr(), lval_str(argv[i]));
            lval* x = builtin_load(e, args);
            
            if (x->type == LVAL_ERR) {
                lval_println(x);
            }
            lval_del(x);
        }
    }

    lenv_del(e);
    
    mpc_cleanup(8, 
            Number, Symbol, String, Comment, 
            Sexpr, Qexpr, Expr, Lispy);
    
    return 0;
}
