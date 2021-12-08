#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#ifdef PDX_XV6
#include "pdx-kernel.h"
#endif // PDX_XV6
#ifdef CS333_P2
#include "uproc.h"
#endif

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      return -1;
    }
    sleep(&ticks, (struct spinlock *)0);
  }
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  xticks = ticks;
  return xticks;
}

#ifdef PDX_XV6
// shutdown QEMU
int
sys_halt(void)
{
  do_shutdown();  // never returns
  return 0;
}
#endif // PDX_XV6

#ifdef CS333_P1
//Following the code that was in the project description a struct is created of rtcdate, the argptr checks the
//pointer and then on return checks if the pointer is valid. If it is not valid then -1 is returned. If valid,
//the pointer is updated with info provided by cmostime and then 0 is returned
int 
sys_date(void)
{
  struct rtcdate *d;
  if(argptr(0, (void*)&d, sizeof(struct rtcdate)) < 0)
    return -1;
  cmostime(d);
  return 0;
}
#endif // CS333_P1

#ifdef CS333_P2
uint
sys_getuid(void)
{
  return getuid();
}

uint
sys_getgid(void)
{
  return getgid();
}

uint
sys_getppid(void)
{
  return getppid();
}

int
sys_setuid(void)
{
  int n;
  if((argint(0, &n) < 0) || (n < 0 || n > 32767))
  {
    return -1;
  }
  else
  {
    setuid(n);
  }
  return 0;
}

int
sys_setgid(void)
{
  int n;
  if((argint(0, &n) < 0) || (n < 0 || n > 32767))
  {
    return -1;
  }
  else
  {
    setgid(n);
  }
  return 0;
}

int 
sys_getprocs(void)
{
  int n;
  struct uproc *t;
  if((argint(0, &n) < 0) || (argptr(1, (void*)&t, n) < 0))
  {
    return -1;
  }
  return getprocs(n, t);
}
#endif // CS333_P2

#ifdef CS333_P4
int 
sys_setpriority(void)
{
  int pid = 0;
  int prior = 0;
  if((argint(0, &pid) < 0) || (argint(1, &prior) < 0))
  {
    return -1;
  }
  if(prior < 0 || prior > MAXPRIO)
  {
    return -1;
  }
  return setpriority(pid, prior);
}

int 
sys_getpriority(void)
{
  int pid = 0;
  if(argint(0, &pid) < 0)
  {
    return -1;
  }
  if(pid < 0)
  {
    return -1;
  }
  return getpriority(pid);
}
#endif
