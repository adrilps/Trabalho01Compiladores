/*
 * Funções Auxiliares para uma calculadora avançada
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include "bison-calc.h"

extern FILE *yyin;
extern FILE *yyout;

/* funções em C para TS */
/* função hashing */

struct symbol symtab[NHASH];

static unsigned symhash(char *sym)
{
    unsigned int hash = 0;
    unsigned c;

    while (c = *sym++)
        hash = hash * 9 ^ c;

    return hash;
}

struct symbol *lookup(char *sym)
{
    struct symbol *sp = &symtab[symhash(sym) % NHASH];
    int scount = NHASH;

    while (--scount >= 0) {
        if (sp->name && !strcasecmp(sp->name, sym))
            return sp;

        if (!sp->name) { /* nova entrada na TS */
            sp->name = strdup(sym);
            sp->value = 0;
            sp->func = NULL;
            sp->syms = NULL;
            return sp;
        }

        if (++sp >= symtab + NHASH)
            sp = symtab; /* tenta a próxima entrada */
    }

    yyerror("overflow na tabela de símbolos\n");
    abort(); /* tabela está cheia */
}

struct ast *newast(int nodetype, struct ast *l, struct ast *r)
{
    struct ast *a = malloc(sizeof(struct ast));

    if (!a) {
        yyerror("sem espaço");
        exit(0);
    }
    a->nodetype = nodetype;
    a->l = l;
    a->r = r;
    return a;
}

struct ast *newnum(double d)
{
    struct numval *a = malloc(sizeof(struct numval));

    if (!a) {
        yyerror("sem espaco");
        exit(0);
    }
    a->nodetype = 'K';
    a->number = d;
    return (struct ast *)a;
}

struct ast *newcmp(int cmptype, struct ast *l, struct ast *r)
{
    struct ast *a = malloc(sizeof(struct ast));

    if (!a) {
        yyerror("sem espaco");
        exit(0);
    }
    a->nodetype = '0' + cmptype;
    a->l = l;
    a->r = r;
    return a;
}

struct ast *newfunc(int functype, struct ast *l)
{
    struct fncall *a = malloc(sizeof(struct fncall));

    if (!a) {
        yyerror("sem espaco");
        exit(0);
    }
    a->nodetype = 'F';
    a->l = l;
    a->functype = functype;
    return (struct ast *)a;
}

struct ast *newcall(struct symbol *s, struct ast *l)
{
    struct ufncall *a = malloc(sizeof(struct ufncall));
    if (!a) {
        yyerror("sem espaco");
        exit(0);
    }
    a->nodetype = 'C';
    a->l = l;
    a->s = s;
    return (struct ast *)a;
}

struct ast *newref(struct symbol *s)
{
    struct symref *a = malloc(sizeof(struct symref));
    if (!a) {
        yyerror("sem espaco");
        exit(0);
    }
    a->nodetype = 'N';
    a->s = s;
    return (struct ast *)a;
}

struct ast *newasgn(struct symbol *s, struct ast *v)
{
    struct symasgn *a = malloc(sizeof(struct symasgn));
    if (!a) {
        yyerror("sem espaco");
        exit(0);
    }
    a->nodetype = '=';
    a->s = s;
    a->v = v;
    return (struct ast *)a;
}

struct ast *newflow(int nodetype, struct ast *cond, struct ast *tl, struct ast *el, struct ast *init, struct ast *inc)
{
    struct flow *a = malloc(sizeof(struct flow));
    if (!a) {
        yyerror("sem espaco");
        exit(0);
    }
    a->nodetype = nodetype;
    a->cond = cond;
    a->tl = tl;
    a->el = el;
    a->init = init;
    a->inc = inc;
    return (struct ast *)a;
}

/* libera uma arvore de AST */
void treefree(struct ast *a)
{
    switch (a->nodetype) {
        /* duas subarvores */
        case 'A':
        case 'O':
        case '+':
        case '-': 
        case '*': 
        case '/':
        case '1': case '2': case '3': case '4': case '5': case '6':
        case 'L':
            treefree(a->r);
            /* uma subarvore */
        case 'C': case 'F':
            treefree(a->l);
            
        /* sem subarvore */
        case 'K': case 'N':
            break;
        case '=':
            free(((struct symasgn *)a)->v);
            break;
        /* acima de 3 subarvores */
        case 'I': case 'W': case 'P':
            free(((struct flow *)a)->cond);
            if (((struct flow *)a)->tl) treefree(((struct flow *)a)->tl);
            if (((struct flow *)a)->el) treefree(((struct flow *)a)->el);
            if (((struct flow *)a)->init) treefree(((struct flow *)a)->init);
            if (((struct flow *)a)->inc) treefree(((struct flow *)a)->inc);
            break;

        default: printf("erro interno: free bad node %c\n", a->nodetype);
        }

    free(a); /* sempre libera o proprio no */
}

struct symlist *newsymlist(struct symbol *sym, struct symlist *next)
{
    struct symlist *sl = malloc(sizeof(struct symlist));
    if (!sl) {
        yyerror("sem espaco");
        exit(0);
    }
    sl->sym = sym;
    sl->next = next;
    return sl;
}

/* libera uma lista de simbolos */
void symlistfree(struct symlist *sl)
{
    struct symlist *nsl;
    while (sl) {
        nsl = sl->next;
        free(sl);
        sl = nsl;
    }
}

/* etapa principal >> avaliacao de expressoes, comandos, funcoes, ... */
static double callbuiltin(struct fncall *);
static double calluser(struct ufncall *);

double eval(struct ast *a)
{
    double v;

    if (!a) {
        yyerror("erro interno, null eval");
        return 0.0;
    }

    switch (a->nodetype) {
        /* constante */
        case 'K': v = ((struct numval *)a)->number; break;

        /* referencia de nome */
        case 'N': v = ((struct symref *)a)->s->value; break;

        /* atribuicao */
        case '=':
            v = ((struct symasgn *)a)->s->value = eval(((struct symasgn *)a)->v);
            break;

        /* expressoes */
        case 'O': v  = (eval(a->l) || eval(a->r)) ? 1 : 0; break;
        case 'A': v  = (eval(a->l) && eval(a->r)) ? 1 : 0; break;
        case '+': v = eval(a->l) + eval(a->r); break;
        case '-': v = eval(a->l) - eval(a->r); break;
        case '*': v = eval(a->l) * eval(a->r); break;
        case '/': v = eval(a->l) / eval(a->r); break;

        /* comparacoes */
        case '1': v = (eval(a->l) > eval(a->r)) ? 1 : 0; break;
        case '2': v = (eval(a->l) < eval(a->r)) ? 1 : 0; break;
        case '3': v = (eval(a->l) != eval(a->r)) ? 1 : 0; break;
        case '4': v = (eval(a->l) == eval(a->r)) ? 1 : 0; break;
        case '5': v = (eval(a->l) >= eval(a->r)) ? 1 : 0; break;
        case '6': v = (eval(a->l) <= eval(a->r)) ? 1 : 0; break;

        /* controle de fluxo */
        /* if / then / else */
        case 'I':
            if (eval(((struct flow *)a)->cond) != 0) {
                if (((struct flow *)a)->tl) {
                    v = eval(((struct flow *)a)->tl);
                } else
                    v = 0.0;
            } else {
                if (((struct flow *)a)->el) {
                    v = eval(((struct flow *)a)->el);
                } else
                    v = 0.0;
            }
            break;

        /* while / do */
        case 'W':
            v = 0.0;
            if (((struct flow *)a)->tl) {
                while (eval(((struct flow *)a)->cond) != 0)
                    v = eval(((struct flow *)a)->tl);
            }
            break;

        case 'P':
            v = 0.0;
            if (((struct flow *)a)->tl && ((struct flow *)a)->cond && ((struct flow *)a)->inc) {
                for(eval(((struct flow *)a)->init);eval(((struct flow *)a)->cond) != 0;eval(((struct flow *)a)->inc)){
                    v = eval(((struct flow *)a)->tl);
                }
            }
            break;

        /* lista de comandos */
        case 'L':
            eval(a->l);
            v = eval(a->r);
            break;

        case 'F':
            v = callbuiltin((struct fncall *)a);
            break;

        case 'C':
            v = calluser((struct ufncall *)a);
            break;

        default:
            printf("erro interno: bad node %c\n", a->nodetype);
    }
    return v;
}

static double callbuiltin(struct fncall *f)
{
    enum bifs functype = f->functype;
    double v = eval(f->l);

    switch (functype) {
        case B_sqrt:
            return sqrt(v);
        case B_exp:
            return exp(v);
        case B_log:
            return log(v);
        case B_print:
            printf("=%4.4g\n", v);
            return v;
        default:
            yyerror("Funcao pre-definida %d desconhecida\n", functype);
            return 0.0;
    }
}

/* funcao definida por usuario */
void dodef(struct symbol *name, struct symlist *syms, struct ast *func)
{
    if (name->syms) symlistfree(name->syms);
    if (name->func) treefree(name->func);

    name->syms = syms;
    name->func = func;
}

static double calluser(struct ufncall *f)
{
    struct symbol *fn = f->s;       /* nome da funcao */
    struct symlist *sl;             /* argumentos (originais) da funcao */
    struct ast *args = f->l;        /* argumentos (usados) na funcao */
    double *oldval, *newval;        /* salvar valores de argumentos */
    double v;
    int nargs;
    int i;

    if (!fn->func) {
        yyerror("chamada para funcao %s indefinida", fn->name);
        return 0;
    }

    /* contar argumentos */
    sl = fn->syms;
    for (nargs = 0; sl; sl = sl->next)
        nargs++;

    /* prepara para salvar argumentos */
    oldval = (double *)malloc(nargs * sizeof(double));
    newval = (double *)malloc(nargs * sizeof(double));
    if (!oldval || !newval) {
        yyerror("Sem espaco em %s", fn->name);
        return 0.0;
    }

    /* avaliacao de argumentos */
    for (i = 0; i < nargs; i++) {
        if (!args) {
            yyerror("poucos argumentos na chamada da funcao %s", fn->name);
            free(oldval);
            free(newval);
            return 0.0;
        }

        if (args->nodetype == 'L') { /* se é uma lista de nós */
            newval[i] = eval(args->l);
            args = args->r;
        } else { /* se é o final da lista */
            newval[i] = eval(args);
            args = NULL;
        }
    }

    /* salvar valores (originais) dos argumentos, atribuir novos valores */
    sl = fn->syms;
    for (i = 0; i < nargs; i++) {
        struct symbol *s = sl->sym;
        oldval[i] = s->value;
        s->value = newval[i];
        sl = sl->next;
    }

    free(newval);

    /* avaliacao da funcao */
    v = eval(fn->func);

    /* recolocar os valores (originais) da funcao */
    sl = fn->syms;
    for (i = 0; i < nargs; i++) {
        struct symbol *s = sl->sym;
        s->value = oldval[i];
        sl = sl->next;
    }

    free(oldval);
    return v;
}

void yyerror(char *s, ...)
{
    va_list ap;
    va_start(ap, s);
    fprintf(stderr, "%d: erro: ", yylineno);
    vfprintf(stderr, s, ap);
    fprintf(stderr, "\n");
}


int main(int argc, char **argv) {
    if (argc > 1) {
        yyin = fopen(argv[1], "r");
        yyout = fopen("out.txt","w");
        if (!yyin || !yyout) {
            printf("O arquivo de entrada ou saida nao puderam ser abertos");
            return 0;
        }
        dup2(fileno(yyout), fileno(stdout));
        dup2(fileno(yyout), fileno(stderr));
    }

    yyparse();

    if (yyin != stdin) {
        fclose(yyin);
        fclose(yyout);
    }
    return 0;
}