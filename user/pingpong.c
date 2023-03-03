#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
  /* constants */
  const int stderr = 2;
  const char byte = 0xFF;

  /* file descriptors for pipes */
  int parent_fd[2];
  int child_fd[2];

  char buf;
  int pid;
  
  /* create pipe */
  pipe(parent_fd);
  pipe(child_fd);

  pid = fork();
  if(pid > 0)
  {
    /* parent segment */
    write(child_fd[1], &byte, sizeof(byte));
    read(parent_fd[0], &buf, sizeof(buf));
    if(byte == buf)
    {
      printf("%d: received pong\n", getpid());
    }
    else
    {
      fprintf(stderr, "pong failed\n");
      exit(1);
    }
  }
  else if(pid == 0)
  {
    /* child segment */
    read(child_fd[0], &buf, sizeof(buf));
    if(byte != buf)
    {
      fprintf(stderr, "ping failed\n");
      exit(1);
    }

    printf("%d: received ping\n", getpid());
    write(parent_fd[1], &byte, sizeof(byte));
  }
  else
  {
    fprintf(stderr, "fork error\n");
  }

  exit(0);
}
