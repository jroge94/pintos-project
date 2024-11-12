/* seek-normal.c

   Tests the 'seek' system call by writing to a specific position in a file
   and verifying the file content.
*/
#include "tests/lib.h"
#include "tests/main.h"
#include <stdio.h>
#include <string.h>
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

  /* Write initial content */
  write(fd, "Hello, PintOS!", 14);

  /* Seek to position 7 */
  seek(fd, 7);

  /* Overwrite content at position 7 */
  write(fd, "World", 5);

  /* Seek back to beginning */
  seek(fd, 0);

  /* Read the entire content */
  char buffer[20];
  int bytes_read = read(fd, buffer, sizeof buffer - 1);
  if (bytes_read < 0) {
    printf("Failed to read from %s\n", filename);
    close(fd);
    return;
  }
  buffer[bytes_read] = '\0'; /* Null-terminate the buffer */

  /* Output the final content */
  printf("File content: %s\n", buffer); /* Expected: "Hello, World!" */
  close(fd);

  /* Indicate test pass */
  printf("seek-normal: pass\n");
}
