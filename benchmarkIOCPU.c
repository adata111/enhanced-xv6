
#include "types.h"
#include "user.h"

int number_of_processes = 10;

int main(int argc, char *argv[])
{
  int j,b;
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
        //  for(b=0;b<=5;b++){
        //  	printf(1, "after %d sleep\n", b*50);
        //  	procs();
          	sleep(200); //io time
       //   }
         // 	printf(1, "after %d sleep\n", b*50);
        //  procs();
        }
        else
        {
          for (i = 0; i < 100000000; i++)
          {
            ; //cpu time
          }
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
    /*if(j<10) {
    	sleep(50);
    	printf(1, "after %d sleep\n", j*50);
    	procs();
    }*/
  }
  /*for (b = 5; b < 10; ++b)
  {
  	sleep(50);
    	printf(1, "after %d sleep\n", b*50);
    	procs();
  }*/
 int status=-1;
 int waittime=0,runtime=0;
  for (j = 0; j < number_of_processes+5; j++)
  {
    status=waitx(&waittime,&runtime);
    if(status!=-1){}
 //     printf(1, "Wait Time (sleep time not included) = %d\nRun Time = %d\nStatus %d \n", waittime, runtime, status); 
 
  }
  procs();
  exit();
}
