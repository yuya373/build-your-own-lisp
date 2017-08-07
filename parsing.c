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

int main(int argc, char **argv) {
  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Operator = mpc_new("operator");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");
  mpca_lang(MPCA_LANG_DEFAULT, "number : /-?[0-9]+/ ; \
             operator: '+' | '-' | '*' | '/' | '%' | /(add|sub|mul|div)/ ; \
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
      mpc_ast_print((mpc_ast_t *)r.output);
      mpc_ast_delete((mpc_ast_t *)r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    add_history(input);

    puts(input);
    /* from stdlib.h */
    free(input);
  }

  mpc_cleanup(4, Number, Operator, Expr, Lispy);
  return 0;
}
