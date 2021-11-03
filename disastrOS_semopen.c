#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include "disastrOS.h"
#include "disastrOS_syscalls.h"
#include "disastrOS_semaphore.h"
#include "disastrOS_semdescriptor.h"

void internal_semOpen() {
	//Get from the PCB the semaphore id of the semaphore to open
	int id = running->syscall_args[0];
	int count = running->syscall_args[1];
	
	//A semaphore has to be initialized with a non-negative value
	if (count < 0) {
		running->syscall_retvalue = DSOS_ESEM_NEG_INIT;
		printf("ERROR %d: attempt by process %d to create a semaphore (id: %d) with a negative count\n",DSOS_ESEM_NEG_INIT,running->pid,id);
		return;
	}
	
	Semaphore* sem = SemaphoreList_byId(&semaphores_list,id);
	
	//The semaphore is allocated only if it doesn't exist; if it does, then
	// the current process only handles the new semdescriptor
	if (!sem) {
		sem = Semaphore_alloc(id,count);
		//At this point we should have the semaphore, if not something was wrong
		if (!sem || sem->count!=count) {
			running->syscall_retvalue = DSOS_ESEM_ALLOC;
			printf("ERROR %d: process %d failed to allocate semaphore %d\n",DSOS_ESEM_ALLOC,running->pid,id);
			return;
		}
		List_insert(&semaphores_list,semaphores_list.last,(ListItem*)sem);
		printf("CREATED SEMAPHORE with id %d and count %d by process %d\n",id,count,running->pid);
	}
	
	//Create the descriptor for the semaphore in this process, and add it to
	// the process semaphore descriptor list. Assign to the semaphore a new fd
	SemDescriptor* des = SemDescriptor_alloc(running->last_sem_fd,sem,running);
	if (!des) {
		running->syscall_retvalue = DSOS_ESEM_FD_ALLOC;
		printf("ERROR %d: process %d failed to allocate the descriptor of semaphore %d\n",DSOS_ESEM_FD_ALLOC,running->pid,id);
		return;
	}
	running->last_sem_fd++; //We increment the fd value for the next call
	
	SemDescriptorPtr* desptr = SemDescriptorPtr_alloc(des);
	if (!desptr) {
		running->syscall_retvalue = DSOS_ESEM_FD_PTR_ALLOC;
		printf("ERROR %d: process %d failed to allocate the pointer to the descriptor of semaphore %d\n",DSOS_ESEM_FD_PTR_ALLOC,running->pid,id);
		return;
	}
	
	List_insert(&running->sem_descriptors,running->sem_descriptors.last,(ListItem*)des);
	
	//Add to the semaphore, in the descriptor ptr list, a pointer to the newly
	// created descriptor
	des->ptr = desptr;
	List_insert(&sem->descriptors,sem->descriptors.last,(ListItem*)desptr);

	//Return the FD of the new descriptor to the process
	running->syscall_retvalue = des->fd;
	
	printf("OPENED SEMAPHORE %d by process %d\n",id,running->pid);
}
