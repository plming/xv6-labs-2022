#include "kernel/types.h"
#include "kernel/fs.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

const char*
get_file_name(const char *path)
{
  const char *p;

  for(p = path + strlen(path); p >= path && *p != '/'; --p)
    ;
  ++p;

  return p;
}

void
find_recursive(const char *path, const char *expr)
{
  char buf[512];
  char *p;
  int fd;
  struct stat st;
  struct dirent de;

  fd = open(path, O_RDONLY);
  if(fd < 0)
  {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0)
  {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type)
  {
  case T_DEVICE:
    /* intentional fallthrough */
  case T_FILE:
    if(strcmp(get_file_name(path), expr) == 0)
    {
      printf("%s\n", path);
    }
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf))
    {
      fprintf(2, "find: path too long\n");
      break;
    }

    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de))
    {
      if(de.inum == 0)
      {
        continue;
      }

      /* except current and parent directory */
      if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
      {
        continue;
      }

      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = '\0';
      find_recursive(buf, expr);
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  if(argc != 3)
  {
    fprintf(2, "usage: find path filename\n");
    exit(1);
  }

  find_recursive(argv[1], argv[2]);
  exit(0);
}
