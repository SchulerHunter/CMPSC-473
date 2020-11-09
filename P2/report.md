Memory Allocator
=
## Overview
- To run the allocator, run `make` followed by `./out <allocatorType> <InputFile>`
    - `<allocatorType>` is an integer of 0 or 1 denoting the scheduler style.
        - 0 - Buddy Allocation: A memory partition technique which divides the memory in half until a sufficient size is found. The minimum size for a memory block is 1KB. A list of available holes is maintained as an array of linked list object pointing in order of ascending memory address.

        - 1 - Slab Allocation: A memory technique which utilizes buddy allocation for a slab where each slab will contain a number of items of the same size. In this implementation, the number of items per slab is 64. Implements a slab descriptor table to track all slabs of each memory object size. Each entry in the table contains a start to a linked list with each object being a slab in the table, ordered by ascending address.

    - `<InputFile>` is a multi-line file with each line representing a memory operation. Each line is of the form `<name> <numOps/index> <type> <size>`
        - `<name>` - Name ID to a certain memory group
        - `<numOps/index>` - In the case of an allocation operation, is the number of allocations requested. In the case of a free operation, is the index of the memory to free.
        - `<type>` - Indeicates the type of operation to be performed
            - `M` - allocation
            - `F` - free
        - `<size>` - Used only for an `M` operation, denotes the size of each memory object requested in the operation.

- The first call to the allocator is a setup, which stores the max memory size, allocation type, and starting address of the memory provided.
    - This call also allocates a list for the available holes in the memory chunk denoted in a buddy allocation style.

- During an `M` allocation call to the allocator, the allocation method used for `my_malloc` depends on the allocation selected during the `setup` of the program.
- The function `my_malloc` handles all allocation calls and accepts the parameter `size` which is the size of the memory object to allocate.
    - With the buddy allocation style, a call to `buddyAllocator` (explained later) is used to find the starting address to a new memory chunk with the correct size. A header is stored in the first four bytes to keep track of the size, and the address after the first four bytes is returned.
    - Alternatively, with the slab allocation method, the process is a little more complex. The slab allocator follows the steps:
        1. Check if there is a slab descriptor table entry for objects of the size requested. If not, create a new entry and a new slab and allocate the header for the slab and item and return the address after the header for the object.
        2. If there is an index in the table, return the first slab with an open spot to put the memory. This is found by scanning a bitmask for a 0, from right to left. If there is, assign the object to that slot and return the address after its header.
        3. If there is no available slabs, create a new slab with the `buddyAllocator` method using the size of `(64*(size+4))` to account for the header of each object within the slab.
        4. Finally, update all the appropriate data, including the table if necessary, and the slab pointers to continue the linked list.
- During an `F` free call to the allocator, the free method used in `my_free` depends on the method selected during `setup`
- The function `my_free` handles all memory free calls and accepts the parameter `ptr` which is the address returned from `my_malloc` as the available memory for the object. The header for the `ptr` is located at `ptr-4`
    - With the buddy allocation style, the `buddyDeallocate` function (explained later) is called with the parameter `ptr-4`.
    - With slab allocation style, the method is more complex, and follows the steps:
        1. Find the entry in the slab descriptor table using the size stored in the header of `ptr`.
        2. Find the slab pointer associated with the object. Since each slab pointer contains both the slab start and end address, the address of `ptr` must fall within those boundaries.
        3. The index of the object within the slab must then be calculated. To do this, the idea of each slab being the same size is used in the formula `(ptr - (slabStart+slabHeader))/(objectSize+objectHeader)` is used to determine which item the ptr will be, and then the value of `1 << slabObjects - objectIndex` is subtracted from the bitmap, marking the object as removed.
        4. Once the object is removed, if the slab is completely empty, the table is checked to determine of the table entry is empty, and if it is remove the entry. The slab is also deallocated using `buddyDeallocate`.
## Data Structures
- `Header` is a simple data structure consisting of a single unsigned integer as the size constrained at 32 bytes to be used on any system.
- `AvailableHole` is a data structure for mapping holes within the available hole list as well as slabs in the slab descriptor table.
    - `start` is the starting address for any memory allocated, either slab or hole.
    - `end` is the end address of the memory allocation for both a slab and hole.
    - `next` points to the next `AvailableHole` data type to form a linked list. The `next` item always has a `start` which is greater than the current `end`.
    - `size` is different for slabs and holes. In a hole for th available hole list, size denotes the size of the available hole, as a power of two. For slabs, size is used as the 64 bit bitmap for the items allocated to each slab.
- `SlabDescriptor` is a descriptor item for each entry in the slab descriptor table.
    - `itemSize` is the size of the items allocated to the slabs linked to this table entry.
    - `allocSize` is the amount of memory allocated to each slab, using the `buddyAllocate` functionality.
    - `total` is the total number of items able to be stored within this slab table entry. This value increase by `N_OBJS_PER_SLAB` each time a slab is added.
    - `used` is the number of items currently occupying the available slabs
    - `slabPtr` is a pointer to the lowest memory address slab. Each slabPtr points to another if there are multiple slabs allocated for this entry.

## Helper Functions
- `fetchHeader` is a function which accepts a pointer `ptr` which is generally passed as an object pointer - 4 and returns the pointer case as a `header` object.
- `convertSize` is a function to implement `log2` rounded down. A `size` parameter is passed to the function and is converted to an appropriate index for the availableHoles table.
- `convertIndex` is a function which accepts the parameter `index` and converts it to the value of `2**index`, or `pow(2, index)`.
- `findDescriptorIndex` accepts a parameter `size` and finds a corresponding entry in the slab descriptor table. If an entry does not exist, it returns -1.
- `buddyAllocate` implements the buddy allocator functionality, accepting a parameter `size` which is the sie of the memory to allocate. It operates by following steps:
    1. Calculate an index using `convertSize` to include both the object size, and an appropriate header.
    2. Search through the available hole list and find the lowest available hole to accomedate the size calculated in step 1.
    3. If a hole to accomodate the size from step 1 is unavailable, the function will recursively break memory chunks to create a hole to accomodate.
        1. Copies the first hole from the lowest available index from step 2 and sets that index to look at the hole after this one.
        2. Calculates a newSize equal to half the old size and preps a newHole to be the one left behind, where the start of newHole will be the `(start of the hole from step 1 + newSize)`, and the end will be the original end.
        3. Update the hole from step 1 to be the correct size and update to end to be start+newSize-1 and sets the next hole for this hole to be newHole.
        4. Finds the appropriate location in the new list to append both holes, though ideally should be the head. If it is not the head of the list, an error has occured, though it will sort the list by memory address.
    4. The function then returns the starting address of a hole to accomodate the size required and frees the dynamic memory to store the data for the hole, while updating the list.
- `buddyDeallocate` implements a deallocation method for the buddy functionality. This function accepts a `ptr-4` as a parameter, where `ptr` is the address of a memory object and the `-4` includes the header. The function follows steps:
    1. Calculate the header, index, and size of the chunk of memory allocated using the parameter.
    2. A new hole is then calculated using this data, with `start` as the parameter, `end` as the `paramter+chunkSize-1` and `size` as chunkSize. There is no `next` yet.
    3. Next we recursively check if buddy merging needs to be completed. We find if the current hole will be a first or second hole to determine if we need to compare start or end addresses.
        - If the hole is a first hole, we compare the end address of the new hole to the start address of the existing holes. If they match, we combine the holes and repeat for the merged hole. If there is no match, we insert the new hole in accordance to memory address.
        - If the hole is a second hole, we compare the start address of the new hole to the end addresses of the existing holes to see if they match. If they match, we combine the holes and repeat for the merged hole, other wise we insert in accordance with the memory address ordering.
    4. Finally, if we didn't insert the hole earlier, then there will be no hole existing at the current level, so we set the list to point to this hole.

## Challenges Faced
- Recusivity: I had a decent struggle attempting to get the `buddyAllocate` and `buddyDeallocate` to run recursively.
- Memory Calculations: Calculating the addresses for slab allocated object and hole parity seemed more difficult then it truly was.