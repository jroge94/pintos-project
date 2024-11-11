#include <stdio.h>
#include <syscall.h>

int main(void) {
  write(STDOUT_FILENO, "Hello, World!\n", 14);
  exit(0);
  return 0;
}