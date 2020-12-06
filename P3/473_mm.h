// Authors: TAs of CMPSC473, Penn State
// Contributors: Adithya Kumar (azk68), Jashwant Raj Gunasekaran (jqg5490)
// Description: Header file for the memory management unit

#ifndef _473_MM_H
#define _473_MM_H

static int statCounter = 0;
struct MM_stats
{
  int virt_page;
  int fault_type;
  int evicted_page;
  int write_back;
  unsigned int phy_addr;
};
struct MM_stats *stats;

/* To be called from within the signal handler
 * virt_page:   Page number of the virtual page being referenced     
 * fault_type:  Indicates what caused SIGSEGV.
 *          0 - Read access to a non-present page
 *          1 - Write access to a non-present page
 *          2 - Write access to a currently Read-only page
 *          3 - Track a "read" reference to the page that has Read and/or Write permissions on.
 *          4 - Track a "write" reference to the page that has Read-Write permissions on.
 * evicted_page:Virtual page number that is evicted. (-1 in case of no eviction)
 * write_back:  = 1 indicates evicted page needs writing back to disk, = 0 otherwise
 * phy_addr:    Represents the physical address (frame_number concatenated with the offset)
 */
extern void mm_logger(int virt_page, int fault_type, int evicted_page, int write_back, unsigned int phy_addr);

/* TO BE IMPLEMENTED
 * 'mm_init()' initializes the memory management system. 
 * 'vm' denotes the pointer to the start of virtual address space, 
 * 'vm_size' denotes the size of the virtual address space, 
 * 'n_frames' denotes the number of physical pages available in the system, 
 * 'page_size' denotes the size of both virtual and physical pages, 
 * 'policy' can take values 1 or 2 -- 1 indicates fifo replacement policy and 2 indicates clock replacement policy. 
 */
extern void mm_init(void *vm, int vm_size, int n_frames, int page_size, int policy); 
extern void print_stats();

/*
 * UNUSED
// 'mm_report_npage_faults' should return the total number of page faults of the entire system (across all virtual pages). 
unsigned long mm_report_npage_faults(); 
// 'mm_report_nwrite_backs' should return the total number of write backs of the entire system (across all virtual pages). 
unsigned long mm_report_nwrite_backs();
*/

#endif
