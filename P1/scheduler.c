/**
 * 
 * File             : scheduler.c
 * Description      : This is a stub to implement all your scheduling schemes
 *
 * Author(s)        : Hunter Schuler
 * Last Modified    : 10/12/2020
*/

// Include Files
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <time.h>

#include <math.h>
#include <pthread.h>

void init_scheduler(int schedType );
int schedule_me(float currentTime, int tid, int remainingTime, int tprio);
int P(float currentTime, int tid, int semid);
int V(float currentTime, int tid, int semid);

#define FCFS    0
#define SRTF    1
#define PBS     2
#define MLFQ    3
#define PRIO_MAX 5
#define SEM_ID_MAX 50

/**
* Data Structures
*/

// Boolean
typedef enum {false, true} bool;

// Thread
//	Thread ID
//	Thread Priority
//	Remaining Time
//	Running Time - Time running in priority
//	Semaphore - If not -1, semaphore being waited on
//	Thread Condition
typedef struct Thread {
	int threadId;
	int threadPrio;
	int remainingTime;
	int runningTime;
	int semaphore;
	pthread_cond_t threadCond;
} Thread;

// Thread list
//	List of available threads, either stopped or queued
struct Thread **availableThreads;

// Thread queue
//	List of threads that are running on CPU
//	Keep each seperate for different lengths
typedef struct {
	int size;
	struct Thread **items;
} Queue;

Queue queue0;
Queue queue1;
Queue queue2;
Queue queue3;
Queue queue4;
Queue threadQueue[PRIO_MAX];

// Available semaphores
//	List of available semaphores
bool availableSemaphores[SEM_ID_MAX];

pthread_mutex_t cpuMutex = PTHREAD_MUTEX_INITIALIZER;

int cpuTime = 0;
int cpuType = -1;
int currentThread = -1;
int previousThread = -1;
int threadCount = 0;

/**
* Helper Functions
*/

/**
* Find Thread
* * Finds the thread pointer from available threads given tid
* @param tid the thread ID to be found
*/
struct Thread *findThread(int id) {
	struct Thread *thread = malloc(sizeof(struct Thread));
	thread->threadId = -1;
	thread->remainingTime = -1;
	thread->threadPrio = -1;
	for (int i = 0; i < threadCount; i++) {
		if (availableThreads[i]->threadId == id) {
			free(thread);
			return availableThreads[i];
		}
	}
	pthread_cond_init(&(thread->threadCond), NULL);
	return thread;
}

/**
* Enqueue
* * Enqueues the given tid to the the given queue priority level
* * Follows expected preempting procedures for cpuType
* @param qLevel the given priority queue of the element
* @param tid the thread ID to be enqueued
*/
void enque(int qLevel, struct Thread *thread) {
	int qElemCount = threadQueue[qLevel].size;
	// Copy elements to a temporary queue
	struct Thread **tempQ = malloc(sizeof(struct Thread *) * (qElemCount + 1));
	int j = 0;
	bool requeue = false;
	for (int i = 0; i < qElemCount; i++) {
		if (threadQueue[qLevel].items[i]->threadId == thread->threadId) { // Test for requeue
			threadQueue[qLevel].size--;
			requeue = true;
			continue;
		}
		tempQ[j] = threadQueue[qLevel].items[i];
		j++;
	}
	if (requeue) qElemCount--;
	tempQ[qElemCount] = thread;

	// Realloc queue level and insert new thread
	threadQueue[qLevel].items = realloc(threadQueue[qLevel].items, sizeof(struct Thread *) * (qElemCount + 1));
	if (cpuType == FCFS) {
		// Add to the end of the thread queue
		for (int i = 0; i < qElemCount; i++) {
			threadQueue[qLevel].items[i] = tempQ[i];
		}
		threadQueue[qLevel].items[qElemCount] = thread;
	} else if (cpuType == SRTF) {
		// If it has a shorter time, set the next thread to this
		// Sorts tempQ by time remaining and copies back
		for (int i = 0; i <= qElemCount; i++) {
			if (tempQ[i]->remainingTime > tempQ[qElemCount]->remainingTime) {
				struct Thread *tempThread = tempQ[i];
				tempQ[i] = tempQ[qElemCount];
				tempQ[qElemCount] = tempThread;
			}
			threadQueue[qLevel].items[i] = tempQ[i];
		}
	} else if (cpuType == PBS) {
		// If it has a higher priority than current, set next thread to this
		for (int i = 0; i <= qElemCount; i++) {
			if (tempQ[i]->threadPrio > tempQ[qElemCount]->threadPrio) {
				struct Thread *tempThread = tempQ[i];
				tempQ[i] = tempQ[qElemCount];
				tempQ[qElemCount] = tempThread;
			}
			threadQueue[qLevel].items[i] = tempQ[i];
		}
	} else if (cpuType == MLFQ) {
		// Add to the end of the appropriate thread queue
		for (int i = 0; i < qElemCount; i++) {
			threadQueue[qLevel].items[i] = tempQ[i];
		}
		threadQueue[qLevel].items[qElemCount] = thread;
	}

	threadQueue[qLevel].size++;
	free(tempQ);

	// Update current thread
	currentThread = -1;
	for (int i = 0; i < PRIO_MAX; i++) {
		if (threadQueue[i].size > 0) {
			currentThread = threadQueue[i].items[0]->threadId;
			break;
		}
	}
}

/**
* Dequeue
* * Dequeues the given tid from the the given queue priority level
* @param qLevel the given priority queue of the element
* @param tid the thread ID to be dequeued
*/
void dequeue(int qLevel, int tid) {
	qLevel--;
	int qElemCount = threadQueue[qLevel].size;
	// Copy elements to a temporary queue
	struct Thread **tempQ = malloc(sizeof(struct Thread) * (qElemCount - 1));
	int j = 0;
	for (int i = 0; i < qElemCount; i++) {
		if (threadQueue[qLevel].items[i]->threadId == tid) continue;
		tempQ[j] = threadQueue[qLevel].items[i];
		j++;
	}

	// Realloc queue level and copy back
	threadQueue[qLevel].items = realloc(threadQueue[qLevel].items, sizeof(struct Thread *) * (qElemCount - 1));
	for (int i = 0; i < (qElemCount-1); i++) {
		threadQueue[qLevel].items[i] = tempQ[i];
	}

	threadQueue[qLevel].size--;
	free(tempQ);

	// Update current thread
	currentThread = -1;
	for (int i = 0; i < PRIO_MAX; i++) {
		if (threadQueue[i].size > 0) {
			currentThread = threadQueue[i].items[0]->threadId;
			break;
		}
	}
}

/**
* Init Scheduler
* * Invoked in beginning to initialize according to scheduler type
* @param schedType 0-FCFS 1-SRTF 2-PBS 3-MLFQ
*/
void init_scheduler(int schedType) {
	cpuType = schedType;

	for (int i = 0; i < SEM_ID_MAX; i++) {
		availableSemaphores[i] = false;
	}

	threadQueue[0] = queue0;
	threadQueue[1] = queue1;
	threadQueue[2] = queue2;
	threadQueue[3] = queue3;
	threadQueue[4] = queue4;
	for (int i = 0; i < PRIO_MAX; i++) {
		threadQueue[i].size = 0;
	}
}

/**
* Schedule Me
* * Thread calls function every timer tick and only return when CPU
* * is given until the next timer tick, else block execution.
* * On each return, remainingTime should decrease
* @param currentTime last current time returned to thread
* @param tid thread identification number
* @param remainingTime amount of remaining CPU time requested
* @param tPrio thread priority, 1-highest 5-lowest
* @return global time on CPU
*/
int schedule_me(float currentTime, int tid, int remainingTime, int tPrio) {
	pthread_mutex_lock(&cpuMutex);
	struct Thread *thisThread = findThread(tid);
	if (currentTime > cpuTime) cpuTime = round(currentTime+.49);

	if (thisThread->threadId == -1 || (cpuType == SRTF && remainingTime > thisThread->remainingTime) || (cpuType == PBS && tPrio != thisThread->threadPrio)) {
		if (thisThread->threadId == -1) {
			threadCount++;
			availableThreads = realloc(availableThreads, sizeof(struct Thread *) * threadCount);
			availableThreads[threadCount-1] = thisThread;
			thisThread->threadId = tid;
			if (cpuType == MLFQ) tPrio = 1;
			thisThread->threadPrio = tPrio;
			thisThread->runningTime = 0;
			thisThread->semaphore = -1;
		}
		
		thisThread->remainingTime = remainingTime;
		if (!(thisThread->semaphore > 0)) enque(0, thisThread);
	}

	// Update thread data
	thisThread->remainingTime = remainingTime;

	// Age threads for MLFQ
	if (cpuType == MLFQ && thisThread->runningTime == 5 * thisThread->threadPrio && !(thisThread->semaphore > 0)) {
		dequeue(thisThread->threadPrio, thisThread->threadId);
		thisThread->runningTime = 0;
		if (thisThread->threadPrio < 5) {
			enque(thisThread->threadPrio++, thisThread);
		} else {
			enque(thisThread->threadPrio-1, thisThread);
		}
	} else {
		thisThread->runningTime++;
	}

	// Thread waiting
	if (currentThread > 0 && tid != currentThread) {
		if (previousThread == thisThread->threadId) {
			pthread_mutex_unlock(&cpuMutex);
			pthread_cond_signal(&(findThread(currentThread)->threadCond));
		}
		pthread_cond_wait(&(thisThread->threadCond), &cpuMutex);
	}

	// Check if dequeue
	if (remainingTime == 0 && tid > 0) {
		if (cpuType != MLFQ) {
			dequeue(1, tid);
		} else {
			dequeue(thisThread->threadPrio, tid);
		}
	}

	previousThread = thisThread->threadId;

	pthread_mutex_unlock(&cpuMutex);
	if (remainingTime == 0) {
		if (currentThread > 0) pthread_cond_signal(&(findThread(currentThread)->threadCond));
		if (currentThread == -1) {
			// Free each thread
			for (int i = 0; i < threadCount; i++) {
				pthread_cond_destroy(&(availableThreads[i]->threadCond));
				free(availableThreads[i]);
				availableThreads[i] = NULL;
			}

			// Free available thread memory
			free(availableThreads);
			availableThreads = NULL;
			threadCount = 0;

			// Free each thread queues items
			for (int i = 0; i < PRIO_MAX; i++) {
				free(threadQueue[i].items);
				threadQueue[i].items = NULL;
			}
		}
	}

	return cpuTime;
}

/**
* Sempahore Wait
* * Implements wait function for semid.
* * Blocks until semaphore receives signal
* @param currentTime last current time returned to thread
* @param tid thread identification number
* @param semid semaphore identification number
* @return global time on CPU
*/
int P(float currentTime, int tid, int semid) {
	pthread_mutex_lock(&cpuMutex);
	if (currentTime > cpuTime) cpuTime = (int)(currentTime+.49);

	// Don't service higher semaphore ID
	if (semid > SEM_ID_MAX) return cpuTime;

	// Block the thread if the semaphore is not free
	if (!availableSemaphores[semid-1]) {
		struct Thread *thread = findThread(tid);
		if (thread->threadId < 0) return -1;
		thread->semaphore = semid;
		// Dequeue current thread
		if (cpuType != MLFQ) {
			dequeue(1, thread->threadId);
		} else {
			dequeue(thread->threadPrio, thread->threadId);
		}
		// Start next thread
		pthread_mutex_unlock(&cpuMutex);
		pthread_cond_signal(&(findThread(currentThread)->threadCond));
	} else {
		pthread_mutex_unlock(&cpuMutex);
		availableSemaphores[semid-1] = false;
	}

	return cpuTime;
}

/**
* Sempahore Signal
* * Implements signal function for semid.
* @param currentTime last current time returned to thread
* @param tid thread identification number
* @param semid semaphore identification number
* @return global time on CPU
*/
int V(float currentTime, int tid, int semid){
	pthread_mutex_lock(&cpuMutex);
	if (currentTime > cpuTime) cpuTime = (int)(currentTime+.49);

	// Don't service higher semaphore ID
	if (semid > SEM_ID_MAX) return cpuTime;

	// Service the semaphore
	// Search through available, non-blocked semaphores
	// Start highest ranked demepending on CPU Type
	struct Thread **potentialThreads = malloc(sizeof(struct Thread));
	int potentialThreadCount = 0;
	for (int i = 0; i < threadCount; i++) {
		if (availableThreads[i]->semaphore == semid) {
			potentialThreadCount += 1;
			potentialThreads = realloc(potentialThreads, sizeof(int)*potentialThreadCount);
			potentialThreads[potentialThreadCount-1] = availableThreads[i];
		}
	}

	if (potentialThreadCount != 0) {
		if (cpuType == FCFS) {
			// Enque first thread found
			potentialThreads[0]->semaphore = -1;
			enque(0, potentialThreads[0]);
		} else if (cpuType == SRTF) {
			// Enque the thread with the shortest remaining time
			int minRemainingTime = __INT_MAX__;
			struct Thread *minThread;
			for (int i = 0; i < potentialThreadCount; i++) {
				if (potentialThreads[i]->remainingTime < minRemainingTime) {
					minThread = potentialThreads[i];
					minRemainingTime = potentialThreads[i]->remainingTime;
				}
			}
			minThread->semaphore = -1;
			enque(0, minThread);
		} else if (cpuType == PBS) {
			// Enque first highest priority thread
			int minPrio = 6;
			struct Thread *minThread;
			for (int i = 0; i < potentialThreadCount; i++) {
				if (potentialThreads[i]->threadPrio < minPrio) {
					minThread = potentialThreads[i];
					minPrio = potentialThreads[i]->threadPrio;
				}
			}
			minThread->semaphore = -1;
			enque(0, minThread);
		} else if (cpuType == MLFQ) {
			// Enque first highest priority thread
			int minPrio = 6;
			struct Thread *minThread;
			for (int i = 0; i < potentialThreadCount; i++) {
				if (potentialThreads[i]->threadPrio < minPrio) {
					minThread = potentialThreads[i];
					minPrio = potentialThreads[i]->threadPrio;
				}
			}
			minThread->semaphore = -1;
			enque(minPrio-1, minThread);
		} else {
			return -1;
		}
	} else {
		availableSemaphores[semid-1] = true;
	}

	free(potentialThreads);
	pthread_mutex_unlock(&cpuMutex);
	return cpuTime;
}