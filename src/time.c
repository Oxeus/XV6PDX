#ifdef CS333_P2
#include "types.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  uint start_time = uptime();
  uint end_time = 0;
  uint total_time = 0;
  uint sec = 0;
  uint mill = 0;

  int pid = fork();
  //Child
  if(pid == 0)
  {
    exec(argv[1],argv+1);
    exit();
  }
  //invalid
  else if(pid < 0)
  {
    exit();
  }
  //wait for child
  else
  {
    wait(); 
  }
 
  end_time = uptime();

  total_time = end_time - start_time;
  sec = total_time / 1000;
  mill = total_time % 1000;
  printf(1, "%s ran in %d",argv[1], sec);
  if(mill < 10)
  {
    printf(1, ".00%d seconds\n", mill);
  }
  else if(mill >= 10 && mill < 100)
  {
    printf(1, ".0%d seconds\n", mill);
  }
  else
  {
    printf(1, ".%d seconds\n", mill);
  }
  exit();
}
#endif

