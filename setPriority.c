#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int
main(int argc, char *argv[])
{
  int newPr, pid;
  if(argc < 3){
    printf(2,"Usage: setPriority new_priority pid\n");
    exit();
  }
  newPr = atoi(argv[1]);
  pid = atoi(argv[2]);
  if (newPr < 0 || newPr > 100){
    printf(2,"Invalid priority! Please enter a priority in the range[0,100]\n");
    exit();
  }
  set_priority(newPr, pid);
  //printf(1, "%d %d\n", pid,chprio(newPr, pid));
  exit();
}