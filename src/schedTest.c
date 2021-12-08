// Test program for CS333 scheduler, project 4.
#ifdef CS333_P4
#include "types.h"
#include "user.h"

#define PrioCount 6
#define numChildren 60

void
countForever(int i)
{
  int j, p, rc;
  unsigned long count = 0;

  j = getpid();
  p = i%PrioCount;
  rc = setpriority(j, p);
  if (rc == 0)
    printf(1, "%d: start prio %d\n", j, p);
  else {
    printf(1, "setpriority failed. file %s at %d\n", __FILE__, __LINE__);
    exit();
  }

  while (1) {
    count++;
    if ((count & (0x1FFFFFFF)) == 0) {
      p = (p+1)% PrioCount;
      rc = setpriority(j, p);
      if (rc == 0)
         printf(1, "%d: new prio %d\n", j, p);
      else {
         printf(1, "setpriority failed. file %s at %d\n", __FILE__, __LINE__);
         exit();
      }
    }
  }
}

int
main(void)
{
  int i, rc;
  int j = getpid();
  if(MAXPRIO == 0){
    if(setpriority(j, -1) == -1)
    {
      printf(2, "Setting to -1 Failed\n");
    }
    printf(2, "Process %d's Prio is %d\n", j, getpriority(j));
    if(setpriority(j, 0) == -1)
    {
      printf(2, "Setting to -1 Failed\n");
    }
    else
    {
      printf(2, "Setting to 0 Succeeded\n");
    }
    printf(2, "Process %d's Prio is %d\n", j, getpriority(j));
    if(setpriority(j, 1 == 1) == -1)
    {
      printf(2, "Setting to 1 Failed\n");
    }
    printf(2, "Process %d's Prio is %d\n", j, getpriority(j));
    exit();
  }
  printf(2, "GETPRIORITY TEST\n");
  printf(2, "Current Process:\nProcess %d's Prio is %d\n", j, getpriority(j));
  printf(2, "Previous Process:\nProcess %d's Prio is %d\n", j - 1, getpriority(j));
  j = -1;
  if(getpriority(j) == -1)
  {
    printf(2, "Invalid PID of %d\n",j);  
  }
  j = 25;
  if(getpriority(j) == -1)
  {
    printf(2, "Invalid PID of %d\n",j);
  }

  printf(2, "SETPRIORITY TEST\n");
  if(setpriority(j, 0) == -1)
  {
    printf(2, "Setting %d to 0 Failed\n", j);
  }
  else
  {
    printf(2, "Setting %d to 0 Succeeded\n", j);
  }
  int res = getpriority(j);
  if(res == -1)
  {
    printf(2, "Getting %d Failed\n", j);
  }
  else
  {
    printf(2, "Getting %d Succeeded\n", j);
  }
  printf(2, "Press C-R and/or C-P\n");
  sleep(200);
  for (i=0; i<numChildren; i++) {
    rc = fork();
    if (!rc) { // child
      countForever(i);
    }
  }
  // what the heck, let's have the parent waste time as well!
  countForever(1);
  exit();
}
#endif
