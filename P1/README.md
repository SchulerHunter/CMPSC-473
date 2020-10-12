CPU Scheduler
=====================

- To run the scheduler, run `make` followed by `./out <cpuType> <InputFile>`
    - `<cpuType>` is an integer of 0 - 3 denoting the scheduler style.
        - 0 - First Come First Serve (FCFS): The first thread scheduled is run to completion or block. Does not implement preempting.

        - 1 - Shortest Remaining Time First (SRTF): The thread with the shortest remaining CPU burst is scheduled. On each enque, if the new scheduled thread has a shorter burst remaining, it preempts the current thread.

        - 2 - Priority Based Scheduling (PBS): The thread with the highest priority is run first. If a new thread is queued with a higher priority, it preempts the current thread.

        - 3 - Multi-level Feedback Queue (MLFQ): Each thread joins the highest priority queue. Each priority queue runs for a burst length equal to `5 *  <PriorityLevel>`. At the end of each burst, the thread is moved to the next lowest, until in the 5th queue where it runs as a round-robin threading style.

    - `<InputFile>` is a multi-line file with each line representing a CPU thread. Each line is of the form `<entryTime> <threadId> <threadPriority> (<cpuBurstTime>|<SemaphoreWait+ID>|<SemaphoreSignal+ID>)*`
        - `<entryTime>` -  Global time that thread enters and is scheduled to CPU.
        - `<threadId>` - Unique ID to identify thread in scheduler.
        - `<threadPriority>` - Priority level for thread. Each priority is an integer between 1 - 5.
            - 1 is the highest priority.
            - 5 is the lowest priority.
        - `<cpuBurstTime>` - The time the CPU needs for the thread to reach its next call or finish
        - `<SemaphoreWait+ID>` - Takes the form of `P#` where `#` is any number between 1 and 50. This blocks the thread from queueing in the ready queue until a signal is received.
        - `<SemaphoreSignal+ID>` - Takes the form of `V#` where `#` is any number between 1 and 50. This signals that the semaphore is available. If no thread is waiting, the signal is stored as available.

- With each call to an interface function in `scheduler.c`, a CPU  mutex is locked to assure thread stability when accessing the thread queue.
- To keep track of threads, the `scheduler.c` file maintains a thread-safe list of available thread pointers that have been scheduled. The file also maintains a thread-safe multi-level ready queue. The head of this queue is the next running thread.
- To maintain queue order and preempt threads, the queue and running thread are updated on each enqueue and deqeueue of a thread. If this changes the current running thread, that running thread finishes and alerts the next thread to run using a its own condition signal.
- `schedule_me` is called on each thread and halts the thread using a conditional wait. A thread runs until completion or preemption.
- `P` is called when a thread must wait for a semaphore. If the semaphore is already signalled, then the thread does not halt unless preempted.
- `V` is called when a thread is signalling a semaphore. 
    - If another thread is waiting for the semaphore, the thread is enqued. 
    - If multiple threads are waiting, the thread with the highest priority based on the scheduling scheme is selected.
        - FCFS - The first thread found that is waiting is re-enqued.
        - SRTF - The first thread found with the shortest remaining time is re-enqued.
        - PBS - The first thread found with the the highest priority is re-enqued.
        - MLFQ - The first thread with the highest priority is re-enqued at the end of its appropriate priority.
    - If no thread is waiting, the semaphore is marked as available.
- `enque` is called when a thread is being pushed to a thread queue. Within the `enque` function, the location in the queue is determined by the CPU type. Swapping is used to organize the queue based on the relevant value.
- `dequeue` is called when a thread should be removed from a queue. This is called when the thread is waiting for a semaphore, time remaining becomes zero, or it ages out of the current priority in MLFQ scheduling.
- All variables that are allocated is freed before completion. Code is memory efficient.