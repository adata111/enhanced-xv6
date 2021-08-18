
### waitx
waitx() system call has been implemented by using the code for wait() system call and modifying it a little. The calculation of wait time and run time has been added to the code. RIn sysproc.c, sys_waitx() passes parameters rtime and wtime to waitx() in proc.c .

The files modified are:
- proc.c 	-> the waitx() function is added.
- proc.h 	-> added ctime (creation time), rtime( run time), etime(end time) and iotime(I/O time) to the proc struct.

- ctime is set in allocproc() function of proc.c
- etime is set  int exit() function of proc.c 
- rtime is updated in trap() function of trap.c [if state of the process is RUNNING, then increase rtime by one]
- iotime is updated in trap() function of trap.c [if state of the process is SLEEPING, then increase iotime by one]
- wtime is the time spent by a process waiting for the CPU i.e. wtime = etime-ctime-rtime-iotime

### time
time command has also been implemented. It takes a command as input and runs it using exec but uses waitx() instead of wait(). Thus at the end it prints rtime, wtime and status(status is same as that returned by wait())

### ps
ps is a user program that prints the details of all the processes in the process table. This is accomplished by calling the system call procs() that I have implemented in proc.c. This syscall procs() loops over all processes in the process table and prints their information if they have a pid>0. 
New fields like nrun, wtime, curq and an array q[5] have been added to the proc structure in proc.h for number of ticks that the the process has been running, time for which the process has been waiting, current queue of the process and the ticks spent in each of the 5 queues respectively.


### FCFS

FCFS scheduling has been implemented by simply looping over all the processes in the process table and selecting the one with the lowest creation time as the next process to be picked up by the scheduler. We also check if the process is "RUNNABLE".

The following files have been changed:
- proc.c 	-> the main code is written in scheduler() function and a few initializations done in allocproc() function.
- trap.c 	-> the yield() function call has not been done from trap() function if the scheduling policy is fcfs. This is because we don't want the process to return control to scheduler after every clock tick.


### PBS

Priority based scheduling has been implemented by looping over all the processes in the process table and finding the process with minimum value of priority(i.e. highest priority, I've assumed smaller numbers to represent hegher priority). Initially all processes are assigned to 60. This highest priority process is selected as the process to be picked up by the scheduler next. This is pre-emptive , when a process with higher priority is found, the current process is preempted. If there are multiple processes with the same priority then they are chosen in round robin fashion. This is done by a nested for loop. When a process gets yielded, any other process with same priority is executed next.
A system call set_priority has been implemented which is called by the setPriority user program. set_priority takes newPriority and pid as arguments, loops over all processes to find the process with the given pid and resets the priority of that process to the newPriority and finally returns the old priority of that process. setPriority user program also checks if the input priority is valid i.e. it is in the range [0,100]. 
If a process priority is changed using setPriority user program, yield() is called if the new priority the process has a value lower than the previous priority.

The following files have been changed:
- proc.c 			-> the main code for PBS has been implemented in scheduler() function.
- proc.h 			->added priority to proc struct
- setPriority.c 	->added to implement the user program
- the six files modified to account for set_priority system call and setPriority user program 


### MLFQ

Implemented MLFQ scheduler with five queues(arrays); the top queue (numbered 0) has the highest priority and the bottom queue (numbered 4) has the lowest priority. When a process uses up its time-slice, it gets downgraded to the next (lower) priority level. The time-slices are specified in a an array which is checked every time control returns to the scheduler. If the ticks of the process is more than the time slice for that priority level, the process is shifted to the lower priority level. For the bottom queue if there is more than one process, they will be scheduled in Round Robin policy. If any process in queues 1-4 has been waiting for more than 30 ticks it is moved to the higher priority queue.
```c
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
```
Function to add a process to a queue :- qinsert()
Function to remove a process from a queue :- qrem()
If a process voluntarily relinquishes control of the CPU, it leaves the queuing
network, and when the process becomes ready again after the I/O, it is inserted
at the tail of the same queue, from which it is relinquished earlier. **This can be exploited by a process if it issues an I/O operation just before the timer tick occurs and thus relinquishes the CPU. In this way the process remains in the same priority queue and thus gets a higher percentage of the CPU time.**

The following files have been changed:
- proc.c 	-> mainly scheduler function has been changed to implement mlfq. Other than that kill(), userinit() and fork() call qinsert() whenever a process becomes runnable
- proc.h 	-> added curr_ticks, change_q, ent to proc struct. ent stores the time at which the process entered a queue.
- trap.c 	-> we check if the curr_ticks of the process exceeds the time-slice for that priority queue. If it does then we call yield() and insert the process to the lower queue. Else we just increase the curr_ticks for that process and it remains in the same queue.
- defs.h 	-> to add the functions change_qflag() and incr_curr_ticks()
