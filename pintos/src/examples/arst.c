#include <stdio.h>
#include <syscall.h>

int
main (int argc, char **argv)
{
  int i;
  int count = printf ("I AM SUPERMAN\n");
  printf("That was %d characters.\n", count);
  if (argc < 3)
	  return EXIT_SUCCESS;

  char * name = argv[1];
  char * name2 = argv[2];

  bool creat = create(name, 1);
  if (creat)
	  printf("Create Success!\n");
  else
	  printf("No create sucess...\n");

  int fd1 = open(name);
  int fd2 = open(name2);
  printf("Open returned %d.\n", fd1);
  printf("Open returned %d.\n", fd2);

  close(fd1);
  close(fd2);

  printf("Open returned %d.\n", open(name));
  printf("Open returned %d.\n", open(name2));

  printf("Remove was a %d\n", remove(name));
  return EXIT_SUCCESS;
}
