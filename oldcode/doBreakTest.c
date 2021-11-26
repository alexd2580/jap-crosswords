#include<stdio.h>

int main(void)
{

  printf("TEST\n");
  fflush(stdout);
    
  for(int i=0; i<10; i++)
  {
    printf("TESTI\n");
    if(i == 5) goto otherwise;
  }
  
  printf("TEST1\n");
  fflush(stdout);
  
  printf("TEST2\n");
  fflush(stdout);

otherwise:

  printf("TEST3\n");
  fflush(stdout);
  
  return 0;
}
