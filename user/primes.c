#include "kernel/types.h"
#include "user/user.h"

void
sieve(int left_read_fd, int right_write_fd, int prime)
{
  int n;

  while(read(left_read_fd, &n, sizeof(n)) != 0)
  {
    if(n % prime != 0)
    {
      write(right_write_fd, &n, sizeof(n));
    }
  }
}

int
main(void)
{
  int left[2];
  int pid;

  if(pipe(left) != 0)
  {
    fprintf(2, "primes: create left pipe failed\n");
    exit(1);
  }

  pid = fork();
  if(pid == 0)
  {
    int i;

    /* feed numbers */
    for(i = 2; i < 35; ++i)
    {
      write(left[1], &i, sizeof(i));
    }
    
    exit(0);
  }
  else if(pid < 0)
  {
    fprintf(2, "primes: feed failed\n");
  }

  while(1)
  {
    int right[2];
    int prime;
    int num_bytes;

    close(left[1]);

    num_bytes = read(left[0], &prime, sizeof(prime));
    if(num_bytes == 0)
    {
      break;
    }

    printf("prime %d\n", prime);

    if(pipe(right) != 0)
    {
      fprintf(2, "primes: create right pipe failed\n");
      exit(1);
    }

    pid = fork();
    if(pid > 0)
    {
      close(left[0]);

      /* shift pipe */
      left[0] = right[0];
      left[1] = right[1];
    }
    else if(pid == 0)
    {
      sieve(left[0], right[1], prime);
      exit(0);
    }
    else
    {
      fprintf(2, "primes: fork failed\n");
      exit(1);
    }
  }

  exit(0);
}
