#include <stdio.h>
#include <syscall.h>

int
main (int argc, char **argv)
{
  int i;
  int count = printf ("I AM SUPERMAN\n");
  printf("That was %d characters.\n", count);
  if (argc > 1)
  {
    int t = exec("arst");
    printf("Thread id %d\n", t);
  }
  return EXIT_SUCCESS;
}
