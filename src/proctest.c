#ifdef CS333_P2
#include "types.h"
#include "user.h"
#include "uproc.h"
#include "pdx.h"

int
main(int argc, char *argv[])
{
  int pid = 0;
  unsigned long x = 0;
  printf(1, "\n----------\nRunning GetProcs Test\n----------\n");
  printf(1, "Filling the proc[] array withs forks of this Process\n");
  //Rest of this test pulled from CS333 handout https://web.cecs.pdx.edu/~markem/CS333/handouts/loopforever.c
  for (int i=0; i<NPROC; i++) {
    sleep(5*TPS);  // pause before each child starts
    pid = fork();
    if (pid < 0) {
      printf(2, "Fork Failed! End of Filling!\n");
      exit();
    }

    if (pid == 0) { // child
      sleep(getpid()*TPS); // stagger start
      do {
        x += 1;
      } while (1);
      printf(1, "Child %d exiting\n", getpid());
      exit();
    }
  }

  pid = fork();
  if (pid == 0) {
    sleep(2);
    do {
      x = x+1;
    } while (1);
  }

  sleep(15*TPS);
  wait();
  printf(1, "\n** End of Tests **\n");
  exit();
}

#endif

