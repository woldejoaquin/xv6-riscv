#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
  int pid = getpid();
  int ppid = getppid();

  printf("Mi PID es: %d\n", pid);
  printf("El PID de mi padre es: %d\n", ppid);

  exit(0);
}
