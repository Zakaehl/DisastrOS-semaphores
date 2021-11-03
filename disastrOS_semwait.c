#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include "disastrOS.h"
#include "disastrOS_syscalls.h"
#include "disastrOS_semaphore.h"
#include "disastrOS_semdescriptor.h"

void internal_semWait(){
	//Get from the PCB the semaphore id of the semaphore whose count has to be decremented
	int fd = running->syscall_args[0];
	
	SemDescriptor* des = SemDescriptorList_byFd(&running->sem_descriptors,fd);
	//If the fd is not in the the process, we return an error
	if (!des) {
		running->syscall_retvalue = DSOS_ESEM_NO_FD;
		printf("ERROR %d: invalid semaphore descriptor for process %d\n",DSOS_ESEM_NO_FD,running->pid);
		return;
	}

	//Take the Semaphore and the SemDescriptorPtr* from the semaphore descriptor
	Semaphore* sem = des->semaphore;
	SemDescriptorPtr* desptr = des->ptr;
	
	//Count gets decremented
	printf("CALLED WAIT on semaphore %d by process %d - count: %d -> ",fd,running->pid,sem->count);
	sem->count--;
	printf("%d\n",sem->count);
	
	if (sem->count < 0) {
		//Move the descriptor to the waiting ones
		List_detach(&sem->descriptors,(ListItem*)desptr);
		List_insert(&sem->waiting_descriptors,sem->waiting_descriptors.last,(ListItem*)desptr);
		//The running process now waits, then set the new running process
		PCB* next_running_process = (PCB*)List_detach(&ready_list,ready_list.first);
		List_insert(&waiting_list,waiting_list.last,(ListItem*)running);
		running->status = Waiting;
		printf("Process %d is now WAITING - ",running->pid);
		running = next_running_process;
		running->status = Running;
		printf("Process %d is now RUNNING\n",running->pid);
		disastrOS_printStatus();
	}
	
	running->syscall_retvalue = 0;
}
