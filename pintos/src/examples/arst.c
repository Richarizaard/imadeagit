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

  bool creat = create(name, 29);
  if (creat)
	  printf("Create Success!\n");
  else
	  printf("Noh cree ate sucess...\n");

  int fd1 = open(name);
  printf("Open returned %d.\n", fd1);
  
  printf("Writing 'Interior crocodile alligator' to file. %d bytes.\n", write(fd1, "Interior crocodile alligator", 28));
  
  printf("Filezize returned %d.\n", filesize(fd1));
  
  printf("fd1 previous position: %d\n", tell(fd1));
  seek(fd1, 13);
  printf("fd1 next position: %d\n", tell(fd1));
  write(fd1, "Chevrolet", 9);

  close(fd1);
  
  printf("Filezize returned %d.\n", filesize(fd1));

  int fd3 = open(name);
  int fd4 = open(name);
  
  printf("Open returned %d.\n", fd3);
  printf("Open returned %d.\n", fd4);
  
  remove(fd4);
  printf("Remove was a %d\n", remove(name));
  
  char buffGuy[28];
  int readCount = read(fd3, buffGuy, 28);
  printf("Reading from file :: %d bytes read %s\n", readCount, buffGuy);
  return EXIT_SUCCESS;
}
