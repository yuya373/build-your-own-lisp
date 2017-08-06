#include <stdio.h>

/* input buffer */
/* global array of 2048 characters */
/* static keyword makess this variable local to this file */
static char input[2048];

int main(int argc, char **argv) {
  puts("Lispy Version 0.0.0.0.1");
  puts("Press Ctrl+c to Exit\n");

  /* not 0, infinit loop */
  while (1) {
    /* http://en.cppreference.com/ */
    fputs("lispy> ", stdout);
    fgets(input, 2048, stdin);

    printf("%s", input);
  }

  return 0;
}
