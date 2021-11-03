#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include "disastrOS.h"
#include "disastrOS_syscalls.h"
#include "disastrOS_semaphore.h"
#include "disastrOS_semdescriptor.h"

void internal_semClose(){
	//Retrieve the fd of the semaphore to close
	int fd = running->syscall_args[0];

	SemDescriptor* des = SemDescriptorList_byFd(&running->sem_descriptors,fd);
	//If the fd is not in the the process, we return an error
	if (!des) {
		running->syscall_retvalue = DSOS_ESEM_NO_FD;
		printf("ERROR %d: invalid semaphore descriptor for process %d\n",DSOS_ESEM_NO_FD,running->pid);
		return;
	}

	//We remove the descriptor from the process list
	des = (SemDescriptor*)List_detach(&running->sem_descriptors,(ListItem*)des);
	if (!des) {
		running->syscall_retvalue = DSOS_ESEM_NO_FD;
		printf("ERROR %d: process %d couldn't remove a semaphore descriptor correctly\n",DSOS_ESEM_NO_FD,running->pid);
		return;
	}

	Semaphore* sem = des->semaphore;

	//We remove the descriptor pointer from the resource list
	SemDescriptorPtr* desptr = (SemDescriptorPtr*)List_detach(&sem->descriptors,(ListItem*)(des->ptr));
	if (!desptr) {
		running->syscall_retvalue = DSOS_ESEM_NO_FD_PTR;
		printf("ERROR %d: process %d couldn't remove a semaphore descriptor pointer correctly\n",DSOS_ESEM_NO_FD_PTR,running->pid);
		return;
	}
	
	//Free the resources
	SemDescriptor_free(des);
	SemDescriptorPtr_free(desptr);
	
	printf("CLOSED SEMAPHORE %d by process %d\n",fd,running->pid);
	
	//If the semaphore is not used, free it
	if (sem->descriptors.size==0 && sem->waiting_descriptors.size==0) {
		sem = (Semaphore*)List_detach(&semaphores_list,(ListItem*)sem);
			if (!sem) {
				running->syscall_retvalue = DSOS_ESEM_NO_SEM;
				printf("ERROR %d: process %d coudln't remove a semaphore correctly\n",DSOS_ESEM_NO_SEM,running->pid);
				return;
			}
		Semaphore_free(sem);
		printf("FREED SEMAPHORE %d by process %d\n",fd,running->pid);
	}
	
	running->syscall_retvalue = 0;
}
