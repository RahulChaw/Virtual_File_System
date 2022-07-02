# Virtual_File_System
Implementation of virtual disk and file system in C

#Queue implementation:
Queue_create: We allocate memory using malloc to create the queue and return the queue pointer. 

Queue_destroy: we dellocate the memory of the queue struct using free and return 0. We check for the edge condition and return -1 if the queue does not exist of the queue is not empty. 

Queue_iterate: Our queue is implemented as a linked list, so we used next pointer in each node to go through the queue and used the function calling using function pointer to call the function on each item of the queue with the provided argument. We check for the edge condition if the queue does not exist and return -1. 
//(add an edge condition to check if the queue is empty)

Queue_length: Since our queue is implemented as a linked list and we don’t have a counter for the number of nodes or serial number for each node, we have to go through the whole queue using the next pointer and increment a counter to find out the length of the queue. We implemented the edge conditions and return -1 when the queue does not exist and return 0 when the queue is empty. 

#Uthread Implementation:
Uthread_stop: We check if all the queues (ready, zombie, blocked) are empty and destroy them if they are and return 0. 
We check for the edge conditions i.e., if any of the queues are not empty and return -1. 

Uthread_yield: We dequeue the running and ready queue (i.e., get the running thread and the ready thread on top of the queue) and context switch them and change their states before pushing them on their respective queues (i.e., the thread which was on the top of ready queue is pushed on the running queue and the running thread at the end of ready queue.)
//(add edge case if ready queue is empty)

Uthread_exit: We dequeue the current running thread from running library. 
Now first we check if the child has returned for this thread, if not then we join the child thread. Then we let the thread run until its stack is empty and switch the context with the thread on top of ready queue.
 And then if the thread to be deleted is not in zombie state, then we change it to zombie state and push it on zombie queue. If it is already in zombie state, then we use queue_delete to delete the thread. Finally, we push the thread on top of ready queue to the running queue.

Uthread_join: We have 2 cases the thread to be joined can either be completed (in zombie queue) or still need to finish execution (in ready queue). First we check if it is in zombie queue, but dequeuing in the whole queue and saving it in a copy, and the restoring the zombie queue again. If it was in the zombie queue, we call uthread_exit to delete and collect the thread. 
