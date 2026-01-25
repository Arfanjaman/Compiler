#include <stdio.h>
#define COALESCE(a,b) ((a)!=0 ? (a) : (b))

int main() {
  int x = 5;
  printf("x=%d\n", x);
  if (x > 0 && !(x == 2)) {
    printf("inside if\n");
  }
  return 0;
}
// this is  comment
/* this is a multi line comment*/