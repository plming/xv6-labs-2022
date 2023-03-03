#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int ticks;

  if(argc != 2)
  {
    fprintf(2, "usage: sleep ticks\n");
    exit(1);
  }

  ticks = atoi(argv[1]);
  sleep(ticks);

  exit(0);
}
