#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

struct stack
{
  char *args[MAXARG];
  int top;
};

void
push_arg(struct stack *stack, char *arg)
{
  stack->args[stack->top] = arg;
  ++(stack->top);
}

int
main(int argc, char *argv[])
{
  struct stack s;
  int extra_args_top;
  char buf[512];
  int i = 0;

  if(argc < 2)
  {
    fprintf(2, "usage: xargs utility args...\n");
    exit(1);
  }

  /* arguments[0] is command */
  push_arg(&s, argv[1]);

  /* arguments[1..n] is command arguments */
  for(i = 2; i < argc; ++i)
  {
    push_arg(&s, argv[i]);
  }

  i = 0;
  extra_args_top = s.top;
  while(read(0, buf + i, sizeof(char)) != 0)
  {
    if(buf[i] != '\n')
    {
      ++i;
    }
    else /* found new line */
    {
      int pid;

      buf[i] = '\0';
      push_arg(&s, buf);

      pid = fork();
      if(pid > 0)
      {
        wait(0);

        /* reset buf index */
        i = 0;

        /* reset stack */
        s.top = extra_args_top;
      }
      else if(pid == 0)
      {
        /* last argument must be null */
        push_arg(&s, 0);

        exec(argv[1], s.args);

        /* exec failed */
        fprintf(2, "xargs: exec failed\n");
        exit(1);
      }
      else
      {
        fprintf(2, "xargs: fork failed\n");
        exit(1);
      }
    }
  }
  exit(0);
}
