// Include files
#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <signal.h>
#include "473_mm.h"

/**
* Data Structures
*/

// Page fault error codes
typedef enum {
    PF_PROT  = 1 << 0,
    PF_WRITE = 1 << 1,
    PF_USER  = 1 << 2,
    PF_RSVD  = 1 << 3,
    PF_INSTR = 1 << 4
} pfErrorCode;

// Page fault fault type
//  ReadNPP - Read on non-present page
//  WriteNPP - Write on non-present page
//  WriteRO - Write on a read-only page
//  ReadRW - Read on a page that is RW (Only for page ref bit)
//  WriteRW - Write on a page that is RW (Only for page write bit)
typedef enum {
    ReadNPP  = 0,
    WriteNPP = 1,
    WriteRO  = 2,
    ReadRW   = 3,
    WriteRW  = 4
} pfFaultType;

// Page info
//  start - Start address of the page
//  prev - Previous page in linked list
//  next - Next page in linked list
//  pageFrame - The frame number in physical memory, -1 if not
//  readOnly - Flag set for when mprotect is in ReadOnly
//  ref - The ref bit for the third chance replacement algorithm
//  write - The 2-bit write field for the third chance replacement algorithm
typedef struct Page {
    char *start;
    struct Page *prev;
    struct Page *next;
    int pageFrame;
    bool readOnly;
    bool ref;
    ushort write : 2;
} Page;

/**
* Global Variables
*/

char *start;
int size = -1;
int mmPolicy = -1;
int pageSize = -1;
int pFrames = -1;

int activePages = 0;
int clockIndex = 0;

Page *pageListHead = NULL;
Page *pageListTail = NULL;

/**
* Helper Functions
*/

/**
* Find Page
* * Finds the page pointer from available pages given page start address
* @param pageStart the start address of the page
*/
Page *findPage(void *pageStart) {
    Page *tempPage = malloc(sizeof(struct Page));
    tempPage->start = pageStart;
    tempPage->prev = NULL;
    tempPage->next = NULL;
    tempPage->pageFrame = -1;
    tempPage->readOnly = 0;
    tempPage->ref = 0;
    tempPage->write = 0;

    // Search through linked list for the page
    Page *nextPage = pageListHead;
    while (nextPage != NULL) {
        if (nextPage->start == pageStart) {
            free(tempPage);
            return nextPage;
        }
        nextPage = nextPage->next;
    }

    return tempPage;
}

/**
* Enque Page
* * Enques the provided page to the end of page queue
* @param page the page to be queued
*/
void enquePage(Page *page) {
    if (pageListHead == NULL) {
        pageListHead = page;
        pageListTail = page;
    } else {
        page->prev = pageListTail;
        pageListTail->next = page;
        pageListTail = page;
    }
}

/**
* Evict Page
* * Evicts the provided page from the page queue
* @param page the page to be evicted
*/
void evictPage(Page *page) {
    if (page->prev == NULL) {
        pageListHead = page->next;
    } else {
        page->prev->next = page->next;
    }

    if (page->next == NULL) {
        pageListTail = page->prev;
    } else {
        page->next->prev = page->prev;
    }

    // Set page as independent item
    page->prev = NULL;
    page->next = NULL;
}

/**
* Reset Page
* * Sets page to default values of a new temp page
* @param page the page to be reset
*/
void resetPage(Page *page) {
    // Reset the page
    page->prev = NULL;
    page->next = NULL;
    page->pageFrame = -1;
    page->readOnly = 0;
    page->ref = 0;
    page->write = 0;

    // Protect the page
    mprotect(page->start, pageSize, PROT_NONE);
}

/**
* Main Functions
*/

/**
* Page Fault Handler
* * Handles a page fault which generates a SIGSEGV signal
* @param sig 
* @param sigInfo
* @param context
*/
static void pfHandler(int sig, siginfo_t *sigInfo, void *context) {
    char *addr = (char *)sigInfo->si_addr;
    bool write = ((ucontext_t *)context)->uc_mcontext.gregs[REG_ERR] & (PF_WRITE);

    // Check if address is in range of allocated memory
    if ((char *)addr > ((char *)start + size - 1) || (char *)addr < (char *)start) exit(SIGSEGV);

    int virtualPage = (addr - start) / pageSize;
    int pageOffet = (addr - start) % pageSize;
    char *startAddr = start + (virtualPage * pageSize);

    Page *pfPage = findPage(startAddr);

    // Determine if the page is a new page
    bool newPage = false;
    if (pfPage->prev == NULL && pfPage->pageFrame == -1 && pfPage->next == NULL) newPage = true;

    // Detemine cause of page fault
    bool writeBack = false;
    int evictedPage = -1;
    pfFaultType cause;
    if (pfPage->pageFrame == -1) {
        // Page not present
        if (!write) {
            cause = ReadNPP;
            pfPage->readOnly = 1;
            pfPage->ref = 1;
            mprotect(pfPage->start, pageSize, PROT_READ);
        } else {
            cause = WriteNPP;
            pfPage->ref = 1;
            pfPage->write = 3;
            mprotect(pfPage->start, pageSize, PROT_READ | PROT_WRITE);
        }
    } else {
        // Page present, no page placement necessary
        if (pfPage->readOnly && write) {
            cause = WriteRO;
            pfPage->readOnly = false;
            pfPage->ref = 1;
            pfPage->write = 3;
            mprotect(pfPage->start, pageSize, PROT_READ | PROT_WRITE);
        } else {
            // Should only be triggered in mmPolicy == 2
            if (!write) {
                cause = ReadRW;
                pfPage->ref = 1;
                mprotect(pfPage->start, pageSize, PROT_READ);
            } else {
                cause = WriteRW;
                pfPage->ref = 1;
                pfPage->write = 3;
                mprotect(pfPage->start, pageSize, PROT_READ | PROT_WRITE);
            }
        }
        mm_logger(virtualPage, cause, evictedPage, writeBack, (pfPage->pageFrame * pageSize) + pageOffet);
        return;
    }

    if (activePages < pFrames) {
        // Not all physical frames are filled
        pfPage->pageFrame = activePages++;
        enquePage(pfPage);
    } else {
        int pageFrame = -1;
        if (mmPolicy == 1) {
            // Do FIFO replacement
            // Find new page frame and evict oldest page
            Page *nextPage = pageListHead;
            while (nextPage != NULL) {
                if (nextPage->pageFrame > -1) {
                    // Evict the candidate page
                    evictPage(nextPage);

                    // Determine logger values
                    pageFrame = nextPage->pageFrame;
                    evictedPage = ((nextPage->start) - start) / pageSize;
                    if (nextPage->write > 0) writeBack = true;
                    resetPage(nextPage);

                    // Re-enque the page
                    enquePage(nextPage);
                    break;
                }
                // Shouldn't ever run since head will always be next item
                nextPage = nextPage->next;
            }
            
            // Set page frame and re-enque page
            pfPage->pageFrame = pageFrame;
            if (!newPage) evictPage(pfPage);
            enquePage(pfPage);
        } else if (mmPolicy == 2) {
            // Do third chance replacement
            bool pageEviction = false;

            // Loop until a page is evicted
            while (pageEviction == false) {
                // Determine first page in cycle
                Page *nextPage = pageListHead;
                for (int i = 0; i < clockIndex; i++) {
                    nextPage = nextPage->next;
                }

                // Loop until the end of the list
                while (nextPage != NULL) {
                    clockIndex++;
                    // Update current page
                    if (nextPage->pageFrame > -1) {
                        // Make sure the page is protected so we can
                        // Update clock bits for third chance cycle
                        mprotect(nextPage->start, pageSize, PROT_NONE);

                        // Check first chance
                        if (nextPage->ref == 1) {
                            // First chance, check reference bit
                            nextPage->ref = 0;
                        } else if ((nextPage->write & 1 << 1) > 0) {
                            // Second chance, check write first bit
                            nextPage->write = 1;
                        } else {
                            // Evict current page
                            pageEviction = true;
                            evictedPage = ((nextPage->start) - start) / pageSize;
                            pageFrame = nextPage->pageFrame;
                            nextPage->pageFrame = -1;

                            // Check second write bit to determine writeback
                            if ((nextPage->write & 1 << 0) > 0) {
                                nextPage->write = 0;
                                writeBack = true;
                            } else {
                                writeBack = false;
                            }

                            break;
                        }
                    }

                    nextPage = nextPage->next;
                }

                //  Check if loop needs to continue cycle
                if (!pageEviction || nextPage->next == NULL) clockIndex = 0;
            }

            // Setup pfPage
            pfPage->pageFrame = pageFrame;
            if (newPage) enquePage(pfPage);
        } else {
            // Using a policy that doesn't exist
            exit(-1);
        }
    }

    mm_logger(virtualPage, cause, evictedPage, writeBack, (pfPage->pageFrame * pageSize) + pageOffet);
}

/**
* Memory Management Init
* * Initializes the memory management system
* @param vm Pointer to the start of a memory region
* @param vm_size Size of the virtual memory region
* @param n_frames Number of physical pages in the system
* @param page_size Size of both physical and virtual pages
* @param policy MM Policy, 1 = FIFO, 2 = Clock replacement
*/
void mm_init(void* vm, int vm_size, int n_frames, int page_size, int policy) {
    // Assign global variables
    mmPolicy = policy;
    start = (char *)vm;
    size = vm_size;
    pageSize = page_size;
    pFrames = n_frames;

    // Set page fault handler
    struct sigaction sigAction;
    sigAction.sa_sigaction=pfHandler;
    sigemptyset(&(sigAction.sa_mask));
    sigAction.sa_flags = SA_SIGINFO;
    if (sigaction(SIGSEGV, &sigAction, NULL) != 0) exit(-1);

    // Protect the memory region
    if (mprotect(vm, vm_size, PROT_NONE) != 0) exit(-1);
}