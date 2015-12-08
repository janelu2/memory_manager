#####################################################################
# Malloc Lab - Memory Manager 
######################################################################

***********
Main Files:
***********
* mm.{c,h}	
	- The malloc package
* mdriver.c	
	- The malloc driver that tests mm.c file
* Makefile	
	- Builds the driver

*********
Approach:
*********
 * MALLOC: A dynamic memory allocator that maintains an area of a process's virtual 
 * memory known as the heap. In this approach, blocks are allocated by traversing 
 * an explicit, doubly linked list consisted of only free blocks to optimize search time. 
 * The first free block that will accomodate the requested size is chosen to be allocated.
 * FREE: When the program frees a block, the explicit free list will add that 
 * block to the head of its list using doubly linked list node insertion methods.
 * BLOCKS: The heap is 8-byte aligned. Each block begins with a header and ends
 * with a footer that holds information on the size of the block and if it is free.
 * To implement an explicit free list, each block also holds the address of the 
 * next and previous free block if it is free. 
 *      
 *      free block: [header|previous_free_block|next_free_block|some_data|footer]
 * allocated block: [header|-------------------some_data-----------------|footer]
 *                 
 * O(K) time, where k is the number of free blocks in the free list.

***********
Evaluation:
***********
* Performance Index = 0.6(Space Utilization) + 0.4 * min(1, throughput/throughput of libc malloc)
* Performance of this approach: 90/100

**********************************
Other support files for the driver
**********************************
* config.h	
	- Configures the malloc lab driver
* fsecs.{c,h}	
 	- Wrapper function for the different timer packages
* clock.{c,h}	
 	- Routines for accessing the Pentium and Alpha cycle counters
* fcyc.{c,h}	
 	- Timer functions based on cycle counters
* ftimer.{c,h}	
 	- Timer functions based on interval timers and gettimeofday()
* memlib.{c,h}	
 	- Models the heap and sbrk function

*******************************
Building and running the driver
*******************************
* To build the driver, type "make" to the shell.
* To run the driver on a tiny test trace:
	- unix> mdriver -V -f short1-bal.rep
* The -V option prints out helpful tracing and summary information.
* To get a list of the driver flags:
	- unix> mdriver -h

