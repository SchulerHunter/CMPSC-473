Virtual Memory Page Manager
=
## Overview
- To run the VM memory manager, run `make` followed by `./out <vmReplacementAlgorithm> <InputFile>`
    - `<vmReplacementAlgorithm>` is an integer of 0 or 1 denoting the replacement algorithm for each page of the virtual memory.
        - 1 - First In First Out (FIFO): A page management algorithm where the first page allocated is the first page removed when a new page needs to be allocated but there is no space. This does no account for any recent access to the page and will simply evict the oldest.

        - 2 - Third Chance Replacement: A modification to the second chance replacement algorithm. This algorithm cycles through all pages, and if a page is in physical memory, will offer two chances before replacement. The first is by reading the `referenced` bit and the second by one of the `modified` bits. If both of those bits are 0, the page will be evicted, and written to disk if need be.

    - `<InputFile>` is a multi-line file with each line representing a memory operation. Each line is of the form `<read|write> <virtualPageNumber> <offset> <result>`
        - `<read|write>` - The type of memory operation being performed
        - `<virtualPageNumber>` - The virtual page number the operation is being performed on
        - `<offset>` - The starting offset of the page being writen to
        - `<result>` - The value being written to the page, 0 if a read

- The first call to the manager is a init which stores the policy type, start address of the VM, size of the virtual memory, the size of each page, and the number of frames in the physical memory.
    - This call also establishes `pfHandler` as the handler function for when a `SIGSEGV` signal is raised from a segmentation fault.

- Whenever the test program in `project3.c` raises a segmentation fault, the function `pfHandler` is run to handle the fault.

- The function `pfHandler` handles all segmentation faults and accepts the parameters `sig`, the identifier for the signal, `sigInfo`, information regarding the signal, such as address, and `context` which can be used to identify the type of operation.
    - The function first checks that the memory being accessed is valid, then continues to find the `Page` object or create a new one, and proceeds to determine the cause of the `SIGSEGV` signal through metadata on the page and the cause determined from `context`.
    - If there is available physical frames, since no eviction is necessary, the page is simply queued and the function retuns. Otherwise it will evict a page based on the alogorithm supplied when the function was run.
        - FIFO:
            1. Iterate through the linked list of pages, assign the physical frame from the first page found with a non-negative frame (signalling the page is assigned)
            2. Reset and reque the old page
            3. Finally, if the page is not a new page, reque the page, otherwise it is new and can simply be enqued.
        - Third Chance Replacement
            1. Determine the page which the cycle stopped on last
            2. Iterate through the list of valid pages, activating protection on each valid frame to be able to reset the reference bit
            3. Remove bits as each chance, the first bit is reference bit, the second is the modified bit.
                - If there are no more bits, the page is evicted, and if it was modified, also flagged to write back to disk.
            4. The new page is then set to have the physical pageFrame of the evicted frame and queued if it is a new page.

## Data Structures
- `pfErrorCode` is a enum of the error code bits for a page fault. The only one we utilize is `PF_WRITE`, which is `1 << 1`.
- `pfFaultType` is another enum, used to store the causes of each page fault, which is written back using `mm_logger`.
    - `ReadNPP` - A read access was performed on a page which is not present in a physical frame. The page is set to read-only and takes the page frame of an evicted page.
    - `WriteNPP` - A write access was peformed on a page which is not present in a physical frame. The page is set to allow read and write access and takes the page frame of an evicted frame.
    - `WriteRO` - A write access was performed on a present frame which was set to read-only. The page is set to allow read and write access.
    - `ReadRW` - On normal hardware, would not cause a page fault, however causes one on the third chance replacement in order to accurately track the reference bit.
    - `WriteRW` - The same as `ReadRW`, used to track reference bit and write bits.
- `Page` - A struct used to monitor the metadata of a virtual memory page.
    - `start` - The start address of the page.
    - `prev` - A pointer to the previous page in the page list.
    - `next` - A pointer to the next page in the page list.
    - `pageFrame` - The physical frame of the page, -1 if the page does not have a physical frame.
    - `readOnly` - A bit flag to denote if the frame is considered readOnly
    - `ref` - The reference bit, used for the third chance replacement. This bit is set to one every time the page is read or written.
    - `write` - A two bit flag. The left bit is used to track the third chance, while the write bit is simply to denote if the page was written. When the page is written and evicted, it is flagged to signal a write back.

## Global Variables
- `start` - The start of the virtual memory space.
- `size` - The size of the virtual memory space.
- `mmPolicy` - The policy used to manage the pages for the virtual memory.
- `pageSize` - The size of each page in the system.
- `pFrames` - The number of physical frames on the system.
- `activePages` - The number of currently active VM pages in the physical space.
- `clockIndex` - Index of the current point in the triple chance replacement algorithm.
- `pageListHead` - A pointer to the head of the page linked list.
- `pageListTail` - A pointer to the tail of the page linked list.

## Helper Functions
- `findPage` is a function that accepts the start address of a page and generates a temporary page. If the page is found in the page list, this temporary page is released and the found page is returned.
- `enquePage` is a simple function which just adds the passed page pointer to the page list.
- `evictage` is a function which accepts a pointer to a page and removes the page from the linked list.
- `resetPage` is a function which resets the passed pages metadata.

## Challenges Faced
- Recusivity: I had a decent struggle attempting to get the `buddyAllocate` and `buddyDeallocate` to run recursively.
- Memory Calculations: Calculating the addresses for slab allocated object and hole parity seemed more difficult then it truly was.