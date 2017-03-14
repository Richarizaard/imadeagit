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

  bool creat = create(name, 29);
  if (creat)
	  printf("Create Success!\n");
  else
	  printf("No create sucess...\n");

  int fd1 = open(name);
  int fd2 = open(name2);
  printf("Open returned %d.\n", fd1);
  printf("Open returned %d.\n", fd2);
  
  printf("Writing 'Interior crocodile alligator' to file\n", write(fd1, "Interior crocodile alligator", 28));
  
  char buffGuy[28];
  read(fd1, buffGuy, 28);
  printf("Reading from file :: %s\n", buffGuy);
  
  printf("Filezize returned %d.\n", filesize(fd1));

  close(fd1);
  close(fd2);
  
  printf("Filezize returned %d.\n", filesize(fd1));

  printf("Open returned %d.\n", open(name));
  printf("Open returned %d.\n", open(name2));

  printf("Remove was a %d\n", remove(name));
  return EXIT_SUCCESS;
}
