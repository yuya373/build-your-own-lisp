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

#define LASSERT(args, cond, fmt, ...)                                          \
  if (!(cond)) {                                                               \
    lval *err = lval_err(fmt, ##__VA_ARGS__);                                  \
    lval_del(args);                                                            \
    return err;                                                                \
  }
#define LASSERT_TYPE(fname, args, i, expected)                                 \
  LASSERT(args, args->cell[i]->type == expected,                               \
          (char *)"Function '%s' passed incorrect type for argument %i. Got "  \
                  "%s, Expected %s.",                                          \
          fname, i, ltype_name(a->cell[i]->type), ltype_name(expected))

#define LASSERT_NUM(fname, args, num)                                          \
  LASSERT(args, args->count == num,                                            \
          (char *)"Function '%s' passed incorrect number of arguments. Got "   \
                  "%i, Expected %i.",                                          \
          fname, args->count, num)

#define LASSERT_NOT_EMPTY(fname, args, i)                                      \
  LASSERT(args, args->cell[i]->count != 0,                                     \
          (char *)"Function '%s' passed {} for argument %i.", fname, i)
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
  LVAL_FUN,
  LVAL_QUIT,
};

char *ltype_name(int t) {
  switch (t) {
  case LVAL_NUM:
    return (char *)"Number";
  case LVAL_ERR:
    return (char *)"Error";
  case LVAL_SYM:
    return (char *)"Symbol";
  case LVAL_SEXPR:
    return (char *)"S-Expression";
  case LVAL_QEXPR:
    return (char *)"Q-Expression";
  case LVAL_FUN:
    return (char *)"Function";
  default:
    return (char *)"Unknown";
  }
}
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

  lbuiltin builtin;
  lenv *env;
  lval *formals;
  lval *body;
  /* pointer to (a list of (pointer to lval)) */
  struct lval **cell;
  int count;
};

struct lenv {
  lenv *par;
  int count;
  char **syms;
  lval **vals;
};

lenv *lenv_new(void) {
  lenv *e = (lenv *)malloc(sizeof(lenv));
  e->par = NULL;
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;
  return e;
}

void lval_del(lval *v);
void lenv_del(lenv *e) {
  for (int i = 0; i < e->count; i++) {
    free(e->syms[i]);
    lval_del(e->vals[i]);
  }
  free(e);
}

lenv *lenv_copy(lenv *e);
lval *lval_copy(lval *v) {
  lval *x = (lval *)malloc(sizeof(lval));
  x->type = v->type;

  switch (v->type) {
  case LVAL_FUN:
    if (v->builtin) {
      x->builtin = v->builtin;
      x->sym = (char *)malloc(strlen(v->sym) + 1);
      strcpy(x->sym, v->sym);
    } else {
      x->builtin = NULL;
      x->env = lenv_copy(v->env);
      x->formals = lval_copy(v->formals);
      x->body = lval_copy(v->body);
    }
    break;
  case LVAL_NUM:
    x->num = v->num;
    break;
  case LVAL_ERR:
    x->err = (char *)malloc(strlen(v->err) + 1);
    strcpy(x->err, v->err);
    break;
  case LVAL_SYM:
    x->sym = (char *)malloc(strlen(v->sym) + 1);
    strcpy(x->sym, v->sym);
    break;
  case LVAL_SEXPR:
  case LVAL_QEXPR:
    x->count = v->count;
    x->cell = (lval **)malloc(sizeof(lval *) * x->count);
    for (int i = 0; i < x->count; i++) {
      x->cell[i] = lval_copy(v->cell[i]);
    }
    break;
  }
  return x;
}

lval *lval_err(char *m, ...);
lval *lenv_get(lenv *e, lval *k) {
  for (int i = 0; i < e->count; i++) {
    if (strcmp(e->syms[i], k->sym) == 0) {
      return lval_copy(e->vals[i]);
    }
  }

  if (e->par) {
    return lenv_get(e->par, k);
  } else {
    return lval_err((char *)"unbound symbol!");
  }
}

void lenv_put(lenv *e, lval *k, lval *v) {
  for (int i = 0; i < e->count; i++) {
    if (strcmp(e->syms[i], k->sym) == 0) {
      lval_del(e->vals[i]);
      e->vals[i] = lval_copy(v);
      return;
    }
  }

  e->count++;
  e->vals = (lval **)realloc(e->vals, sizeof(lval *) * e->count);
  e->syms = (char **)realloc(e->syms, sizeof(char *) * e->count);

  e->vals[e->count - 1] = lval_copy(v);
  e->syms[e->count - 1] = (char *)malloc(strlen(k->sym) + 1);
  strcpy(e->syms[e->count - 1], k->sym);
  return;
}

void lenv_def(lenv *e, lval *k, lval *v) {
  while (e->par) {
    e = e->par;
  }
  lenv_put(e, k, v);
}

lenv *lenv_copy(lenv *e) {
  lenv *n = (lenv *)malloc(sizeof(lenv));
  n->par = e->par;
  n->count = e->count;
  n->syms = (char **)malloc(sizeof(char *) * n->count);
  n->vals = (lval **)malloc(sizeof(lval *) * n->count);

  for (int i = 0; i < n->count; i++) {
    n->vals[i] = lval_copy(e->vals[i]);

    n->syms[i] = (char *)malloc(strlen(e->syms[i]) + 1);
    strcpy(n->syms[i], e->syms[i]);
  }

  return n;
}

void lval_print(lval *v);
void lenv_print(lenv *e) {
  for (int i = 0; i < e->count; i++) {
    printf("%s = ", e->syms[i]);
    lval_print(e->vals[i]);
    printf("\n");
  }
}

void lval_debug_print(lval *a) {
  printf("type: %s\n", ltype_name(a->type));
  if (a->type == LVAL_SYM) {
    printf("sym: %s\n", a->sym);
  }
  if (a->type == LVAL_NUM) {
    printf("num: %f\n", a->num);
  }

  /* if (a->env) { */
  /*   printf("env:---------\n"); */
  /*   lenv_print(a->env); */
  /* } */

  if (a->type == LVAL_FUN) {
    if (a->builtin) {
      printf("builtin function\n");
    } else {
      printf("formals:--------\n");
      lval_debug_print(a->formals);
      printf("body:--------\n");
      lval_debug_print(a->body);
    }
  }

  printf("number of children: %i\n", a->count);
  for (int i = 0; i < a->count; i++) {
    printf("\nChildren: [%i/%i]-----------------------\n", i, a->count);
    lval_debug_print(a->cell[i]);
  }
}

lval *lval_num(double n) {
  lval *v = (lval *)malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = n;
  return v;
}

lval *lval_err(char *fmt, ...) {
  lval *v = (lval *)malloc(sizeof(lval));
  v->type = LVAL_ERR;

  va_list va;
  va_start(va, fmt);

  v->err = (char *)malloc(512);

  vsnprintf(v->err, 511, fmt, va);

  v->err = (char *)realloc(v->err, strlen(v->err) + 1);

  va_end(va);
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

lval *lval_fun(lbuiltin func, char *name) {
  lval *v = (lval *)malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->builtin = func;
  v->sym = (char *)malloc(strlen(name) + 1);
  strcpy(v->sym, name);
  return v;
}

lval *lval_quit(void) {
  lval *v = (lval *)malloc(sizeof(lval));
  v->type = LVAL_QUIT;
  return v;
}

lval *lval_lambda(lval *formals, lval *body) {
  lval *v = (lval *)malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->builtin = NULL;
  v->env = lenv_new();
  v->formals = formals;
  v->body = body;
  return v;
}

lval *lval_add(lval *v, lval *x);
lval *builtin_eval(lenv *e, lval *a);
lval *lval_pop(lval *v, int i);
lval *builtin_list(lenv *e, lval *a);
lval *lval_call(lenv *e, lval *f, lval *a) {
  if (f->builtin) {
    return f->builtin(e, a);
  }

  int given = a->count;
  int total = f->formals->count;

  while (a->count) {
    if (f->formals->count == 0) {
      lval_del(a);
      return lval_err(
          (char *)"Function passed too many arguments. Got %i, Expected %i.",
          given, total);
    }

    lval *sym = lval_pop(f->formals, 0);

    if (strcmp(sym->sym, "&") == 0) {
      if (f->formals->count != 1) {
        lval_del(a);
        return lval_err((char *)"Function format invalid. Symbol '&' not "
                                "followed by single symbol");
      }
      lval *nsym = lval_pop(f->formals, 0);
      lenv_put(f->env, nsym, builtin_list(e, a));
      lval_del(sym);
      lval_del(nsym);
      break;
    }
    lval *val = lval_pop(a, 0);

    lenv_put(f->env, sym, val);

    lval_del(sym);
    lval_del(val);
  }

  lval_del(a);

  if (f->formals->count > 0 && strcmp(f->formals->cell[0]->sym, "&") == 0) {
    if (f->formals->count != 2) {
      return lval_err((char *)"Function format Invalid. Symbol '&' not "
                              "followed by single symbol.");
    }

    lval_del(lval_pop(f->formals, 0));

    lval *sym = lval_pop(f->formals, 0);
    lval *val = lval_qexpr();

    lenv_put(f->env, sym, val);
    lval_del(sym);
    lval_del(val);
  }

  if (f->formals->count == 0) {
    f->env->par = e;
    return builtin_eval(f->env, lval_add(lval_sexpr(), lval_copy(f->body)));
  } else {
    return lval_copy(f);
  }
}

void lval_del(lval *v) {
  switch (v->type) {
  case LVAL_NUM:
  case LVAL_QUIT:
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
  case LVAL_FUN:
    if (!v->builtin) {
      lenv_del(v->env);
      lval_del(v->formals);
      lval_del(v->body);
    } else {
      free(v->sym);
    }
    break;
  }

  free(v);
}

lval *lval_read_num(mpc_ast_t *t) {
  errno = 0;
  double x = strtod(t->contents, NULL);
  return errno != ERANGE ? lval_num(x)
                         : lval_err((char *)"invalid number, %f", x);
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
  case LVAL_FUN:
    if (!v->builtin) {
      printf("(\\ ");
      lval_print(v->formals);
      putchar(' ');
      lval_print(v->body);
      putchar(')');
    } else {
      printf("<builtin function: %s>", v->sym);
    }
    break;
  }
}

void lval_println(lval *v) {
  lval_print(v);
  putchar('\n');
}

lval *lval_take(lval *v, int i);
lval *lval_pop(lval *v, int i);
lval *lval_eval(lenv *e, lval *v);
lval *buitin(lval *a, char *func);
lval *lval_eval_sexpr(lenv *e, lval *v) {
  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lval_eval(e, v->cell[i]);
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
    lval *_v = lval_take(v, 0);

    return _v;
  }

  lval *f = lval_pop(v, 0);
  if (f->type != LVAL_FUN) {
    lval_del(f);
    lval_del(v);
    return lval_err((char *)"first element is not a function");
  }

  lval *result = lval_call(e, f, v);
  lval_del(f);
  return result;
}

lval *lval_eval(lenv *e, lval *v) {
  if (v->type == LVAL_SYM) {
    if (strcmp("print_env", v->sym) == 0) {
      lenv_print(e);
      lval_del(v);
      return lval_sexpr();
    }
    if (strcmp("exit", v->sym) == 0) {
      lval_del(v);
      return lval_quit();
    }
    lval *x = lenv_get(e, v);
    lval_del(v);
    return x;
  }
  if (v->type == LVAL_SEXPR) {
    return lval_eval_sexpr(e, v);
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

lval *builtin_op(lenv *e, lval *a, char *op) {
  for (int i = 0; i < a->count; i++) {
    /* printf("lval type: %i", a->cell[i]->type); */
    if (a->cell[i]->type != LVAL_NUM) {
      lval_del(a);
      return lval_err((char *)"Function '%s' passed incorrect type for "
                              "argument %i. Got %s, Expected %s",
                      op, i, ltype_name(a->cell[i]->type),
                      ltype_name(LVAL_NUM));
    }
  }

  lval *x = lval_pop(a, 0);

  if ((strcmp(op, "-") == 0) && a->count == 0) {
    x->num = -x->num;
  }

  while (a->count > 0) {
    lval *y = lval_pop(a, 0);

    if (strcmp(op, "+") == 0) {
      x->num += y->num;
    }
    if (strcmp(op, "-") == 0) {
      x->num -= y->num;
    }
    if (strcmp(op, "*") == 0) {
      x->num *= y->num;
    }
    if (strcmp(op, "/") == 0) {
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

lval *builtin_head(lenv *e, lval *a) {
  LASSERT_NUM((char *)"head", a, 1);
  LASSERT_TYPE((char *)"head", a, 0, LVAL_QEXPR);
  LASSERT_NOT_EMPTY((char *)"head", a, 0);

  lval *v = lval_take(a, 0);
  while (v->count > 1) {
    lval_del(lval_pop(v, 1));
  }

  return v;
}

lval *builtin_fst(lenv *e, lval *a) {
  lval *v = builtin_head(e, a);

  if (v->count) {
    v = lval_take(v, 0);
  }

  return v;
}

lval *builtin_tail(lenv *e, lval *a) {
  LASSERT_TYPE((char *)"tail", a, 0, LVAL_QEXPR);
  LASSERT_NUM((char *)"tail", a, 1);
  LASSERT_NOT_EMPTY((char *)"tail", a, 0);

  lval *v = lval_take(a, 0);
  lval_del(lval_pop(v, 0));
  return v;
}

lval *builtin_list(lenv *e, lval *a) {
  a->type = LVAL_QEXPR;
  return a;
}

lval *builtin_eval(lenv *e, lval *a) {
  LASSERT_NUM((char *)"eval", a, 1);
  LASSERT_TYPE((char *)"eval", a, 0, LVAL_QEXPR);

  lval *x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(e, x);
}

lval *lval_join(lval *x, lval *y) {
  while (y->count) {
    x = lval_add(x, lval_pop(y, 0));
  }

  lval_del(y);
  return x;
}

lval *builtin_join(lenv *e, lval *a) {
  for (int i = 0; i < a->count; i++) {
    LASSERT_TYPE((char *)"join", a, i, LVAL_QEXPR);
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
    LASSERT_TYPE((char *)"cons", a, i, LVAL_QEXPR);
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
  LASSERT_NUM((char *)"len", a, 1);
  LASSERT_TYPE((char *)"len", a, 0, LVAL_QEXPR);
  long len = 0;
  for (int i = 0; i < a->count; i++) {
    len = len + a->cell[i]->count;
  }
  return lval_num(len);
}

lval *builtin_init(lval *a) {
  LASSERT_NUM((char *)"init", a, 1);
  LASSERT_TYPE((char *)"init", a, 0, LVAL_QEXPR);

  lval *x = lval_qexpr();
  lval *arg = lval_pop(a, 0);

  while (arg->count > 1) {
    x = lval_add(x, lval_pop(arg, 0));
  }

  lval_del(a);

  return x;
}

lval *builtin_add(lenv *e, lval *a) { return builtin_op(e, a, (char *)"+"); }
lval *builtin_sub(lenv *e, lval *a) { return builtin_op(e, a, (char *)"-"); }
lval *builtin_mul(lenv *e, lval *a) { return builtin_op(e, a, (char *)"*"); }
lval *builtin_div(lenv *e, lval *a) { return builtin_op(e, a, (char *)"/"); }
lval *builtin_mod(lenv *e, lval *a) { return builtin_op(e, a, (char *)"%"); }
lval *builtin_min(lenv *e, lval *a) { return builtin_op(e, a, (char *)"min"); }
lval *builtin_max(lenv *e, lval *a) { return builtin_op(e, a, (char *)"max"); }
lval *builtin_var(lenv *e, lval *a, char *func) {
  LASSERT_TYPE(func, a, 0, LVAL_QEXPR);

  lval *syms = a->cell[0];

  for (int i = 0; i < syms->count; i++) {
    /* printf("cell[%i]: %i\n", i, syms->cell[i]->type); */
    LASSERT_TYPE(func, syms, i, LVAL_SYM);
  }

  /* printf("syms->count: %i\n", syms->count); */
  /* printf("a->count: %i\n", a->count); */

  LASSERT_NUM(func, syms, (a->count - 1));

  for (int i = 0; i < syms->count; i++) {
    if (strcmp(func, "def") == 0) {
      lenv_def(e, syms->cell[i], a->cell[i + 1]);
    }
    if (strcmp(func, "=") == 0) {
      /* i is syms, +1 to fetch corresponding value */
      /* a->cell: [syms 1 2] */
      /* syms->cell: [a b] */
      lenv_put(e, syms->cell[i], a->cell[i + 1]);
    }
  }

  lval_del(a);
  return lval_sexpr();
}
lval *builtin_def(lenv *e, lval *a) { return builtin_var(e, a, (char *)"def"); }
lval *builtin_put(lenv *e, lval *a) { return builtin_var(e, a, (char *)"="); }
lval *builtin_fun(lenv *e, lval *a) {
  /* lval_debug_print(a); */
  /* fun {a x y} {+ x y} */
  lval *args = lval_pop(a, 0);
  lval *name = lval_pop(args, 0);

  lval *body = lval_pop(a, 0);
  lval *lambda = lval_lambda(args, body);

  lenv_def(e, name, lambda);

  lval_del(name);
  lval_del(lambda);

  return lval_sexpr();
}

lval *builtin_lambda(lenv *e, lval *a) {
  LASSERT_NUM((char *)"\\", a, 2);
  LASSERT_TYPE((char *)"\\", a, 0, LVAL_QEXPR);
  LASSERT_TYPE((char *)"\\", a, 1, LVAL_QEXPR);

  for (int i = 0; i < a->cell[0]->count; i++) {
    LASSERT_TYPE("\\", a->cell[0], i, LVAL_SYM);
  }

  lval *formals = lval_pop(a, 0);
  lval *body = lval_pop(a, 0);

  lval_del(a);
  return lval_lambda(formals, body);
}

lval *builtin_ord(lenv *e, lval *a, char *op) {
  LASSERT_NUM(op, a, 2);
  LASSERT_TYPE(op, a, 0, LVAL_NUM);
  LASSERT_TYPE(op, a, 1, LVAL_NUM);

  int r;
  if (strcmp(op, ">") == 0) {
    r = (a->cell[0]->num) > (a->cell[1]->num);
  }
  if (strcmp(op, "<") == 0) {
    r = (a->cell[0]->num) < (a->cell[1]->num);
  }
  if (strcmp(op, ">=") == 0) {
    r = (a->cell[0]->num) >= (a->cell[1]->num);
  }
  if (strcmp(op, "<=") == 0) {
    r = (a->cell[0]->num) <= (a->cell[1]->num);
  }

  lval_del(a);
  return lval_num(r);
}

lval *builtin_gt(lenv *e, lval *a) { return builtin_ord(e, a, (char *)">"); }
lval *builtin_lt(lenv *e, lval *a) { return builtin_ord(e, a, (char *)"<"); }
lval *builtin_gteq(lenv *e, lval *a) { return builtin_ord(e, a, (char *)">="); }
lval *builtin_lteq(lenv *e, lval *a) { return builtin_ord(e, a, (char *)"<="); }

void lenv_add_builtin(lenv *e, char *name, lbuiltin func) {
  lval *k = lval_sym(name);
  lval *v = lval_fun(func, name);
  lenv_put(e, k, v);
  lval_del(k);
  lval_del(v);
}

void lenv_add_builtins(lenv *e) {
  lenv_add_builtin(e, (char *)"list", builtin_list);
  lenv_add_builtin(e, (char *)"head", builtin_head);
  lenv_add_builtin(e, (char *)"tail", builtin_tail);
  lenv_add_builtin(e, (char *)"eval", builtin_eval);
  lenv_add_builtin(e, (char *)"join", builtin_join);

  lenv_add_builtin(e, (char *)"+", builtin_add);
  lenv_add_builtin(e, (char *)"add", builtin_add);
  lenv_add_builtin(e, (char *)"-", builtin_sub);
  lenv_add_builtin(e, (char *)"sub", builtin_sub);
  lenv_add_builtin(e, (char *)"*", builtin_mul);
  lenv_add_builtin(e, (char *)"mul", builtin_mul);
  lenv_add_builtin(e, (char *)"/", builtin_div);
  lenv_add_builtin(e, (char *)"div", builtin_div);
  lenv_add_builtin(e, (char *)"%", builtin_mod);

  lenv_add_builtin(e, (char *)"min", builtin_min);
  lenv_add_builtin(e, (char *)"max", builtin_max);

  lenv_add_builtin(e, (char *)"def", builtin_def);
  lenv_add_builtin(e, (char *)"\\", builtin_lambda);
  lenv_add_builtin(e, (char *)"=", builtin_put);
  lenv_add_builtin(e, (char *)"fst", builtin_fst);
  lenv_add_builtin(e, (char *)"fun", builtin_fun);
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

  lenv *e = lenv_new();
  lenv_add_builtins(e);

  /* not 0, infinit loop */
  while (1) {
    mpc_result_t r;
    char *input = readline("lispy> ");
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      mpc_ast_t *ast = (mpc_ast_t *)r.output;

      lval *x = lval_eval(e, lval_read(ast));
      if (x->type == LVAL_QUIT) {
        break;
      }
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

  lenv_del(e);

  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
  return 0;
}
