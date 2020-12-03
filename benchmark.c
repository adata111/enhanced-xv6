
#include "types.h"
#include "user.h"

int number_of_processes = 10;

int main(int argc, char *argv[])
{
  int j;
  for (j = 0; j < number_of_processes; j++)
  {
    int pid = fork();
    if (pid < 0)
    {
      printf(1, "Fork failed\n");
      continue;
    }
    if (pid == 0)
    {
      volatile int i;
      for (volatile int k = 0; k < number_of_processes; k++)
      {
        if (k <= j)
        {
          sleep(200); //io time
        }
        else
        {
        //  break;
          for (i = 0; i < 100000000; i++)
          {
             ;//cpu time
            
          }
           // exec(argv[1],argv+1);

          
        }
      }
//  procs();
    //   printf(1, "Process: %d Finished\n", j);
      exit();
    }
    else{
    //    printf(1, "****** %d %d\n", 100-(20+j), pid);
      #ifdef PBS
        set_priority(100-(20+j),pid); // will only matter for PBS, comment it out if not implemented yet (better priorty for more IO intensive jobs)
      #endif
    }
  }
 int status=-1;
 int waittime=0,runtime=0;
  for (j = 0; j < number_of_processes+5; j++)
  {
    status=waitx(&waittime,&runtime);
    if(status!=-1){
      printf(1, "Wait Time (sleep time not included) = %d\nRun Time = %d\nStatus %d \n", waittime, runtime, status); 
    }
  }
  procs();
  exit();
}
