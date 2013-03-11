/*
 * Userland helloworld
 */

#include "tests/lib.h"

static const size_t BUFFER_SIZE = 20;

int main(void)
{
  char *name;
  //int count;
  //heap_init();
  puts("Malloc test!\n");
/*
  while (1) {
    name = (char*)malloc(BUFFER_SIZE);
    name = "hej";
    printf("%s \n", name);
    //free(name);
  }
*/
  name = (char*)malloc(BUFFER_SIZE);
  name = (char*)malloc(BUFFER_SIZE);
  name = (char*)malloc(BUFFER_SIZE);
  name = (char*)malloc(BUFFER_SIZE);
  name = (char*)malloc(BUFFER_SIZE);
  name = (char*)malloc(BUFFER_SIZE);
  name = (char*)malloc(BUFFER_SIZE);
  name = name;
  puts("Now I shall exit!\n");
  syscall_exit(2);
  return 0;
}
