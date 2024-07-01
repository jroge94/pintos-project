/* Tests writing to standard output. */

#include "tests/lib.h"
#include "tests/main.h"
#include <string.h>

void
test_main (void)
{
  char *msg = "I love CS4103\n";
  write (1, msg, strlen (msg));
}
