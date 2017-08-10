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

long eval_op(long acc, char *op, long n) {
  if (strcmp(op, "add") == 0) {
    return eval_op(acc, (char *)"+", n);
  }
  if (strcmp(op, "sub") == 0) {
    return eval_op(acc, (char *)"-", n);
  }
  if (strcmp(op, "mul") == 0) {
    return eval_op(acc, (char *)"*", n);
  }
  if (strcmp(op, "div") == 0) {
    return eval_op(acc, (char *)"/", n);
  }
  if (strcmp(op, "+") == 0) {
    return acc + n;
  }
  if (strcmp(op, "-") == 0) {
    return acc - n;
  }
  if (strcmp(op, "*") == 0) {
    return acc * n;
  }
  if (strcmp(op, "/") == 0) {
    return acc / n;
  }
  if (strcmp(op, "%") == 0) {
    return acc % n;
  }
  if (strcmp(op, "^") == 0) {
    long ret = acc;
    for (int i = 1; i < n; i++) {
      ret = ret * acc;
    }
    return ret;
  }
  return 0;
}

long eval(mpc_ast_t *t) {
  if (strstr(t->tag, "number")) {
    return atoi(t->contents);
  }

  char *op = t->children[1]->contents;
  long x = eval(t->children[2]);

  int i = 3;
  while (strstr(t->children[i]->tag, "expr")) {
    x = eval_op(x, op, eval(t->children[i]));
    i++;
  }

  return x;
}

int main(int argc, char **argv) {
  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Operator = mpc_new("operator");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");
  mpca_lang(MPCA_LANG_DEFAULT, "number : /-?[0-9]+[\\.]?[0-9]*/ ; \
             operator: '^' | '+' | '-' | '*' | '/' | '%' | /(add|sub|mul|div)/ ; \
             expr: <number> | '(' <operator> <expr>+ ')' ; \
             lispy: /^/ <operator> <expr>+ /$/ ; \
            ",
            Number, Operator, Expr, Lispy);

  puts("Lispy Version 0.0.0.0.1");
  puts("Press Ctrl+c to Exit\n");

  /* not 0, infinit loop */
  while (1) {
    mpc_result_t r;
    char *input = readline("lispy> ");
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      /* mpc_ast_print((mpc_ast_t *)r.output); */
      mpc_ast_t *ast = (mpc_ast_t *)r.output;
      /* print_ast(ast); */
      /* printf("Number of nodes: %i\n", number_of_nodes(ast)); */
      /* printf("Number of leaves: %li\n", number_of_leaves(ast)); */
      /* printf("Number of branches: %li\n", number_of_branches(ast)); */
      printf("%li\n", eval(ast));
      mpc_ast_delete((mpc_ast_t *)r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    add_history(input);

    /* puts(input); */
    /* from stdlib.h */
    free(input);
  }

  mpc_cleanup(4, Number, Operator, Expr, Lispy);
  return 0;
}
