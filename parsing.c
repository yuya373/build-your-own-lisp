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

enum ERRORS { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };
enum LVAL { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };

/* reference to itself in struct, need struct name */
/* must not contain its own type directly, contain pointer to its own type */
/* struct size grow infinite when contain its own type directly */
typedef struct lval {
  int type;
  double num;
  char *err;
  char *sym;
  int count;
  /* pointer to (a list of (pointer to lval)) */
  struct lval **cell;
} lval;

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

void print_ast(mpc_ast_t *ast) {
  printf("Tag: %s\n", ast->tag);
  printf("Contents: %s\n", ast->contents);
  printf("pos: %ld\n", ast->state.pos);
  printf("row: %ld\n", ast->state.row);
  printf("col: %ld\n", ast->state.col);
  printf("Number of children: %i\n", ast->children_num);
  if (ast->children_num > 0) {
    for (int i = 0; i < ast->children_num; i++) {
      printf("%i child\n", i);
      print_ast(ast->children[i]);
    }
  }
}

int number_of_nodes(mpc_ast_t *ast) {
  if (ast->children_num == 0) {
    return 1;
  }
  if (ast->children_num >= 1) {
    int total = 1;
    for (int i = 0; i < ast->children_num; i++) {
      total = total + number_of_nodes(ast->children[i]);
    }
    return total;
  }
  return 0;
}

long number_of_leaves(mpc_ast_t *t) {
  if (strstr(t->tag, "number") || strstr(t->tag, "operator")) {
    return 1;
  }
  if (t->children_num >= 1) {
    int n = 0;
    for (int i = 0; i < t->children_num; i++) {
      n = n + number_of_leaves(t->children[i]);
    }
    return n;
  }
  return 0;
}

long number_of_branches(mpc_ast_t *t) {
  if (strstr(t->tag, "operator")) {
    return 1;
  }
  if (t->children_num >= 1) {
    int n = 0;
    for (int i = 0; i < t->children_num; i++) {
      n = n + number_of_branches(t->children[i]);
    }
    return n;
  }
  return 0;
}

lval *lval_take(lval *v, int i);
lval *lval_pop(lval *v, int i);
lval *lval_eval(lval *v);
lval *builtin_op(lval *a, char *op);
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

  lval *result = builtin_op(v, f->sym);
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

int main(int argc, char **argv) {
  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Symbol = mpc_new("symbol");
  mpc_parser_t *Sexpr = mpc_new("sexpr");
  mpc_parser_t *Qexpr = mpc_new("qexpr");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");
  mpca_lang(MPCA_LANG_DEFAULT, "number : /-?[0-9]+[\\.]?[0-9]*/ ; \
             symbol: '^' | '+' | '-' | '*' | '/' | '%' | \
                     \"list\" | \"head\" | \"tail\" | \"join\" | \"eval\" | \
                     \"add\" | \"sub\" | \"mul\" | \"div\" | \"min\" | \"max\" ; \
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
