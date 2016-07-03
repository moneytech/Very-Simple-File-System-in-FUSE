#include <stdio.h>
#include <iostream>
#include <string.h>


int main(int argc, char const *argv[]) {

  FILE * file_ptr;
  char temp;
  char file_address[128];


  strcpy (file_address, argv[1]);
  // struct timeval diff, startTV, endTV;
  // gettimeofday(&startTV, NULL);

  for (int j = 0; j < 200; j++) {

    file_ptr = fopen(file_address, "w");
    // file_ptr = fopen("/home/udaysavaria/Documents/test.txt", "w");

    for (int i = 0; i < 4096 ; i++)
    {
      fprintf(file_ptr, "%c",(char) (((i + j) % 26) + 65));
	if (i % 8 == 0)
		fflush(file_ptr);
    }

    fclose(file_ptr);

    file_ptr = fopen(file_address, "r");
    // file_ptr = fopen("/home/udaysavaria/Documents/test.txt", "r");

    for (int i = 0; i < 4096; i++)
    {
      fscanf(file_ptr, "%c", &temp);

      if (temp != (char) (((i + j) % 26) + 65))
        printf("not Equal %c\n", temp);
    }

  fclose(file_ptr);
  }

  return 0;
}
