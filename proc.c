#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

#define LOGS 0

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

struct proc* q0[64];
struct proc* q1[64];
struct proc* q2[64];
struct proc* q3[64];
struct proc* q4[64];
int c0 = -1;
int c1 = -1;
int c2 = -1;
int c3 = -1;
int c4 = -1;
int cnt[5]={-1,-1,-1,-1,-1};
struct proc *queue[5][NPROC];
int slice[5] ={1,2,4,8,16};

int nextpid = 1;
int qinsert(struct proc *p, int qno);
int qrem(struct proc *p, int qno);

extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);


void 
change_qflag(struct proc* p)
{
  acquire(&ptable.lock);
  p-> change_q = 1;
  release(&ptable.lock);
}

void 
incr_curr_ticks(struct proc *p)
{
  acquire(&ptable.lock);
  p->curr_ticks++;
  p->q[p->curq]++;
  release(&ptable.lock);
}

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
    if (cpus[i].apicid == apicid)
      return &cpus[i];
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

//PAGEBREAK: 32
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

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  //c0++;
 // p->priority=1; 
 // q0[c0] = p;
  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  

  //cprintf("************%d\n",c0);
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
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

/********************************** ADDING ************************************/

    p->priority = 60;
 // #ifdef MLFQ
 //   c0++;
    p->curq=0;
    p->curr_ticks = 0;
    p->ent = 0;
    for(int i=0; i<5; i++)
      p->q[i] = 0;

  //  cprintf("hi\n");
 // #endif
//  acquire(&tickslock);
  p->ctime = ticks;
//  release(&tickslock);
  p->etime = 0;
  p->rtime = 0;
  p->iotime = 0;
  p->nrun = 0;
  p->wtime=0;
  p->lastExec = 0;
/********************************** ADDING ************************************/

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
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

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;
  #ifdef MLFQ
      qinsert(p, 0);
  #endif

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
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
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

  acquire(&ptable.lock);

  np->state = RUNNABLE;
  #ifdef MLFQ
      qinsert(np, 0);
  #endif


  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
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
  /******************************** addition start ******************************/
 // acquire(&tickslock);
  curproc->etime = ticks;
 // release(&tickslock);
  /******************************** addition end ******************************/
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
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
        #ifdef MLFQ
          qrem(p, p->curq);
        #endif 
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

/******************************** waitx addition start ******************************/
int
waitx(int *wtime, int* rtime)
{
  struct proc *p;
  int havekids, pid;
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
        *wtime= p->etime - p->ctime - p->rtime - p->iotime;
        *rtime=p->rtime;
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        #ifdef MLFQ
          qrem(p, p->curq);
        #endif 
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

/******************************* waitx addition end  ******************************/


//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  //struct proc *p = 0;

  struct cpu *c = mycpu();
  c->proc = 0;
  int i; int j;

  for(;;)
  {
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    #ifdef MLFQ
      //aging
      for(i=1; i < 5; i++)
      {
        for(j=0; j <= cnt[i]; j++)
        {
          struct proc *p = queue[i][j];
          int age = ticks - p->ent;
         // p->wtime = age;
          if(age > 32)
          {
            //promote
            qrem(p, i);
            #if LOGS
              cprintf("[%d] Moved {%d} from queue =%d= to _%d_\n",
                       ticks, p->pid, i, i-1);
            #endif
            qinsert(p, i-1);
            p->wtime=0;
          }

        }
      }
      struct proc *p =0;

      for(i=0; i < 5; i++)
      {
        if(cnt[i] >=0)
        {
          p = queue[i][0];
          qrem(p, i);
          break;
        }
      }

      if(p!=0 && p->state==RUNNABLE)
      {
        p->curr_ticks++;
     //   p->wtime++;
        p->nrun++;
        p->q[p->curq]++;
        c->proc = p;
         #if LOGS
               cprintf("[%d] {%d} scheduled in queue _%d_\n",
                       ticks, p->pid, p->curq);
              #endif
       // cprintf("Process %s with PID %d and priority %d and queue %d running\n",p->name, p->pid, p->priority, p->curq);
        switchuvm(p);
        p->state = RUNNING;
        swtch(&c->scheduler, p->context);
        switchkvm();
        c->proc = 0;

        if(p!=0 && p->state == RUNNABLE)
        {
          if(p->change_q == 1)
          {
            //demote
            p->change_q = 0;
            p->curr_ticks = 0;
            p->wtime=0;
            if(p->curq != 4){
              p->curq++;
             
            //  if(p->curq == 3){
               /* cprintf("hi3  %d\n",p->pid);
                //procs();
                cprintf("%d \t %d \t ", p->pid,p->priority);
                cprintf("sleeping");
                cprintf(" \t %d \t %d \t %d \t %d", p->rtime, p->wtime, p->nrun, p->curq );
                for (int i = 0; i < 5; ++i)
                {
                  cprintf(" \t %d",p->q[i]);
                }
                cprintf("\n");*/
            //  }
            }


          }

          else{
            p->curr_ticks = 0;
            p->wtime = 0;
          }
          qinsert(p, p->curq);

        }
      }


    #else
    #ifdef DEFAULT
      struct proc *p =0;

    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if(p->state != RUNNABLE)
        continue;
      if(p != 0)
      {

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
          p->nrun++;
      p->state = RUNNING;
      p->wtime=0;
      //cprintf("Process %d with pid %d running\n",p->priority,p->pid);

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      }
    }
   
    #else
    #ifdef FCFS
      struct proc *p =0;
      struct proc *minP = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if(p->state != RUNNABLE)
        continue;

      // ignore init and sh processes from FCFS
     // if(p->pid > 1)
      {
        if (minP != 0){
        if(p->ctime < minP->ctime)
          minP = p;
        }
        else{
          minP = p;
        }
      }
      if(minP != 0 && minP->state == RUNNABLE){
        p = minP;
        c->proc = p;
        switchuvm(p);
            p->nrun++;
        p->state = RUNNING;
        p->wtime =0;
        //cprintf("Process %d with pid %d running\n",p->priority,p->pid);

        swtch(&(c->scheduler), p->context);
        switchkvm();

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
    }
    #else
    #ifdef PBS
      struct proc *p =0;
        struct proc *highP = 0;
        struct proc *p1 = 0;
      for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
      {
        if(p->state != RUNNABLE)
          continue;
        // Choose the process with highest priority (among RUNNABLEs)
        if(highP==0)
          highP = p;
        else if((p->state == RUNNABLE) && (p->priority < highP->priority))
            highP = p;
      }
      if(highP == 0)
      {
        release(&ptable.lock);
        continue;   
      }
      for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
      {
        p1 = 0;int flag=0;
        for(p1 = ptable.proc; p1 < &ptable.proc[NPROC]; p1++){
          if((p1->state == RUNNABLE) && (p1->priority < highP->priority)){
            flag=1;
            break;
          }
        }
        if(flag)
          break;

        if(highP != 0 && p->state==RUNNABLE){
          if(highP->priority == p->priority){
            c->proc = p;
          //  cprintf("Process %s with PID %d and priority %d running\n",p->name, p->pid, p->priority);
            switchuvm(p);
                p->nrun++;
            p->state = RUNNING;
            //cprintf("Process %d with pid %d running\n",p->priority,p->pid);

            swtch(&(c->scheduler), p->context);
            switchkvm();

            // Process is done running for now.
            // It should have changed its p->state before coming back.
            c->proc = 0;
          }
        }
      }

    #endif
    #endif
    #endif
    #endif
    release(&ptable.lock);
  
  }
}

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
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
  
}

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
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
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
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;
int i;
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->state == SLEEPING && p->chan == chan){
      
      p->state = RUNNABLE;
      #ifdef MLFQ
        p->curr_ticks = 0;
        qinsert(p,p->curq);
      #endif
    }
  }
}

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
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING){
        p->state = RUNNABLE;
        #ifdef MLFQ
          qinsert(p,p->curq);
        #endif
      }

      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

int
procs()
{
static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleeping",
  [RUNNABLE]  "runnable",
  [RUNNING]   "running",
  [ZOMBIE]    "zombie"
  };
struct proc *p;
//Enables interrupts on this processor.
sti();

//Loop over process table looking for process with pid.
acquire(&ptable.lock);
cprintf("PID\tPriority\tState\tr_time\tw_time\tn_run\tcur_q\tq0 \t q1 \t q2 \t q3 \t q4 \n");
for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
  
//  p->wtime=ticks - p->lastExec;
  if(p->state == SLEEPING){
    cprintf("%d \t %d \t ", p->pid,p->priority);
    cprintf("sleeping");
    cprintf(" \t %d \t %d \t %d \t %d", p->rtime, p->wtime, p->nrun, p->curq );
    for (int i = 0; i < 5; ++i)
    {
      cprintf(" \t %d",p->q[i]);
    }
    cprintf("\n");
  }
  else if(p->state == RUNNING){
    cprintf("%d \t %d \t ", p->pid,p->priority);
    cprintf("running");
    cprintf(" \t %d \t %d \t %d \t %d", p->rtime, p->wtime, p->nrun, p->curq );
    for (int i = 0; i < 5; ++i)
    {
      cprintf(" \t %d",p->q[i]);
    }
    cprintf("\n");
  }
  else if(p->state == RUNNABLE){
    cprintf("%d \t %d \t ", p->pid,p->priority);
    cprintf("runnable");
    cprintf(" \t %d \t %d \t %d \t %d", p->rtime, p->wtime, p->nrun, p->curq );
    for (int i = 0; i < 5; ++i)
    {
      cprintf(" \t %d",p->q[i]);
    }
    cprintf("\n");
  }
}
release(&ptable.lock);
return 1;
}

int
set_priority(int newPrio, int pid)
{
//  cprintf("  %d \n", pid);
  struct proc *p; int old=0;
  acquire(&ptable.lock);
  for(p=ptable.proc; p < &ptable.proc[NPROC]; p++){
  //  cprintf("%d\n",p->pid);
    if(p->pid == pid){
      old = p->priority;
      p->priority = newPrio;
     // cprintf("Changed priority of process %d from %d to %d\n", p->pid, old, p->priority);

      break;
    }
  }
  #ifdef PBS
  if(newPrio < old)
    yield();
  #endif
  release(&ptable.lock);
  return old;
}

int qrem(struct proc *p, int qno)
{
  int pflag = 0, rem = 0;
  for(int i=0; i <= cnt[qno]; i++)
  {
    if(queue[qno][i] -> pid == p->pid)
    {
      //cprintf("%d %d\n", p->pid, qno);
      rem = i;
      pflag = 1;
      break;
    }
  }

  if(pflag == 0)
  {
    return -1;
  }

  for(int i = rem; i < cnt[qno]; i++)
    queue[qno][i] = queue[qno][i+1]; 

  cnt[qno]--;
  return 1;

}

int qinsert(struct proc *p, int qno)
{ 

  for(int i=0; i < cnt[qno]; i++)
  {
    if(p->pid == queue[qno][i]->pid)
      return -1;
  }
  // cprintf("Process %d added to Queue %d\n", p->pid, q_no);
  p->ent = ticks;
//  p->wtime=0;
  p -> curq = qno;
  cnt[qno]++;
  queue[qno][cnt[qno]] = p;

  return 1;
}

void updateWaittime() {
    struct proc *p;
    acquire(&ptable.lock);

    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if(p->state == RUNNING)
          p->wtime =0;
        else if(p->state == SLEEPING)
          p->wtime =0;
        else if(p->state == RUNNABLE)
          p->wtime +=1;
    }

    release(&ptable.lock);
}
