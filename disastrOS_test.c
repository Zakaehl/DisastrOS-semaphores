#include <stdio.h>
#include <unistd.h>
#include <poll.h>

#include "disastrOS.h"

#define SIZE 5

//This is the shared resource, accessible only in critical section
//It contains SIZE numbers, and "0" denotes a free spot
int buf[SIZE] = {0};

void printBuf() {
	printf("|");
	for (int i=0; i<SIZE; ++i)
		printf(" %d |",buf[i]);
	printf("\n");
}

//When a producer accesses the shared buffer, it inserts its PID in the first free spot
void insert(int pid) {
	for (int i=0; i<SIZE; ++i) {
		if (buf[i] == 0) {
			buf[i] = pid;
			break;
		}
	}
}

//When a consumer accesses the shared buffer, it takes the number in the first occupied spot
void take() {
	for (int i=0; i<SIZE; ++i) {
		if (buf[i] !=0) {
			buf[i] = 0;
			break;
		}
	}
}

// we need this to handle the sleep state
void sleeperFunction(void* args){
  printf("Hello, I am the sleeper, and I sleep %d\n",disastrOS_getpid());
  while(1) {
    getc(stdin);
    disastrOS_printStatus();
  }
}

void childFunction(void* args){
  printf("Hello, I am the child function %d\n",disastrOS_getpid());
  printf("I will iterate a bit, before terminating\n");
  int type=0;
  int mode=0;
  int fd=disastrOS_openResource(disastrOS_getpid(),type,mode);
  printf("fd=%d\n", fd);
  printf("PID: %d, terminating\n", disastrOS_getpid());

  int mutex = disastrOS_semOpen(0,1);    //Grants mutual exclusion
  int fill  = disastrOS_semOpen(1,0);    //#elements in the buffer; if it's empty, the consumer waits
  int empty = disastrOS_semOpen(2,SIZE); //Free space in the buffer; if it's full, the producer waits
  int err   = disastrOS_semOpen(3,-3);   //A semaphore to test errors, its life is useless
  
  disastrOS_printStatus();

  for (int i=0; i<(disastrOS_getpid()+1); ++i){
    printf("PID: %d, iterate %d\n", disastrOS_getpid(), i);
    disastrOS_sleep((20-disastrOS_getpid())*5);
    
    //Processes with PID from 2 to 11 have access to the semaphores
    // and can be producers or consumers
    int pid = disastrOS_getpid();
    
	//Choose the PID of producers and consumers so that when the processes get terminated,
	// there are no processes that get stuck on the first wait and wait indefinitely (since
	// the processes that should call post on the semaphore were already terminated)
	//Some examples that work:
	// prod: 3          cons: 2          - size 1+
	// prod: 5,6,7      cons: 3,4,8      - size 3+
	// prod: 3,5,7,9,11 cons: 2,4,6,8,10 - size 5+
	// prod: 3,5,6,9,11 cons: 2,4,7,8,10 - size 3+ holy ship
    
    //PROCESS IS A PRODUCER
	if (pid%2 != 0) {
	  printf("Process %d (PRODUCER) wants to access the shared buffer: ",pid);
	  printBuf();
	  
	  disastrOS_semWait(empty);		//Id 2
	  disastrOS_semWait(mutex); 	//Id 0
	  
	  printf("CRITICAL SECTION: process %d (PRODUCER) is accessing the shared buffer\n",pid);
	  insert(pid);
	  printf("The buffer was modified by process %d (PRODUCER): ",pid);
	  printBuf();
	  
	  disastrOS_semPost(mutex); 	//Id 0
	  disastrOS_semPost(fill);  	//Id 1
	}
	
	//PROCESS IS A CONSUMER
    if (pid%2 == 0) {
	  printf("Process %d (CONSUMER) wants to access the shared buffer: ",pid);
	  printBuf();
	  
	  disastrOS_semWait(fill); 		//Id 1
	  disastrOS_semWait(mutex); 	//Id 0
	  
	  printf("CRITICAL SECTION: process %d (CONSUMER) is accessing the shared buffer\n",pid);
	  take();
	  printf("The buffer was modified by process %d (CONSUMER): ",pid);
	  printBuf();
	  
	  disastrOS_semPost(mutex); 	//Id 0
	  disastrOS_semPost(empty); 	//Id 2
	}
  }
  
  disastrOS_semClose(mutex);
  disastrOS_semClose(fill);
  disastrOS_semClose(empty);
  disastrOS_semClose(err);
  
  disastrOS_exit(disastrOS_getpid()+1);
}


void initFunction(void* args) {
  disastrOS_printStatus();
  printf("hello, I am init and I just started\n");
  disastrOS_spawn(sleeperFunction, 0);
  

  printf("I feel like to spawn 10 nice threads\n");
  int alive_children=0;
  for (int i=0; i<10; ++i) {
    int type=0;
    int mode=DSOS_CREATE;
    printf("mode: %d\n", mode);
    printf("opening resource (and creating if necessary)\n");
    int fd=disastrOS_openResource(i,type,mode);
    printf("fd=%d\n", fd);
    disastrOS_spawn(childFunction, 0);
    alive_children++;
  }

  disastrOS_printStatus();
  
  printf("The shared buffer has been initialized with %d free spaces: ",SIZE);
  printBuf();
  printf("\n");
  
  int retval;
  int pid;
  while(alive_children>0 && (pid=disastrOS_wait(0, &retval))>=0){ 
    disastrOS_printStatus();
    printf("initFunction, child: %d terminated, retval:%d, alive: %d \n",
	   pid, retval, alive_children);
    --alive_children;
  }
  printf("shutdown!");
  disastrOS_shutdown();
}

int main(int argc, char** argv){
  char* logfilename=0;
  if (argc>1) {
    logfilename=argv[1];
  }
  // we create the init process processes
  // the first is in the running variable
  // the others are in the ready queue
  printf("the function pointer is: %p", childFunction);
  // spawn an init process
  printf("start\n");
  disastrOS_start(initFunction, 0, logfilename);
  return 0;
}
