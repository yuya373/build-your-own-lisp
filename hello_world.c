#include <math.h>
#include <stdio.h>

typedef struct {
  float x;
  float y;
} point;

int add(int x, int y) {
  int ret = x + y;
  return ret;
}

void mod(point *p) {
  p->x = 99;
  p->y = 99;
}

void no_mod(point p) {
  p.x = 9;
  p.y = 9;
}

int main(int argc, char **argv) {
  puts("Hello, world!");
  int x = 1;
  int y = 2;
  printf("%d + %d = %d\n", x, y, add(x, y));
  point p;
  p.x = 0.1f;
  p.y = 10.0f;
  float length = sqrt(p.x * p.x + p.y * p.y);
  printf("point.x: %f, point.y: %f, length: %f\n", p.x, p.y, length);
  mod(&p);
  printf("point.x: %f, point.y: %f, length: %f\n", p.x, p.y, length);
  no_mod(p);
  printf("point.x: %f, point.y: %f, length: %f\n", p.x, p.y, length);
  if (p.x > length) {
    puts("x is greater than length");
  } else {
    puts("x is less then or equal to length");
  }
  if (NULL == 0) {
    puts("null == 0 !!");
  }
  if (1) {
    puts("1");
  }
  int i = 10;
  while (i > 0) {
    puts("Loop Iteration");
    i = i - 1;
  }
  for (int i = 0; i < 10; i++) {
    printf("for loop: %d\n", i);
  }
  for (int i = 0; i < 5; i++) {
    puts("Hello World");
  }
  puts("----------------------------");
  int hello_i = 5;
  while (hello_i > 0) {
    puts("Hello World");
    hello_i = hello_i - 1;
  }
  int s = 3;
  switch (s) {
  case 1:
    puts("switch: 1");
  case 2:
    puts("switch: 2");
  case 3:
    puts("switch: 3");
    break;
  default:
    puts("switch: default");
  }
  return 0;
}
