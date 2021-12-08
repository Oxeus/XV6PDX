
#ifdef CS333_P2
#include "types.h"
#include "user.h"
#include "uproc.h"
#include "pdx.h"

int
main(int argc, char *argv[])
{
  uint max = 0;
  if(argc < 2)
  {
    max = NPROC;
  }
  else
  {
    max = atoi(argv[1]);
  }
  struct uproc *table = malloc(max * sizeof(struct uproc));
  int size = getprocs(max, table);
  int spacer = 0;
  int sec = 0;
  int mill = 0;
  int c_sec = 0;
  int c_mill = 0;
#ifdef CS333_P4
  printf(1, "%s", "PID\tNAME         UID\tGID\tPPID\tPRIO\tELAPSED\tCPU\tSTATE\tSIZE\n");
#else
  printf(1, "%s", "PID\tNAME         UID\tGID\tPPID\tELAPSED\tCPU\tSTATE\tSIZE\n");
#endif
  for(int i = 0; i < size; ++i)
  { 
    printf(1, "%d\t%s", table[i].pid, table[i].name);
    spacer = 13 - strlen(table[i].name);
    for(int i = 0; i < spacer; ++i)
    {
      printf(1, " "); 
    }
#ifdef CS333_P4
    printf(1, "%d\t\t%d\t%d\t%d\t", table[i].uid, table[i].gid, table[i].ppid, table[i].priority);
#else 
    printf(1, "%d\t\t%d\t%d\t", table[i].uid, table[i].gid, table[i].ppid);
#endif
    sec = table[i].elapsed_ticks / 1000;
    mill = table[i].elapsed_ticks % 1000;
    c_sec = table[i].CPU_total_ticks / 1000;
    c_mill = table[i].CPU_total_ticks % 1000;
    if(mill < 10)
    {
      printf(1, "%d.00%d\t", sec, mill);
    }
    else if(mill >= 10 && mill < 100)
    {
      printf(1, "%d.0%d\t", sec, mill);
    }
    else
    {
      printf(1, "%d.%d\t", sec, mill);
    }
    if(c_mill < 10)
    {
      printf(1, "%d.00%d\t", c_sec, c_mill);
    }
    else if(c_mill >= 10 && c_mill < 100)
    { 
      printf(1, "%d.0%d\t", c_sec, c_mill);
    }
    else
    { 
      printf(1, "%d.%d\t", c_sec, c_mill);
    }
    printf(1, "%s\t%d\n", table[i].state, table[i].size); 
  }
  free(table);
  exit();
}
#endif

