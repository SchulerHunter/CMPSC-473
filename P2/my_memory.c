// Include files
#include <stdio.h>
#include <stdlib.h>
#define  N_OBJS_PER_SLAB  64
#define  minSize          1024
#define  BUDDY            0
#define  SLAB             1
#define  ULONG_MAX        0xFFFFFFFFFFFFFFFF

// Functional prototypes
void setup(int malloc_type, int mem_size, void* start_of_memory);
void *my_malloc(int size);
void my_free(void *ptr);

/**
* Data Structures
*/

// Boolean
typedef enum {false, true} bool;

// Header
//  Chunk size
//  Chunk allocated boolean
//  Previous chunk allocated boolean
typedef struct Header {
    unsigned int size : 32;
} Header;

// AvailableHole
//  Start address of hole
//  End address of hole
//  Pointer to next hole
//  Size of available hole
//    Serves as bitmap in slab
typedef struct AvailableHole AvailableHole;

struct AvailableHole {
    void *start;
    void *end;
    AvailableHole *next;
    unsigned long size : 64;
};

// Slab Descriptor for table
//  Size of items being allocated
//  Allocated size for slab
//  Total items that can be stored in slab
//  Slab pointer info
typedef struct {
    unsigned int itemSize;
    unsigned int allocSize;
    unsigned int total;
    unsigned int used;
    AvailableHole *slabPtr;
} SlabDescriptor;

// Allocator variables
int allocType = -1;
int maxMem = -1;
void *allMem;

int maxAvailableHoles = 0;
int slabItems = 0;
AvailableHole **holeList;
SlabDescriptor **descriptorTable;

/**
* Helper Functions
*/

/**
* Fetch Header
* * Fetches the header of the provided pointer
* * Implicitly converts pointer to char* for bytesize memops
* @param ptr a pointer whos header is to be fetched
* @return a pointer to the header of the pointed to chunk
*/
Header *fetchHeader(char *ptr) {
    return (Header *)(ptr);
}

/**
* Convert Size
* * Converts the size provided to log base 2
* * index 0 = 1KB, index 1 = 2KB, index 2 = 4KB, index 10 = 1MB
* @param size
* @return size in base 2, rounded down for index
*/
int convertSize(unsigned int size) {
    int index = 0;
    size = (size-1)/minSize;
    for (size; size > 0; size /= 2) {
        index++;
    }
    return index;
}

/**
* Convert Index
* * Converts the index provided chunk size
* * index 0 = 1KB, index 1 = 2KB, index 2 = 4KB, index 10 = 1MB
* @param index index whos size is to be calculated
* @return size of given index
*/
int convertIndex(unsigned int index) {
    int size = minSize;
    for (int i = 0; i < index; i++) {
        size *= 2;
    }
    return size;
}

/**
* Find Descriptor Index
* * Finds the index of the size in the descriptorTable
* @param size size whos index is to be found
* @return index in desciptor table
*/
int findDescriptorIndex(int size) {
    int index = -1;
    for (int i = 0; i < slabItems; i++) {
        if (size == descriptorTable[i]->itemSize) {
            index = i;
            break;
        }
    }
    return index;
}

/**
* Buddy Allocate
* * Allocates a chunk of memory of given size
* @param size size of chunk to be allocated
* @return pointer to chunk, not including header
*/
void *buddyAllocate(int size) {
    void *retVal = (void *)-1;

    // Include +4 for the header
    int log2size = convertSize(size+4);
    int i = log2size;

    // Iterate through hole list and find lowest 
    // available hole to accomodate needed size
    while (holeList[i] == NULL && i < (maxAvailableHoles - 1)) i++;
    if (holeList[i] == NULL && i == (maxAvailableHoles - 1)) return retVal;

    // Begin recursively breaking hole in two
    while (holeList[log2size] == NULL) {
        // Copy hole
        AvailableHole *hole = holeList[i];

        // Update the new hole list
        // Decrease index
        holeList[i--] = hole->next;

        // Prep newHole, the hole that is left behind
        int newSize = (hole->size)/2;
        AvailableHole *newHole = malloc(sizeof(AvailableHole));
        newHole->start = (char *)(hole->start)+newSize;
        newHole->end = hole->end;
        newHole->size = newSize;
        newHole->next = NULL;

        // Update the original hole
        hole->size = newSize;
        hole->end = (char *)(hole->start)+newSize-1;

        // Set the hole to point to new hole
        hole->next = newHole;

        // Check if newHole becomes head of next size
        if (holeList[i] != NULL && hole->start < holeList[i]->start) {
            // Set the new hole to point to this hole
            newHole->next = holeList[i];
            holeList[i] = hole;
        } else {
            if (holeList[i] == NULL) {
                holeList[i] = hole;
            // This else shouldn't ever run, but if it does, it will correct any out of order memory
            } else {
                // Iterate through next hole list to find where the new holes go
                AvailableHole *thisHole = holeList[i];
                AvailableHole *nextHole = thisHole->next;
                while (nextHole != NULL) {
                    if (nextHole->start != NULL && hole->start < nextHole->start) {
                        thisHole->next = hole;
                        newHole->next = nextHole;
                        break;
                    }
                    thisHole = nextHole;
                    nextHole = thisHole->next;
                }
                if (nextHole == NULL) thisHole->next = hole;
            }
        }
    }
    // Temporarily store the new hole
    AvailableHole *newHole = holeList[log2size];
    // Store the start address of the new hole
    void *start = newHole->start;
    // Set the hole list to point at the next hole
    holeList[log2size] = newHole->next;
    // Free the temporary memory
    free(newHole);
    // Return the start address for the memory chunk
    return start;
}

/**
* Buddy Deallocate
* * Deallocates a chunk of memory pointed at by ptr
* @param ptr memory chunk to be deallocated
*/
void buddyDeallocate(void *ptr) {
    Header *header = fetchHeader(ptr);
    // Calculate index and chunk size of allocated block
    int log2size = convertSize((header->size)+4);
    int chunkSize = convertIndex(log2size);
    
    // Calculate the new available hole being added
    AvailableHole *newHole = malloc(sizeof(AvailableHole));
    newHole->start = (char *)(ptr);
    newHole->end = (char *)(ptr)+chunkSize-1;
    newHole->size = chunkSize;
    newHole->next = NULL;

    // Check if we need to check for buddy merge
    AvailableHole *thisHole = holeList[log2size];
    while (thisHole != NULL) {
        // Check if this is a first or second hole in the holes list
        bool firstHole = 1 - ((((char *)(newHole->start) - (char *)allMem) / newHole->size) % 2);
        bool buddyFound = false;
        if (firstHole) {
            AvailableHole *nextHole = thisHole->next;

            // If the first hole in the list is this holes buddy
            if (thisHole->start == (char *)(newHole->end)+1) {
                newHole->end = thisHole->end;
                newHole->size = 2*newHole->size;
                holeList[log2size] = nextHole;
                free(thisHole);
                buddyFound = true;
            }

            // Iterate through the list to find the holes buddy
            while (!buddyFound && nextHole != NULL && (char *)(newHole->end) < (char *)(nextHole->start)) {
                if (nextHole->start == (char *)(newHole->end)+1) {
                    // Do some buddy merging
                    buddyFound = true;
                    newHole->end = nextHole->end;
                    newHole->size = 2*newHole->size;
                    thisHole->next = nextHole->next;
                    free(nextHole);
                    break;
                }
                thisHole = nextHole;
                nextHole = thisHole->next;
            }

            // Check finish criteria for list interation
            if (buddyFound) {
                thisHole = holeList[++log2size];
            } else {
                // If there is no nextHole and thisHole is not the buddy
                if (nextHole == NULL) {
                    if ((char *)thisHole->start < (char *)(newHole->start)) {
                        thisHole->next = newHole;
                        return;
                    } else {
                        newHole->next = thisHole;
                        holeList[log2size] = newHole;
                        return;
                    }
                }
                // The buddy is not in the list and we are now out of order
                if (newHole->end > nextHole->start) {
                    newHole->next = nextHole->next;
                    nextHole->next = newHole;
                    return;
                }
            }
        } else { // Is a second hole
            AvailableHole *nextHole = thisHole->next;

            // If the first hole in the list is this holes buddy
            if (thisHole->end == (char *)(newHole->start)-1) {
                newHole->start = thisHole->start;
                newHole->size = 2*newHole->size;
                holeList[log2size] = nextHole;
                free(thisHole);
                buddyFound = true;
            }

            // Iterate through the list to find the holes buddy
            while (!buddyFound && nextHole != NULL && nextHole->end < newHole->start) {
                if (nextHole->end == (char *)(newHole->start)-1) {
                    // Do some buddy merging
                    buddyFound = true;
                    newHole->start = nextHole->start;
                    newHole->size = 2*newHole->size;
                    thisHole->next = nextHole->next;
                    free(nextHole);
                    break;
                }
                thisHole = nextHole;
                nextHole = thisHole->next;
            }

            // Check finish criteria for list interation
            if (buddyFound) {
                thisHole = holeList[++log2size];
            } else {
                // If there is no nextHole and thisHole is not the buddy
                if (nextHole == NULL) {
                    if (thisHole->start < newHole->start) {
                        thisHole->next = newHole;
                        return;
                    } else {
                        newHole->next = thisHole;
                        holeList[log2size] = newHole;
                        return;
                    }
                }
                // The buddy is not in the list and we are now out of order
                if (nextHole->end > newHole->start) {
                    thisHole->next = newHole;
                    newHole->next = nextHole;
                    return;
                }
            }
        }
    }

    if (thisHole == NULL) {
        // Add to the list
        holeList[log2size] = newHole;
        return;
    }
}

/**
* Main Functions
*/

/**
* Setup
* * Initialize memory related variables for the memory allocator
* @param mallocType type of allocator to use for system
* @param memSize size of memory chunk provided
* @param startOfMemory pointer to chunk of available memory
*/
void setup(int mallocType, int memSize, void* startOfMemory) {
    allocType = mallocType;
    maxMem = memSize;
    allMem = startOfMemory;

    maxAvailableHoles = convertSize(memSize) + 1;
    holeList = malloc(sizeof(AvailableHole *) * maxAvailableHoles);

    for (int i = 0; i < maxAvailableHoles; i++) {
        holeList[i] = NULL;
    }

    AvailableHole *maxHole = malloc(sizeof(AvailableHole));;
    maxHole->start = (char *)(startOfMemory);
    maxHole->end = (char *)(startOfMemory+memSize);
    maxHole->next = NULL;
    maxHole->size = memSize;
    holeList[maxAvailableHoles-1] = maxHole;
}

/**
* My Malloc
* * Allocates size bytes from allMem chunk according to allocType
* @param size size of pointer to allocate
* @return pointer to allocated memory, pointer to -1 if no can't be allocated
*/
void *my_malloc(int size) {
    void *retVal = (void *)-1;

    if (allocType == BUDDY) {
        // Check that memory can be allocated
        if (size > maxMem) return retVal;
        // Get and check next memory address
        void *start = buddyAllocate(size);
        if (start == (void *)-1) return retVal;

        // Allocate header
        Header *header = start;
        header->size = size;
        retVal = (char *)(header)+4;
    } else if (allocType == SLAB) {
        // Check if slab entry exists
        int index = findDescriptorIndex(size);

        if (index > -1) {
            // Find first available slot in slab, if it exists
            int itemNumber = -1;
            AvailableHole *currentSlab = descriptorTable[index]->slabPtr;
            while (currentSlab != NULL && itemNumber == -1) {
                if (currentSlab->size != ULONG_MAX) {
                    for (int i = 0; i < N_OBJS_PER_SLAB; i++) {
                        long mask = (long)1 << (63 - i);
                        if ((currentSlab->size & mask) == 0) {
                            // Mark new spot as taken
                            currentSlab->size |= mask;
                            itemNumber = i;
                            break;
                        }
                    }
                }
                if (itemNumber == -1 && currentSlab->next == NULL) break;
                if (itemNumber == -1) currentSlab = currentSlab->next;
            }

            if (itemNumber > -1) {
                descriptorTable[index]->used++;

                // Start address for item is start address of slab + slab header + number of prior items * (size of items + header)
                void *start = (char *)(currentSlab->start)+4 + itemNumber*(size+4);
                Header *header = start;
                header->size = size;
                retVal = (char *)(header)+4;
            } else {
                // Allocate new slab of 64*(size+header)
                void *start = buddyAllocate(N_OBJS_PER_SLAB*(size+4));
                if (start == (void *)-1) return retVal;

                // Set the new slab header
                Header *header = start;
                header->size = (N_OBJS_PER_SLAB * (size+4));

                // Set up slab entry
                AvailableHole *newSlabPtr = malloc(sizeof(AvailableHole));
                newSlabPtr->start = (char *)(start);
                newSlabPtr->end = (char *)(start)+N_OBJS_PER_SLAB*(4+size)-1;
                newSlabPtr->size = ((long)1 << 63);
                newSlabPtr->next = NULL;

                // Update descriptor table
                descriptorTable[index]->total += N_OBJS_PER_SLAB;
                descriptorTable[index]->used += 1;
                currentSlab->next = newSlabPtr;

                // Allocate item in new slab
                start = (char *)(start)+4;
                header = start;
                header->size = size;

                // Set up return value
                retVal = (char *)(header)+4;
            }
        } else {
            // Check slab can be allocated
            if (N_OBJS_PER_SLAB*(size+4) > maxMem) return retVal;
            // Allocate new slab
            void *start = buddyAllocate(N_OBJS_PER_SLAB*(size+4));
            if (start == (void *)-1) return retVal;

            // Set up slab pointer entry
            AvailableHole *newSlabPtr = malloc(sizeof(AvailableHole));
            newSlabPtr->start = (char *)(start);
            newSlabPtr->end = (char *)(start)+N_OBJS_PER_SLAB*(size+4)-1;
            newSlabPtr->size = ((long)1 << 63);
            newSlabPtr->next = NULL;

            // Set up slab entry data
            descriptorTable = realloc(descriptorTable, (++slabItems)*sizeof(SlabDescriptor));
            SlabDescriptor *newSlabDescriptor = malloc(sizeof(SlabDescriptor));
            newSlabDescriptor->itemSize = size;
            newSlabDescriptor->allocSize = convertIndex(convertSize(N_OBJS_PER_SLAB*(size+4)));
            newSlabDescriptor->total = N_OBJS_PER_SLAB;
            newSlabDescriptor->used = 1;
            newSlabDescriptor->slabPtr = newSlabPtr;
            descriptorTable[slabItems-1] = newSlabDescriptor;

            // Set the new slab header
            Header *header = start;
            header->size = (N_OBJS_PER_SLAB*(size+4));

            // Allocate item in new slab
            start = (char *)(start)+4;
            header = start;
            header->size = size;

            // Set up return value
            retVal = (char *)(header)+4;
        }
    }

    return retVal;
}

/**
* My Free
* * Frees the provided pointer from allMem
* * Converts freed space according to alloc type
* @param ptr a pointer to free
* @return None
*/
void my_free(void *ptr) {
    ptr = (char *)(ptr)-4;

    if (allocType == BUDDY) {
        buddyDeallocate(ptr);
    } else if (allocType == SLAB) {
        // Find slab descriptor entry
        Header *header = fetchHeader(ptr);
        int tableIndex = findDescriptorIndex(header->size);
        if (tableIndex == -1) return;

        // Find slab pointer with associated object
        AvailableHole *currentSlab = descriptorTable[tableIndex]->slabPtr;
        AvailableHole *prevSlab = NULL;
        while (currentSlab != NULL) {
            if (currentSlab->start < ptr && currentSlab->end > ptr) break;
            if (currentSlab->next == NULL) break;
            prevSlab = currentSlab;
            currentSlab = currentSlab->next;
        }
        if (currentSlab == NULL) return;

        // Find slab item index in bitmap and mark empty
        int objIndex = ((char *)ptr - (char *)(currentSlab->start)+4) / (header->size + 4);
        currentSlab->size -= ((long)1 << (63 - objIndex));

        // Free the object
        header->size = -1;

        // Release whole slab if there are no more objects present
        if (currentSlab->size == 0) {
            buddyDeallocate(currentSlab->start);
            if (prevSlab != NULL) {
                prevSlab->next = currentSlab->next;
            } else {
                if (currentSlab->next == NULL) {
                    // Swap end of table to current position and realloc the table
                    SlabDescriptor *tempDescriptor = descriptorTable[--slabItems];
                    free(descriptorTable[tableIndex]);
                    descriptorTable[tableIndex] = tempDescriptor;
                    descriptorTable = realloc(descriptorTable, slabItems*sizeof(SlabDescriptor));
                } else {
                    descriptorTable[tableIndex]->slabPtr = currentSlab->next;
                }
            }
            free(currentSlab);
        }
    }
}
