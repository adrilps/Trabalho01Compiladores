/*
 * Declarações para uma calculadora avançada
 */

/* Interface com o lexer */
extern int yylineno;
void yyerror(char *s, ...);

/* Tabela de símbolos */
struct symbol { /* um nome de variável */
    char *name;
    double value;
    struct ast *func;        /* stmt para função */
    struct symlist *syms;    /* lista de argumentos */
};

/* Tabela de símbolos de tamanho fixo */
#define NHASH 9997
struct symbol *lookup(char *);

/* Lista de símbolos, para lista de argumentos */
struct symlist {
    struct symbol *sym;
    struct symlist *next;
};

struct symlist *newsymlist(struct symbol *sym, struct symlist *next);
void symlistfree(struct symlist *sl);

/*
 * Tipos de nós:
 * O operação lógica OR
 * A operação lógica AND
 * + - * /
 * 0–7 operadores de comparação, 04 igual, 02 menor que, 01 maior que
 * L expressão ou lista de comandos
 * I comando IF
 * W comando WHILE
 * P comando FOR
 * N símbolo de referência
 * = atribuição
 * S lista de símbolos
 * F chamada de função predefinida
 * C chamada de função definida pelo usuário
 */

/* Funções predefinidas */
enum bifs {
    B_sqrt = 1,
    B_exp,
    B_log,
    B_print
};

/* Nós na AST */
/* Todos têm o "nodetype" inicial em comum */
struct ast {
    int nodetype;
    struct ast *l;
    struct ast *r;
};

struct fncall { /* funções predefinidas */
    int nodetype;        /* tipo F */
    struct ast *l;
    enum bifs functype;
};

struct ufncall { /* funções do usuário */
    int nodetype;        /* tipo C */
    struct ast *l;       /* lista de argumentos */
    struct symbol *s;
};

struct flow {
    int nodetype;        /* tipo I ou W ou P*/
    struct ast *cond;    /* condição */
    struct ast *tl;      /* ramo "then" ou lista "do" */
    struct ast *el;      /* ramo opcional "else" */
    struct ast *init;    /* ramo opcional "init" */
    struct ast *inc;     /* ramo opcional "inc" */
};

struct numval {
    int nodetype;        /* tipo K */
    double number;
};

struct symref {
    int nodetype;        /* tipo N */
    struct symbol *s;
};

struct symasgn {
    int nodetype;        /* tipo = */
    struct symbol *s;
    struct ast *v;       /* valor a ser atribuído */
};

/* Construção de uma AST */
struct ast *newast(int nodetype, struct ast *l, struct ast *r);
struct ast *newcmp(int cmptype, struct ast *l, struct ast *r);
struct ast *newfunc(int functype, struct ast *l);
struct ast *newcall(struct symbol *s, struct ast *l);
struct ast *newref(struct symbol *s);
struct ast *newasgn(struct symbol *s, struct ast *v);
struct ast *newnum(double d);
struct ast *newflow(int nodetype, struct ast *cond, struct ast *tl, struct ast *tr, struct ast *init, struct ast *inc);

/* Definição de uma função */
void dodef(struct symbol *name, struct symlist *syms, struct ast *stmts);

/* Avaliação de uma AST */
double eval(struct ast *);

/* Deletar e liberar uma AST */
void treefree(struct ast *);