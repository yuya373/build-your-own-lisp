#include <stdio.h>
/* "<header name>" searches the system locations for headers first */
#include <stdlib.h>
/* "header name" searches the current directory for headers first */
#include "mpc.h"

#ifdef _WIN32
#include <string.h>
static char buffer[2048];

char *readline(char *prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char *cpy = malloc(strlen(buffer) + 1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy) - 1] = '\0';
  return cpy;
}

void add_history(char *unused) {}
#else
/* yaourt libedit */
/* cc -std=c99 -Wall -ledit prompt.c -o prompt */
#include <editline/readline.h>
#endif

#define LASSERT(args, cond, err)                                               \
  if (!(cond)) {                                                               \
    lval_del(args);                                                            \
    return lval_err(err);                                                      \
  }
/* Forward Declarations */
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

enum LISP_VALUE {
  LVAL_NUM,
  LVAL_ERR,
  LVAL_SYM,
  LVAL_SEXPR,
  LVAL_QEXPR,
  LVAL_FUN
};
/* To get an lval* we dereference lbuiltin and call it with a lenv* and a lval*.
 * Therefore lbuiltin must be a function pointer that takes an lenv* and a lval*
 * and returns a lval*. */
typedef lval *(*lbuiltin)(lenv *, lval *);
/* reference to itself in struct, need struct name */
/* must not contain its own type directly, contain pointer to its own type */
/* struct size grow infinite when contain its own type directly */
struct lval {
  int type;
  double num;
  char *err;
  char *sym;
  int count;
  lbuiltin fun;
  /* pointer to (a list of (pointer to lval)) */
  struct lval **cell;
};

lval *lval_num(double n) {
  lval *v = (lval *)malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = n;
  return v;
}

lval *lval_err(char *m) {
  lval *v = (lval *)malloc(sizeof(lval));
  v->type = LVAL_ERR;
  /* strlen returns the number of bytes in a string excluding the null
   * terminator */
  /* so need to add one to ensure enough allocated space for org string + null
   * terminator */
  v->err = (char *)malloc(strlen(m) + 1);
  strcpy(v->err, m);
  return v;
}

lval *lval_sym(char *s) {
  lval *v = (lval *)malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = (char *)malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

lval *lval_sexpr(void) {
  lval *v = (lval *)malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  /* NULL is constant that points to memory location 0 */
  v->cell = NULL;
  return v;
}

lval *lval_qexpr(void) {
  lval *v = (lval *)malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

void lval_del(lval *v) {
  switch (v->type) {
  case LVAL_NUM:
    break;
  case LVAL_ERR:
    free(v->err);
    break;
  case LVAL_SYM:
    free(v->sym);
    break;
  case LVAL_SEXPR:
  case LVAL_QEXPR:
    /* delete all elements inside cell */
    for (int i = 0; i < v->count; i++) {
      lval_del(v->cell[i]);
    }
    /* also delete cell (memory allocated to contain pointers) itself */
    free(v->cell);
    break;
  }

  free(v);
}

lval *lval_read_num(mpc_ast_t *t) {
  errno = 0;
  double x = strtod(t->contents, NULL);
  return errno != ERANGE ? lval_num(x) : lval_err((char *)"invalid number");
}

lval *lval_add(lval *v, lval *x) {
  v->count++;
  v->cell = (struct lval **)realloc(v->cell, (sizeof(lval *) * v->count));
  v->cell[v->count - 1] = x;
  return v;
}

lval *lval_read(mpc_ast_t *t) {
  if (strstr(t->tag, "number")) {
    return lval_read_num(t);
  }
  if (strstr(t->tag, "symbol")) {
    return lval_sym(t->contents);
  }
  lval *x = NULL;
  if (strcmp(t->tag, ">") == 0) {
    x = lval_sexpr();
  }
  if (strstr(t->tag, "sexpr")) {
    x = lval_sexpr();
  }
  if (strstr(t->tag, "qexpr")) {
    x = lval_qexpr();
  }

  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) {
      continue;
    }
    if (strcmp(t->children[i]->contents, ")") == 0) {
      continue;
    }
    if (strcmp(t->children[i]->contents, "{") == 0) {
      continue;
    }
    if (strcmp(t->children[i]->contents, "}") == 0) {
      continue;
    }
    if (strcmp(t->children[i]->tag, "regex") == 0) {
      /* printf("tag is regex: %s\n", t->children[i]->contents); */
      continue;
    }
    x = lval_add(x, lval_read(t->children[i]));
  }
  return x;
}

/* forward declare to resolve dependency */
void lval_print(lval *v);
void lval_expr_print(lval *v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++) {
    lval_print(v->cell[i]);
    if (i != (v->count - 1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

void lval_print(lval *v) {
  switch (v->type) {
  case LVAL_NUM:
    printf("%f", v->num);
    break;
  case LVAL_ERR:
    printf("Error: %s", v->err);
    break;
  case LVAL_SYM:
    printf("%s", v->sym);
    break;
  case LVAL_SEXPR:
    lval_expr_print(v, '(', ')');
    break;
  case LVAL_QEXPR:
    lval_expr_print(v, '{', '}');
    break;
  }
}

void lval_println(lval *v) {
  lval_print(v);
  putchar('\n');
}

lval *lval_take(lval *v, int i);
lval *lval_pop(lval *v, int i);
lval *lval_eval(lval *v);
lval *buitin(lval *a, char *func);
lval *lval_eval_sexpr(lval *v) {
  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lval_eval(v->cell[i]);
  }

  for (int i = 0; i < v->count; i++) {
    if (v->cell[i]->type == LVAL_ERR) {
      return lval_take(v, i);
    }
  }

  if (v->count == 0) {
    return v;
  }

  if (v->count == 1) {
    return lval_take(v, 0);
  }

  lval *f = lval_pop(v, 0);
  if (f->type != LVAL_SYM) {
    lval_del(f);
    lval_del(v);
    return lval_err((char *)"S-expression Does not start with symbol!");
  }

  lval *result = buitin(v, f->sym);
  lval_del(f);
  return result;
}

lval *lval_eval(lval *v) {
  if (v->type == LVAL_SEXPR) {
    return lval_eval_sexpr(v);
  }
  return v;
}

lval *lval_pop(lval *v, int i) {
  lval *x = v->cell[i];

  /* struct lval *destination = v->cell[i]; */
  /* struct lval *src = v->cell[i + 1]; */
  /* size_t n = sizeof(lval *) * (v->count - (i + 1)); */
  /* memmove(&destination, &src, n); */
  /* ↑this does not work why? :thinking: */
  struct lval **destination = &v->cell[i];
  struct lval **src = &v->cell[i + 1];
  size_t n = sizeof(lval *) * (v->count - (i + 1));
  memmove(destination, src, n);

  v->count--;

  v->cell = (struct lval **)realloc(v->cell, (sizeof(lval *) * v->count));

  return x;
}

lval *lval_take(lval *v, int i) {
  lval *x = lval_pop(v, i);
  lval_del(v);
  return x;
}

lval *builtin_op(lval *a, char *op) {
  for (int i = 0; i < a->count; i++) {
    /* printf("lval type: %i", a->cell[i]->type); */
    if (a->cell[i]->type != LVAL_NUM) {
      lval_del(a);
      return lval_err((char *)"Cannot operate on non-number!");
    }
  }

  lval *x = lval_pop(a, 0);

  if ((strcmp(op, "-") == 0) && a->count == 0) {
    x->num = -x->num;
  }

  while (a->count > 0) {
    lval *y = lval_pop(a, 0);

    if (strcmp(op, "+") == 0 || strcmp(op, "add") == 0) {
      x->num += y->num;
    }
    if (strcmp(op, "-") == 0 || strcmp(op, "sub") == 0) {
      x->num -= y->num;
    }
    if (strcmp(op, "*") == 0 || strcmp(op, "mul") == 0) {
      x->num *= y->num;
    }
    if (strcmp(op, "/") == 0 || strcmp(op, "div") == 0) {
      if (y->num == 0) {
        lval_del(x);
        lval_del(y);
        x = lval_err((char *)"Division By Zero!");
        break;
      }
      x->num /= y->num;
    }
    if (strcmp(op, "%") == 0) {
      if (y->num == 0) {
        lval_del(x);
        lval_del(y);
        x = lval_err((char *)"Division By Zero!");
        break;
      }
      x->num = (int)x->num % (int)y->num;
    }
    if (strcmp(op, "min") == 0) {
      if (x->num > y->num) {
        x->num = y->num;
      }
    }
    if (strcmp(op, "max") == 0) {
      if (x->num < y->num) {
        x->num = y->num;
      }
    }

    lval_del(y);
  }

  lval_del(a);
  return x;
}

lval *builtin_head(lval *a) {
  LASSERT(a, a->count == 1,
          (char *)"Function 'head' passed too many arguments!");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          (char *)"Function 'head' passed incorrect types!");
  LASSERT(a, a->cell[0]->count != 0, (char *)"Function 'head' passed {}!");

  lval *v = lval_take(a, 0);
  while (v->count > 1) {
    lval_del(lval_pop(v, 1));
  }
  return v;
}

lval *builtin_tail(lval *a) {
  LASSERT(a, a->count == 1,
          (char *)"Function 'tail' passed too many arguments!");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          (char *)"Function 'tail' passed incorrect types!");
  LASSERT(a, a->cell[0]->count != 0, (char *)"Function 'tail' passed {}!");

  lval *v = lval_take(a, 0);
  lval_del(lval_pop(v, 0));
  return v;
}

lval *builtin_list(lval *a) {
  a->type = LVAL_QEXPR;
  return a;
}

lval *builtin_eval(lval *a) {
  LASSERT(a, a->count == 1,
          (char *)"Function 'eval' passed too many arguments!");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          (char *)"Function 'eval' passed incorrect type!");

  lval *x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(x);
}

lval *lval_join(lval *x, lval *y) {
  while (y->count) {
    x = lval_add(x, lval_pop(y, 0));
  }

  lval_del(y);
  return x;
}

lval *builtin_join(lval *a) {
  for (int i = 0; i < a->count; i++) {
    LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
            (char *)"Function 'join' passed incorrect type.");
  }

  lval *x = lval_pop(a, 0);
  while (a->count) {
    x = lval_join(x, lval_pop(a, 0));
  }

  lval_del(a);
  return x;
}

lval *builtin_cons(lval *a) {
  for (int i = 1; i < a->count; i++) {
    LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
            (char *)"Function 'cons' passed incorrect type.");
  }

  lval *x = lval_qexpr();
  x = lval_add(x, lval_pop(a, 0));

  while (a->count) {
    x = lval_join(x, lval_pop(a, 0));
  }

  lval_del(a);
  return x;
}

lval *builtin_len(lval *a) {
  LASSERT(a, a->count == 1,
          (char *)"Function 'len' passed too many arguments!");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          (char *)"Function 'len' passed incorrect type.");
  long len = 0;
  for (int i = 0; i < a->count; i++) {
    len = len + a->cell[i]->count;
  }
  return lval_num(len);
}

lval *builtin_init(lval *a) {
  LASSERT(a, a->count == 1,
          (char *)"Function 'init' passed too many arguments!");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          (char *)"Function 'init' passed incorrect type");

  lval *x = lval_qexpr();
  lval *arg = lval_pop(a, 0);

  while (arg->count > 1) {
    x = lval_add(x, lval_pop(arg, 0));
  }

  lval_del(a);

  return x;
}

lval *buitin(lval *a, char *func) {
  if (strcmp("list", func) == 0) {
    return builtin_list(a);
  }
  if (strcmp("head", func) == 0) {
    return builtin_head(a);
  }
  if (strcmp("tail", func) == 0) {
    return builtin_tail(a);
  }
  if (strcmp("join", func) == 0) {
    return builtin_join(a);
  }
  if (strcmp("eval", func) == 0) {
    return builtin_eval(a);
  }
  if (strcmp("cons", func) == 0) {
    return builtin_cons(a);
  }
  if (strcmp("len", func) == 0) {
    return builtin_len(a);
  }
  if (strcmp("init", func) == 0) {
    return builtin_init(a);
  }
  if (strcmp("+", func) == 0 || strcmp("-", func) == 0 ||
      strcmp("^", func) == 0 || strcmp("*", func) == 0 ||
      strcmp("/", func) == 0 || strcmp("%", func) == 0 ||
      strcmp("add", func) == 0 || strcmp("sub", func) == 0 ||
      strcmp("mul", func) == 0 || strcmp("div", func) == 0 ||
      strcmp("min", func) == 0 || strcmp("max", func) == 0) {
    return builtin_op(a, func);
  }
  lval_del(a);
  return lval_err((char *)"Unknown Function!");
}

int main(int argc, char **argv) {
  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Symbol = mpc_new("symbol");
  mpc_parser_t *Sexpr = mpc_new("sexpr");
  mpc_parser_t *Qexpr = mpc_new("qexpr");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");
  /* this is part of a C string we need to put two backslashes to represent a
   * single backslash character in the input. */
  mpca_lang(MPCA_LANG_DEFAULT, "number : /-?[0-9]+[\\.]?[0-9]*/ ; \
             symbol : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ; \
             sexpr: '(' <expr>* ')' ; \
             qexpr: '{' <expr>* '}' ; \
             expr: <number> | <symbol> | <sexpr> | <qexpr> ; \
             lispy: /^/ <expr>* /$/ ; \
            ",
            Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

  puts("Lispy Version 0.0.0.0.1");
  puts("Press Ctrl+c to Exit\n");

  /* not 0, infinit loop */
  while (1) {
    mpc_result_t r;
    char *input = readline("lispy> ");
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      mpc_ast_t *ast = (mpc_ast_t *)r.output;
      lval *x = lval_eval(lval_read(ast));
      lval_println(x);
      lval_del(x);
      mpc_ast_delete((mpc_ast_t *)r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    add_history(input);

    /* from stdlib.h */
    free(input);
  }

  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
  return 0;
}
