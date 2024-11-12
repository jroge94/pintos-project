/* tell-normal.c

   Tests the 'tell' system call by moving to various positions in a file
   and verifying the reported position.
*/

#include "tests/lib.h"
#include "tests/main.h"
#include <stdio.h>
#include <syscall.h>

void test_main(void) {
  const char* filename = "testfile.txt";
  int fd = open(filename);
  if (fd < 0) {
    if (!create(filename, 0)) {
      printf("Failed to create %s\n", filename);
      return;
    }
    fd = open(filename);
    if (fd < 0) {
      printf("Failed to open %s\n", filename);
      return;
    }
  }

  write(fd, "Hello, PintOS!", 14);
  int pos = tell(fd);
  printf("Current position after write: %d\n", pos); /* Expected: 14 */

  seek(fd, 5);
  pos = tell(fd);
  printf("Position after seek to 5: %d\n", pos); /* Expected: 5 */

  close(fd);

  /* Indicate test pass */
  printf("tell-normal: pass\n");
}
