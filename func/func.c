#include <stdio.h>
int foo() {
  printf("ASUMI\n");
  return 0;
}
int bar(int x, int y) {
  printf("%d\n", x + y);
  return 0;
}
int bar2(int x, int y, int z) {
  printf("%d + %d + %d = %d\n", x, y, z, x + y + z);
  return 0;
}