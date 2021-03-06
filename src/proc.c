#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#ifdef CS333_P2
#include "uproc.h"
#endif // CS333_P2
#ifdef CS333_P3
#define statecount NELEM(states)
#endif // CS333_P3

static char *states[] = {
[UNUSED]    "unused",
[EMBRYO]    "embryo",
[SLEEPING]  "sleep ",
[RUNNABLE]  "runble",
[RUNNING]   "run   ",
[ZOMBIE]    "zombie"
};

#ifdef CS333_P3
// record with head and tail pointer for constant-time access to the beginning
// and end of a linked list of struct procs.  use with stateListAdd() and
// stateListRemove().
struct ptrs {
  struct proc* head;
  struct proc* tail;
};
#endif // CS333_P3

static struct {
  struct spinlock lock;
  struct proc proc[NPROC];
  #ifdef CS333_P3
  struct ptrs list[statecount];
  #endif // CS333_P3
  #ifdef CS333_P4
  struct ptrs ready[MAXPRIO+1];
  uint PromoteAtTime;
  #endif // CS333_P4
} ptable;

// list management function prototypes
#ifdef CS333_P3
static void initProcessLists(void);
static void initFreeList(void);
static void stateListAdd(struct ptrs*, struct proc*);
static int  stateListRemove(struct ptrs*, struct proc* p);
static void assertState(struct proc*, enum procstate, const char *, int);
#endif // CS333_P3

static struct proc *initproc;

uint nextpid = 1;
extern void forkret(void);
extern void trapret(void);
static void wakeup1(void* chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;

  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");

  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid) {
      return &cpus[i];
    }
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  int found = 0;
  #ifdef CS333_P3
  p = ptable.list[UNUSED].head;
  if(p)
  {
    found = 1;
  }
  #else // CS333_P0
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED) {
      found = 1;
      break;
    }
  #endif // CS333_P3
  if (!found) {
    release(&ptable.lock);
    return 0;
  }

  #ifdef CS333_P3
  if(stateListRemove(&ptable.list[UNUSED], p) == -1)
  {
    panic("Process Not Found In UNUSED List!");
  }
  assertState(p, UNUSED, __FUNCTION__, __LINE__);
  #endif // CS333_P3

  p->state = EMBRYO;

  #ifdef CS333_P3
  stateListAdd(&ptable.list[EMBRYO], p);
  #endif // CS333_P3
  p->pid = nextpid++;
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    #ifdef CS333_P3
    acquire(&ptable.lock);
    if(stateListRemove(&ptable.list[EMBRYO], p) == -1)
    {
      panic("Process Not Found In EMBRYO List!");
    }
    assertState(p, EMBRYO, __FUNCTION__, __LINE__);
    #endif // CS333_P3
    p->state = UNUSED;
    #ifdef CS333_P3
    stateListAdd(&ptable.list[UNUSED], p);
    release(&ptable.lock);
    #endif // CS333_P3
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  #ifdef CS333_P1
  p->start_ticks = ticks;
  #endif // CS333_P1

  #ifdef CS333_P2
  p->uid = 0;
  p->gid = 0;
  p->cpu_ticks_total = 0;
  p->cpu_ticks_in = 0;
  #endif // CS333_P2
  //Set Default Priority to value defined in pdx.h, used DEFAULTPRIO to control the default priority for testing promotion and demotion.
  #ifdef CS333_P4
  p->priority = DEFAULTPRIO;
  p->budget = BUDGET;
  #endif // CS333_P4
  return p;
}

// Set up first user process.
void
userinit(void)
{
  #ifdef CS333_P3
  acquire(&ptable.lock);
  initProcessLists();
  initFreeList();
  #ifdef CS333_P4
  ptable.PromoteAtTime = ticks + TICKS_TO_PROMOTE;
  #endif // CS333_P4
  release(&ptable.lock);
  #endif // CS333_P3

  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();

  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S
  #ifdef CS333_P2
  p->gid = GID;
  p->uid = UID;
  #endif // CS333_P2

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);
  #ifdef CS333_P3
  if(stateListRemove(&ptable.list[EMBRYO], p) == -1)
  {
    panic("Process Not Found In EMBRYO List!");
  }
  assertState(p, EMBRYO, __FUNCTION__, __LINE__);
  #endif // CS333_P3
  p->state = RUNNABLE;
  #if defined (CS333_P4)
  stateListAdd(&ptable.ready[p->priority], p);
  #elif defined (CS333_P3)
  stateListAdd(&ptable.list[RUNNABLE], p);
  #endif // CS333_P4
  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i;

  uint pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }
  #ifdef CS333_P3
  acquire(&ptable.lock);
  if(stateListRemove(&ptable.list[EMBRYO], np) == -1)
  {
    panic("Process Not Found In EMBRYO List!");
  }
  assertState(np, EMBRYO, __FUNCTION__, __LINE__);
  release(&ptable.lock);
  #endif // CS333_P3

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    #ifdef CS333_P3
    acquire(&ptable.lock);
    stateListAdd(&ptable.list[UNUSED], np);
    release(&ptable.lock);
    #endif // CS333_P3
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;
  #ifdef CS333_P2
  np->uid = curproc->uid;
  np->gid = curproc->gid;
  #endif // CS333_P2

  acquire(&ptable.lock);
  np->state = RUNNABLE;
  #if defined (CS333_P4)
  stateListAdd(&ptable.ready[np->priority], np);
  #elif defined (CS333_P3)
  stateListAdd(&ptable.list[RUNNABLE], np);
  #endif // CS333_P4
  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
#if defined (CS333_P4)
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);
  
  // Pass abandoned children to init.
  for(int i = 1; i < statecount; ++i){
    if(i != RUNNABLE)
    {
      p = ptable.list[i].head;
      while(p)
      {
        if(p->parent == curproc){

          p->parent = initproc;
          if(p->state == ZOMBIE)
          {
            wakeup1(initproc);
          }
        }
        p = p->next;
      }
    }
  }

  for(int i = MAXPRIO; i > -1; --i)
  {
    p = ptable.ready[i].head;
    while(p){
      if(p->parent == curproc){
        p->parent = initproc;
      }
      p = p->next;
    }
  }
 
  // Jump into the scheduler, never to return.
  if(stateListRemove(&ptable.list[RUNNING], curproc) == -1)
  {
    panic("Process Not Found In Running List!");
  }
  assertState(curproc, RUNNING, __FUNCTION__, __LINE__);

  // Here to Complete Transition Budget Math
  if(MAXPRIO){
    curproc->budget = curproc->budget - (ticks - curproc->cpu_ticks_in);
    if(curproc->budget <= 0){
      if(curproc->priority > 0){
        curproc->priority -= 1;
      }
      curproc->budget = BUDGET;
    }
  }

  curproc->state = ZOMBIE;
  stateListAdd(&ptable.list[ZOMBIE], curproc);
  
  #ifdef PDX_XV6
  curproc->sz = 0;
  #endif // PDX_XV6
  sched();
  panic("zombie exit");
}

#elif defined (CS333_P3)
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(int i = 1; i < statecount; ++i){
    p = ptable.list[i].head;
    while(p){  
      if(p->parent == curproc){
        p->parent = initproc;
        if(p->state == ZOMBIE)
        {
          wakeup1(initproc);
        }
      }
      p = p->next;
    }
  }

  // Jump into the scheduler, never to return.
  if(stateListRemove(&ptable.list[RUNNING], curproc) == -1)
  {
    panic("Process Not Found In Running List!");
  }
  assertState(curproc, RUNNING, __FUNCTION__, __LINE__);
  curproc->state = ZOMBIE;
  stateListAdd(&ptable.list[ZOMBIE], curproc);
  #ifdef PDX_XV6
  curproc->sz = 0;
  #endif // PDX_XV6
  sched();
  panic("zombie exit");
}

#else
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  #ifdef PDX_XV6
  curproc->sz = 0;
  #endif // PDX_XV6
  sched();
  panic("zombie exit");
}
#endif // CS333_P4

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
#if defined (CS333_P4)
int
wait(void)
{
  struct proc *p;
  int havekids;
  uint pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    havekids = 0;
    // Look for Kids in State Lists
    for(int i = 1; i < statecount; ++i)
    {
      if(i != RUNNABLE)
      {
        p = ptable.list[i].head;
        while(p)
        {
          if(p->parent == curproc)
          {
            havekids = 1;
            if(p->state == ZOMBIE){
              // Found one.
              pid = p->pid;
              kfree(p->kstack);
              p->kstack = 0;
              freevm(p->pgdir);
              p->pid = 0;
              p->parent = 0;
              p->name[0] = 0;
              p->killed = 0;
              if(stateListRemove(&ptable.list[ZOMBIE], p) == -1)
              {
                panic("Process Not Found In ZOMBIE List!");
              }
              assertState(p, ZOMBIE, __FUNCTION__, __LINE__);
              p->state = UNUSED;
              stateListAdd(&ptable.list[UNUSED], p);
              release(&ptable.lock);
              return pid;
            }
          }
          p = p->next;
        }
      }
    }

    // Look for Kids in Ready Lists
    for(int i = MAXPRIO; i > -1; --i)
    {
      p = ptable.ready[i].head;
      while(p){
        if(p->parent == curproc){
          havekids = 1;
        }
        p = p->next;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

#elif defined(CS333_P3)
int
wait(void)
{
  struct proc *p;
  int havekids;
  uint pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(int i = 1; i < statecount; ++i)
    {
      p = ptable.list[i].head;
      while(p)
      {
        if(p->parent == curproc)
        {
          havekids = 1;
          if(p->state == ZOMBIE){
          // Found one.
            pid = p->pid;
            kfree(p->kstack);
            p->kstack = 0;
            freevm(p->pgdir);
            p->pid = 0;
            p->parent = 0;
            p->name[0] = 0;
            p->killed = 0;
            if(stateListRemove(&ptable.list[ZOMBIE], p) == -1)
            {
              panic("Process Not Found In ZOMBIE List!");
            }
            assertState(p, ZOMBIE, __FUNCTION__, __LINE__);
            p->state = UNUSED;
            stateListAdd(&ptable.list[UNUSED], p);
            release(&ptable.lock);
            return pid;
          }
        }
        p = p->next;
      }
    }
    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

#else
int
wait(void)
{
  struct proc *p;
  int havekids;
  uint pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}
#endif // CS333_P4

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
#if defined(CS333_P4)
void
scheduler(void)
{
  struct proc *p;
  struct proc *save;
  struct cpu *c = mycpu();
  c->proc = 0;
  #ifdef PDX_XV6
  int idle;  // for checking if processor is idle
  #endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

    #ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
    #endif // PDX_XV6

    acquire(&ptable.lock);
    // PROMOTION has MAXPRIO as field to turn on and off ready lists
    // For the case MAXPRIO = 0
    if(ticks >= ptable.PromoteAtTime && MAXPRIO)
    {
      //Promotes On SLEEPING List
      p = ptable.list[SLEEPING].head;
      while(p){
        if(p->priority < MAXPRIO) // Prevents prio over MAXPRIO or promoting procs at MAXPRIO
        {
          p->priority += 1;
          p->budget = BUDGET;
        }
        p = p->next;
      }

      //Promotes On RUNNING List
      p = ptable.list[RUNNING].head;
      while(p){
        if(p->priority < MAXPRIO) // Prevents prio over MAXPRIO or promoting procs at MAXPRIO
        {
          p->priority += 1;
          p->budget = BUDGET;
        }
        p = p->next;
      }

      // Promotes On Ready (RUNNABLE) Lists
      for(int i = MAXPRIO; i > -1; --i)
      {
        p = ptable.ready[i].head;
        while(p){
          // Saves next process in ready list
          save = p->next;
          if(p->priority < MAXPRIO) // Prevents access to ready list MAXPRIO
          {
            // Removes From Priority List, Sets Promoted Priority,
            // and adds back to ready list with the budget.
            if(stateListRemove(&ptable.ready[p->priority], p) == -1)
            {
              panic("Process Not Found in Ready Lists!");
            }
            assertState(p, RUNNABLE, __FUNCTION__, __LINE__); // Checks if state is still correctA
            p->priority += 1;
            p->budget = BUDGET;
            stateListAdd(&ptable.ready[p->priority], p);
          }
          // readies next process
          p = save;
        }
      }
      ptable.PromoteAtTime = ticks + TICKS_TO_PROMOTE;
    }
    release(&ptable.lock);
    
    // Loop through ready lists starting at MAXPRIO looking for non empty ready list,
    // If found select the head of the list to run
    acquire(&ptable.lock);
    for(int i = MAXPRIO; i > -1; --i)
    {
      p = ptable.ready[i].head; // Selects Head
      if(p){ // Checks if head exists
        // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to us.
        #ifdef PDX_XV6
        idle = 0;  // not idle this timeslice
        #endif // PDX_XV6
        c->proc = p;
        switchuvm(p);
        if(stateListRemove(&ptable.ready[p->priority], p) == -1)
        {
          if(p->priority != i)
          {
            panic("Process Not Found in Correct Ready List!");
          }
          else
          {
            panic("Process Not Found In Ready Lists!");
          }
        }
        assertState(p, RUNNABLE, __FUNCTION__, __LINE__);
        p->state = RUNNING;
        stateListAdd(&ptable.list[RUNNING], p);
        #ifdef CS333_P2
        p->cpu_ticks_in = ticks;
        #endif // CS333_P2
        swtch(&(c->scheduler), p->context);
        switchkvm();

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
        break; // Used to prevent scheduler from scheduling another
        // process due to for loop being used.
      }
    }
    release(&ptable.lock);
    #ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
    #endif // PDX_XV6
  }
}

#elif defined(CS333_P3)
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  #ifdef PDX_XV6
  int idle;  // for checking if processor is idle
  #endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

    #ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
    #endif // PDX_XV6
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    p = ptable.list[RUNNABLE].head;
    while(p){
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      #ifdef PDX_XV6
      idle = 0;  // not idle this timeslice
      #endif // PDX_XV6
      c->proc = p;
      switchuvm(p);
      if(stateListRemove(&ptable.list[RUNNABLE], p) == -1)
      {
        panic("Process Not Found In RUNNABLE List!");
      }
      assertState(p, RUNNABLE, __FUNCTION__, __LINE__);
      p->state = RUNNING;
      stateListAdd(&ptable.list[RUNNING], p);
      #ifdef CS333_P2
      p->cpu_ticks_in = ticks;
      #endif // CS333_P2
      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      p = p->next;
    }
    release(&ptable.lock);
    #ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
    #endif // PDX_XV6
  }
}

#else
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  #ifdef PDX_XV6
  int idle;  // for checking if processor is idle
  #endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

    #ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
    #endif // PDX_XV6
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      #ifdef PDX_XV6
      idle = 0;  // not idle this timeslice
      #endif // PDX_XV6
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;
      #ifdef CS333_P2
      p->cpu_ticks_in = ticks;
      #endif // CS333_P2
      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
    #ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
    #endif // PDX_XV6
  }
}
#endif // CS333_P4

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  #ifdef CS333_P2
  p->cpu_ticks_total += (ticks - p->cpu_ticks_in);
  #endif // CS333_P2
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
#if defined(CS333_P4)
void
yield(void)
{
  struct proc *curproc = myproc();

  acquire(&ptable.lock);  //DOC: yieldlock
  if(stateListRemove(&ptable.list[RUNNING], curproc) == -1)
  {
    panic("Process Not Found In RUNNING List!");
  }
  assertState(curproc, RUNNING, __FUNCTION__, __LINE__);
  curproc->state = RUNNABLE;
  
  // Add to Ready List
  if(MAXPRIO)
  {
    curproc->budget = curproc->budget - (ticks - curproc->cpu_ticks_in);
    if(curproc->budget <= 0){
      if(curproc->priority > 0)
      {
        curproc->priority -= 1;
      }
      curproc->budget = BUDGET; // Adjust budget if value
    }
  }
  stateListAdd(&ptable.ready[curproc->priority], curproc);
  sched();
  release(&ptable.lock);
}

#elif defined(CS333_P3)
void
yield(void)
{
  struct proc *curproc = myproc();

  acquire(&ptable.lock);  //DOC: yieldlock
  if(stateListRemove(&ptable.list[RUNNING], curproc) == -1)
  {
    panic("Process Not Found In RUNNING List!");
  }
  assertState(curproc, RUNNING, __FUNCTION__, __LINE__);
  curproc->state = RUNNABLE;
  stateListAdd(&ptable.list[RUNNABLE], curproc);
  sched();
  release(&ptable.lock);
}

#else
void
yield(void)
{
  struct proc *curproc = myproc();

  acquire(&ptable.lock);  //DOC: yieldlock
  curproc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}
#endif // CS333_P4

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
#if defined(CS333_P3)
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    if (lk) release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  
  if(stateListRemove(&ptable.list[RUNNING], p) == -1)
  {
    panic("Process Not Found In RUNNING List!");
  }
  assertState(p, RUNNING, __FUNCTION__, __LINE__);
  #ifdef CS333_P4
  if(MAXPRIO)
  {
    p->budget = p->budget - (ticks - p->cpu_ticks_in);
    if(p->budget <= 0)
    {
      if(p->priority > 0)
      {
        p->priority -= 1;
      }
      p->budget = BUDGET;
    }
  }
  #endif // CS333_P4
  p->state = SLEEPING;
  stateListAdd(&ptable.list[SLEEPING], p);

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}

#else
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    if (lk) release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}
#endif // CS333_P3

// Wake up all processes sleeping on chan.
// The ptable lock must be held.
#if defined(CS333_P4)
static void
wakeup1(void *chan)
{
  struct proc *p = ptable.list[SLEEPING].head;
  while(p){
    if(p->state == SLEEPING && p->chan == chan){
      if(stateListRemove(&ptable.list[SLEEPING], p) == -1)
      {
        panic("Proccess Not Found In SLEEPING List!");
      }
      assertState(p, SLEEPING, __FUNCTION__, __LINE__);
      p->state = RUNNABLE;
      stateListAdd(&ptable.ready[p->priority], p);
    }
    p = p->next;
  }
}

#elif defined(CS333_P3)
static void
wakeup1(void *chan)
{
  struct proc *p = ptable.list[SLEEPING].head;
  while(p){
    if(p->state == SLEEPING && p->chan == chan){
      if(stateListRemove(&ptable.list[SLEEPING], p) == -1)
      {
        panic("Proccess Not Found In SLEEPING List!");
      }
      assertState(p, SLEEPING, __FUNCTION__, __LINE__);
      p->state = RUNNABLE;
      stateListAdd(&ptable.list[RUNNABLE], p);
    }
    p = p->next;
  }
}

#else
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}
#endif // CS333_P4

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
#if defined(CS333_P4)
int
kill(int pid)
{
  struct proc *p;
  
  acquire(&ptable.lock);
  for(int i = 1; i < statecount; ++i)
  {
    if(i != RUNNABLE)
    {
      p = ptable.list[i].head;
      while(p)
      {
        if(p->pid == pid) {
          p->killed = 1;
          if(p->state == SLEEPING){
            if(stateListRemove(&ptable.list[SLEEPING], p) == -1)
            {
              panic("Process Not Found in Sleeping List!");
            }
            assertState(p, SLEEPING, __FUNCTION__, __LINE__);
            p->state = RUNNABLE;
            stateListAdd(&ptable.ready[p->priority], p);
          }
          release(&ptable.lock);
          return 0;
        }
        p = p->next;
      }
    }
  }

  for(int i = MAXPRIO; i > -1; --i)
  {
    p = ptable.ready[i].head;
    while(p)
    {
      if(p->pid == pid)
      {
        p->killed = 1;
        release(&ptable.lock);
        return 0;
      }
      p = p->next;
    }
  }
  release(&ptable.lock);
  return -1;
}

#elif defined(CS333_P3)
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(int i = 1; i < statecount; ++i)
  {
    p = ptable.list[i].head;
    while(p)
    {
      if(p->pid == pid) {
        p->killed = 1;
        if(p->state == SLEEPING){
          if(stateListRemove(&ptable.list[SLEEPING], p) == -1)
          {
            panic("Process Not Found in Sleeping List!");
          }
          assertState(p, SLEEPING, __FUNCTION__, __LINE__);
          p->state = RUNNABLE;
          stateListAdd(&ptable.list[RUNNABLE], p);
        }
        release(&ptable.lock);
        return 0;
      }
      p = p->next;
    }
  }
  release(&ptable.lock);
  return -1;
}

#else
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}
#endif //CS333_P4

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.

#if defined(CS333_P4)
void
procdumpP4(struct proc *p, char *state_string)
{
  int spacer = 13 - strlen(p->name);
  int t_ticks = ticks - p->start_ticks;
  int sec = t_ticks / 1000;
  int mill = t_ticks % 1000;

  int c_ticks = p->cpu_ticks_total;
  int c_sec = c_ticks / 1000;
  int c_mill = c_ticks % 1000;
  cprintf("%d\t%s", p->pid, p->name);
  for(int i = 0; i < spacer; ++i)
  {
    cprintf(" ");
  }
  int ppid = 0;
  if(p->parent == NULL)
  {
    ppid = p->pid;
  }
  else
  {
    ppid = p->parent->pid;
  }
  cprintf("%d\t\t%d\t%d\t%d\t", p->uid, p->gid, ppid, p->priority);
  if(mill < 10)
  {
    cprintf("%d.00%d\t", sec, mill);
  }
  else if(mill >= 10 && mill < 100)
  {
    cprintf("%d.0%d\t", sec, mill);
  }
  else
  {
    cprintf("%d.%d\t", sec, mill);
  }
  if(c_mill < 10)
  {
    cprintf("%d.00%d\t", c_sec, c_mill);
  }
  else if(c_mill >= 10 && c_mill < 100)
  {
    cprintf("%d.0%d\t", c_sec, c_mill);
  }
  else
  {
    cprintf("%d.%d\t", c_sec, c_mill);
  }
  cprintf("%s\t%d\t", state_string, p->sz);
  return;
}

#elif defined(CS333_P3)
void
procdumpP3(struct proc *p, char *state_string)
{  
  int spacer = 13 - strlen(p->name);
  int t_ticks = ticks - p->start_ticks;
  int sec = t_ticks / 1000;
  int mill = t_ticks % 1000;

  int c_ticks = p->cpu_ticks_total;
  int c_sec = c_ticks / 1000;
  int c_mill = c_ticks % 1000;
  cprintf("%d\t%s", p->pid, p->name);
  for(int i = 0; i < spacer; ++i)
  {
    cprintf(" ");
  }
  int ppid = 0;
  if(p->parent == NULL)
  {
    ppid = p->pid;
  }
  else
  {
    ppid = p->parent->pid;
  }
  cprintf("%d\t\t%d\t%d\t", p->uid, p->gid, ppid);
  if(mill < 10)
  {
    cprintf("%d.00%d\t", sec, mill);
  }
  else if(mill >= 10 && mill < 100)
  {
    cprintf("%d.0%d\t", sec, mill);
  }
  else
  {
    cprintf("%d.%d\t", sec, mill);
  }
  if(c_mill < 10)
  {
    cprintf("%d.00%d\t", c_sec, c_mill);
  }
  else if(c_mill >= 10 && c_mill < 100)
  {
    cprintf("%d.0%d\t", c_sec, c_mill);
  }
  else
  {
    cprintf("%d.%d\t", c_sec, c_mill);
  }
  cprintf("%s\t%d\t", state_string, p->sz); 
  return;
}
#elif defined(CS333_P2)
void
procdumpP2(struct proc *p, char *state_string)
{
  int spacer = 13 - strlen(p->name);
  int t_ticks = ticks - p->start_ticks;
  int sec = t_ticks / 1000;
  int mill = t_ticks % 1000;

  int c_ticks = p->cpu_ticks_total;
  int c_sec = c_ticks / 1000;
  int c_mill = c_ticks % 1000;
  cprintf("%d\t%s", p->pid, p->name);
  for(int i = 0; i < spacer; ++i)
  {
    cprintf(" ");
  }
  int ppid = 0;
  if(p->parent == NULL)
  {
    ppid = p->pid;
  }
  else
  {
    ppid = p->parent->pid;
  }
  cprintf("%d\t\t%d\t%d\t", p->uid, p->gid, ppid);
  if(mill < 10)
  {
    cprintf("%d.00%d\t", sec, mill);
  }
  else if(mill >= 10 && mill < 100)
  { 
    cprintf("%d.0%d\t", sec, mill);
  }
  else
  { 
    cprintf("%d.%d\t", sec, mill);
  }
  if(c_mill < 10)
  { 
    cprintf("%d.00%d\t", c_sec, c_mill);
  }
  else if(c_mill >= 10 && c_mill < 100)
  {
    cprintf("%d.0%d\t", c_sec, c_mill);
  }
  else
  {   
    cprintf("%d.%d\t", c_sec, c_mill);
  }
  cprintf("%s\t%d\t", state_string, p->sz);
  return;
}
#elif defined(CS333_P1)
//Originally one line needed more variables to process the process run time correctly, and for loop to process
//spaces correctly for formatting between names and elapsed
void
procdumpP1(struct proc *p, char *state_string)
{
  int spacer = 13 - strlen(p->name);
  int t_ticks = ticks - p->start_ticks;
  int sec = t_ticks / 1000;
  int mill = t_ticks % 1000;
  cprintf("%d\t%s", p->pid, p->name);
  for(int i = 0; i < spacer; ++i)
  {
    cprintf(" ");
  }
  if(mill < 10)
  {
    cprintf("%d.00%d\t%s\t%d\t", sec, mill, state_string, p->sz);
  }
  else if(mill >= 10 && mill < 100)
  {  
    cprintf("%d.0%d\t%s\t%d\t", sec, mill, state_string, p->sz);
  }
  else
  {
    cprintf("%d.%d\t%s\t%d\t", sec, mill, state_string, p->sz);
  }
  return;
}
#endif

void
procdump(void)
{
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

#if defined(CS333_P4)
#define HEADER "\nPID\tName         UID\tGID\tPPID\tPrio\tElapsed\tCPU\tState\tSize\t PCs\n"
#elif defined(CS333_P3)
#define HEADER "\nPID\tName         UID\tGID\tPPID\tElapsed\tCPU\tState\tSize\t PCs\n"
#elif defined(CS333_P2)
#define HEADER "\nPID\tName         UID\tGID\tPPID\tElapsed\tCPU\tState\tSize\t PCs\n"
#elif defined(CS333_P1)
#define HEADER "\nPID\tName         Elapsed\tState\tSize\t PCs\n"
#else
#define HEADER "\n"
#endif

  cprintf(HEADER);  // not conditionally compiled as must work in all project states

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";

    // see TODOs above this function
#if defined(CS333_P4)
    procdumpP4(p, state);
#elif defined(CS333_P3)
    procdumpP3(p, state);
#elif defined(CS333_P2)
    procdumpP2(p, state);
#elif defined(CS333_P1)
    procdumpP1(p, state);
#else
    cprintf("%d\t%s\t%s\t", p->pid, p->name, state);
#endif

    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
#ifdef CS333_P1
  cprintf("$ ");  // simulate shell prompt
#endif // CS333_P1
}

#ifdef CS333_P3
#if defined (CS333_P4)
void runnabledump()
{
  cprintf("Ready List Processes:\n");
  acquire(&ptable.lock);
  struct proc *p;
  for(int i = MAXPRIO; i > -1; --i)
  {
    p = ptable.ready[i].head;
    cprintf("%d: ", i);
    while(p){
      cprintf("(%d, %d)", p->pid, p->budget);
      if(p->next != NULL)
      {
        cprintf("->");
      }
      p = p->next;
    }
    cprintf("\n");
  }
  release(&ptable.lock);
  cprintf("\n");
  cprintf("$ ");
  return;
}

#elif defined (CS333_P3)
void runnabledump()
{
  cprintf("Ready List Processes:");
  acquire(&ptable.lock);
  struct proc *p = ptable.list[RUNNABLE].head;
  if(p){
    cprintf("\n ");
  }
  while(p){
    cprintf("%d", p->pid);
    if(p->next != NULL)
    {
      cprintf("->");
    }
    p = p->next;
  }
  release(&ptable.lock);
  cprintf("\n");
  cprintf("$ ");
  return;
}
#endif

void unuseddump()
{
  int u = 0;
  acquire(&ptable.lock);
  struct proc *p = ptable.list[UNUSED].head;
  while(p)
  {
    u++;
    p = p->next;
  }
  release(&ptable.lock);
  cprintf("Free List Size: %d Processes\n", u);
  cprintf("$ ");
  return;
}

void sleepdump()
{
  cprintf("Sleep List Processes:");
  acquire(&ptable.lock);
  struct proc *p = ptable.list[SLEEPING].head;
  if(p){
    cprintf("\n ");
  }
  while(p){
    cprintf("%d", p->pid);
    if(p->next != NULL)
    {
      cprintf("->");
    }
    p = p->next;
  }
  release(&ptable.lock);
  cprintf("\n");
  cprintf("$ ");
  return;
}

void zombiedump()
{
  int ppid = 0;
  cprintf("Zombie List Processes:");
  acquire(&ptable.lock);
  struct proc *p = ptable.list[ZOMBIE].head;
  if(p){
    cprintf("\n ");
  }
  while(p){
    if(p->parent == NULL)
    {
      ppid = p->pid;
    }
    else
    {
      ppid = p->parent->pid;
    }
    cprintf("(%d, %d)", p->pid, ppid);
    if(p->next != NULL)
    {
      cprintf("->");
    }
    p = p->next;
  }
  release(&ptable.lock);
  cprintf("\n");
  cprintf("$ "); 
  return;
}
#endif

#if defined(CS333_P3)
// list management helper functions
static void
stateListAdd(struct ptrs* list, struct proc* p)
{
  if((*list).head == NULL){
    (*list).head = p;
    (*list).tail = p;
    p->next = NULL;
  } else{
    ((*list).tail)->next = p;
    (*list).tail = ((*list).tail)->next;
    ((*list).tail)->next = NULL;
  }
}
#endif

#if defined(CS333_P3)
static int
stateListRemove(struct ptrs* list, struct proc* p)
{
  if((*list).head == NULL || (*list).tail == NULL || p == NULL){
    return -1;
  }

  struct proc* current = (*list).head;
  struct proc* previous = 0;

  if(current == p){
    (*list).head = ((*list).head)->next;
    // prevent tail remaining assigned when we've removed the only item
    // on the list
    if((*list).tail == p){
      (*list).tail = NULL;
    }
    return 0;
  }

  while(current){
    if(current == p){
      break;
    }

    previous = current;
    current = current->next;
  }

  // Process not found. return error
  if(current == NULL){
    return -1;
  }

  // Process found.
  if(current == (*list).tail){
    (*list).tail = previous;
    ((*list).tail)->next = NULL;
  } else{
    previous->next = current->next;
  }

  // Make sure p->next doesn't point into the list.
  p->next = NULL;

  return 0;
}
#endif

#if defined(CS333_P3)
static void
initProcessLists()
{
  int i;

  for (i = UNUSED; i <= ZOMBIE; i++) {
    ptable.list[i].head = NULL;
    ptable.list[i].tail = NULL;
  }
#if defined(CS333_P4)
  for (i = 0; i <= MAXPRIO; i++) {
    ptable.ready[i].head = NULL;
    ptable.ready[i].tail = NULL;
  }
#endif
}
#endif

#if defined(CS333_P3)
static void
initFreeList(void)
{
  struct proc* p;

  for(p = ptable.proc; p < ptable.proc + NPROC; ++p){
    p->state = UNUSED;
    stateListAdd(&ptable.list[UNUSED], p);
  }
}
#endif

#if defined(CS333_P3)
// example usage:
// assertState(p, UNUSED, __FUNCTION__, __LINE__);
// This code uses gcc preprocessor directives. For details, see
// https://gcc.gnu.org/onlinedocs/cpp/Standard-Predefined-Macros.html
static void
assertState(struct proc *p, enum procstate state, const char * func, int line)
{
    if (p->state == state)
      return;
    cprintf("Error: proc state is %s and should be %s.\nCalled from %s line %d\n",
        states[p->state], states[state], func, line);
    panic("Error: Process state incorrect in assertState()");
}
#endif

#if defined(CS333_P2)
int 
getuid(void)
{
    int ret = 0;
    acquire(&ptable.lock);
    ret = myproc()->uid;
    release(&ptable.lock);
    return ret;
}

int
getgid(void)
{
    int ret = 0;
    acquire(&ptable.lock);
    ret = myproc()->gid;
    release(&ptable.lock);
    return ret;
}
int
getppid(void)
{
    int ret = 0;
    acquire(&ptable.lock);
    if(myproc()->parent == NULL)
    {
      ret = myproc()->pid;
    }
    else
    {
      ret = myproc()->parent->pid;
    }
    release(&ptable.lock);
    return ret;
}
int
setuid(int uid)
{
    acquire(&ptable.lock);
    myproc()->uid = uid;
    release(&ptable.lock);
    return 0;
}

int
setgid(int gid)
{
    acquire(&ptable.lock);
    myproc()->gid = gid;
    release(&ptable.lock);
    return 0;    
}

int
getprocs(int max, struct uproc * table)
{
  struct proc *p;
  int num = 0;
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == RUNNABLE || p->state == SLEEPING || p->state == RUNNING || p->state == ZOMBIE)
    {
      table[num].pid = p->pid;
      table[num].uid = p->uid;
      table[num].gid = p->gid;
      if(p->parent == NULL)
      {
        table[num].ppid = p->pid;
      }
      else
      {
        table[num].ppid = p->parent->pid;
      }
      #ifdef CS333_P4
      table[num].priority = p->priority;
      #endif //CS333_P4
      table[num].elapsed_ticks = ticks - p->start_ticks;
      table[num].CPU_total_ticks = p->cpu_ticks_total;
      safestrcpy(table[num].state, states[p->state], STRMAX);
      table[num].size = p->sz;
      safestrcpy(table[num].name, p->name, STRMAX);
      num++;
    }
    if(num == max)
    {
      release(&ptable.lock);
      return num;
    }
  }
  release(&ptable.lock);
  return num;
}
#endif //CS333_P2

#ifdef CS333_P4
int
setpriority(int pid, int priority)
{
  acquire(&ptable.lock);
  struct proc *p;

  for(int i = MAXPRIO; i > -1; --i)
  {
    p = ptable.ready[i].head;
    while(p)
    {
      if(p->pid == pid)
      {
        if(p->priority == priority)
        {
          release(&ptable.lock);
          return 0;
        }
        if(stateListRemove(&ptable.ready[p->priority], p) == -1)
        {
          panic("Not on Ready Lists!");
        }
        p->priority = priority;
        p->budget = BUDGET;
        stateListAdd(&ptable.ready[p->priority], p);
        release(&ptable.lock);
        return 0;
      }
      p = p->next;
    }
  }

  p = ptable.list[SLEEPING].head;
  while(p)
  {
    if(p->pid == pid)
    {
      if(p->priority == priority)
      {
          release(&ptable.lock);
          return 0;
      }
      p->priority = priority;
      p->budget = BUDGET;
      release(&ptable.lock);
      return 0;
    }
    p = p->next;
  }

  p = ptable.list[RUNNING].head;
  while(p)
  {
    if(p->pid == pid)
    {
      if(p->priority == priority)
      {
          release(&ptable.lock);
          return 0;
      }
      p->priority = priority;
      p->budget = BUDGET;
      release(&ptable.lock);
      return 0;
    }
    p = p->next;
  }
  release(&ptable.lock);
  return -1;
}

int
getpriority(int pid)
{
  int ret = 0;
  acquire(&ptable.lock);
  struct proc *p;
  for(int i = MAXPRIO; i > -1; --i)
  {
    p = ptable.ready[i].head;
    while(p)
    {
      if(p->pid == pid)
      {
        ret = p->priority;
        release(&ptable.lock);
        return ret;
      }
      p = p->next;
    }
  }

  p = ptable.list[SLEEPING].head;
  while(p)
  {
    if(p->pid == pid)
    {
      ret = p->priority;
      release(&ptable.lock);
      return ret;
    }
    p = p->next;
  }

  p = ptable.list[RUNNING].head;
  while(p)
  {
    if(p->pid == pid)
    {
      ret = p->priority;
      release(&ptable.lock);
      return ret;
    }
    p = p->next;
  }
  release(&ptable.lock);
  return -1;
}
#endif // CS333_P4
