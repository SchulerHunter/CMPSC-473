CMPSC 473 - Operating System Design
=
## Overview
This is the code for Penn State's Computer Science 473 Course from the Fall of 2020.
## P1
P1 implements a simulation for four types of CPU thread scheduling as follows:
- 0 - First Come First Serve (FCFS): The first thread scheduled is run to completion or block. Does not implement preempting.

- 1 - Shortest Remaining Time First (SRTF): The thread with the shortest remaining CPU burst is scheduled. On each enque, if the new scheduled thread has a shorter burst remaining, it preempts the current thread.

- 2 - Priority Based Scheduling (PBS): The thread with the highest priority is run first. If a new thread is queued with a higher priority, it preempts the current thread.

- 3 - Multi-level Feedback Queue (MLFQ): Each thread joins the highest priority queue. Each priority queue runs for a burst length equal to `5 *  <PriorityLevel>`. At the end of each burst, the thread is moved to the next lowest, until in the 5th queue where it runs as a round-robin threading style.

## P2
P2 implements a simulation for two types of dynamic memory allocators as follows:
- 0 - Buddy Allocation: A memory partition technique which divides the memory in half until a sufficient size is found. The minimum size for a memory block is 1KB. A list of available holes is maintained as an array of linked list object pointing in order of ascending memory address.

- 1 - Slab Allocation: A memory technique which utilizes buddy allocation for a slab where each slab will contain a number of items of the same size. In this implementation, the number of items per slab is 64. Implements a slab descriptor table to track all slabs of each memory object size. Each entry in the table contains a start to a linked list with each object being a slab in the table, ordered by ascending address.

## P3
P3 implements a simulation of two types of virtual memory paging algorithms as follows:
- 1 - First In First Out (FIFO): A page management algorithm where the first page allocated is the first page removed when a new page needs to be allocated but there is no space. This does no account for any recent access to the page and will simply evict the oldest.

- 2 - Third Chance Replacement: A modification to the second chance replacement algorithm. This algorithm cycles through all pages, and if a page is in physical memory, will offer two chances before replacement. The first is by reading the `referenced` bit and the second by one of the `modified` bits. If both of those bits are 0, the page will be evicted, and written to disk if need be.