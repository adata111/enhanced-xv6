#include "types.h"
#include "stat.h"
#include "user.h"

int 
main(int argc, char *argv[])
{
 int pid;
 int status=-1;
 int waittime=0,runtime=0;	
 
 pid = fork ();
 if (pid == 0)
   {	
   exec(argv[1],argv+1);
    printf(1, "exec %s failed\n", argv[1]);
    }
  else
 {
    status=waitx(&waittime,&runtime);
 }  
 printf(1, "Wait Time (sleep time not included) = %d\nRun Time = %d\nStatus %d \n", waittime, runtime, status); 
 exit();
}