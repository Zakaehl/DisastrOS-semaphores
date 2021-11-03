#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include "disastrOS.h"
#include "disastrOS_syscalls.h"
#include "disastrOS_semaphore.h"
#include "disastrOS_semdescriptor.h"

void internal_semPost(){
	//Get from the PCB the semaphore id of the semaphore whose count has to be incremented
	int fd = running->syscall_args[0];
	
	SemDescriptor* des = SemDescriptorList_byFd(&running->sem_descriptors,fd);
	//If the fd is not in the the process, we return an error
	if (!des) {
		running->syscall_retvalue = DSOS_ESEM_NO_FD;
		printf("ERROR %d: invalid semaphore descriptor for process %d\n",DSOS_ESEM_NO_FD,running->pid);
		return;
	}
	
	//Take the Semaphore from the semaphore descriptor
	Semaphore* sem = des->semaphore;
	
	//Count gets incremented
	printf("CALLED POST on semaphore %d by process %d - count: %d -> ",fd,running->pid,sem->count);
	sem->count++;
	printf("%d\n",sem->count);
	
	if (sem->count <= 0) {
		//Take a waiting descriptor and move it from the waiting descriptors
		SemDescriptorPtr* desptr = (SemDescriptorPtr*)List_detach(&sem->waiting_descriptors,sem->waiting_descriptors.first);
		List_insert(&sem->descriptors,sem->descriptors.last,(ListItem*)desptr);
		//Take the waiting process, which then becomes ready
		PCB* new_ready_process = desptr->descriptor->pcb;
		new_ready_process->status = Ready;
		List_detach(&waiting_list,(ListItem*)new_ready_process);
		List_insert(&ready_list,ready_list.last,(ListItem*)new_ready_process);
		printf("Process %d is now READY\n",new_ready_process->pid);
	}
	
	running->syscall_retvalue = 0;
}
