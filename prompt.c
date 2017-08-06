#include <stdio.h>
#include <stdlib.h>
/* yaourt libedit */
/* cc -std=c99 -Wall -ledit prompt.c -o prompt */
#include <editline/readline.h>

int main(int argc, char **argv) {
  puts("Lispy Version 0.0.0.0.1");
  puts("Press Ctrl+c to Exit\n");

  /* not 0, infinit loop */
  while (1) {
    /* http://en.cppreference.com/ */
    char *input = readline("lispy> ");
    add_history(input);

    puts(input);
    /* from stdlib.h */
    free(input);
  }

  return 0;
}
