/**
 * @file mm.c
 * @author Jian Ying (Jane) Lu
 * @date 12/8/2015
 * Assignment: Dynamic Storage Allocator Lab - Malloc  
 * @brief: implements an explicit free list (doubly linked) and first-fit search algorithm 
 * @details: 
 *      * MALLOC: A dynamic memory allocator that maintains an area of a process's virtual 
 *      * memory known as the heap. In this approach, blocks are allocated by traversing 
 *      * an explicit, doubly linked list consisted of only free blocks to optimize search time. 
 *      * The first free block that will accomodate the requested size is chosen to be allocated.
 *      *
 *      * FREE: When the program frees a block, the explicit free list will add that 
 *      * block to the head of its list using doubly linked list node insertion methods.
 *      *
 *      * BLOCKS: The heap is 8-byte aligned. Each block begins with a header and ends
 *      * with a footer that holds information on the size of the block and if it is free.
 *      * To implement an explicit free list, each block also holds the address of the 
 *      * next and previous free block if it is free. 
 *      * 
 *      *      free block: [header|previous_free_block|next_free_block|some_data|footer]
 *      * allocated block: [header|-------------------some_data-----------------|footer]
 *      *           
 *      * O(K) time, where k is the number of free blocks in the free list.
 * @bugs none
 * @todo none
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/* Basic constants and macros */
#define WSIZE 4	/* word size (bytes) */
#define DSIZE 8	/* doubleword size (bytes) */
#define CHUNKSIZE 1<<12	/* initial heap size (bytes) */

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p) (*(int *)(p))
#define PUT(p, val) (*(int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((void *)(bp) - WSIZE)
#define FTRP(bp) ((void *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((void *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((void *)(bp) - GET_SIZE(HDRP(bp) - WSIZE))

/* Given block ptr bp, compute address of next and previous free blocks */
#define NEXT_FREE_BLKP(bp)(*(void **)(bp + DSIZE))
#define PREV_FREE_BLKP(bp)(*(void **)(bp))

/* Global variables*/
static char *heap_listp = 0; 
static char *free_listp = 0;

team_t team = {
    /* Team name */
    "what team",
    /* First member's full name */
    "Jane Lu",
    /* First member's email address */
    "jlu00@nmt.edu",
    /* Second member's full name */
    "",
    /* Second member's email address */
    ""
};

/* 
 * mm_init - Initialize the malloc package.
 * This function gets four words from the memory system and initializes them
 * to create the empty free lists. It then calls the extend_heap function to 
 * extend the heap by CHUNKSIZE and creates inital free block.
 */
int mm_init(void)
{
    /* initialize an empty heap */
    if ((heap_listp = mem_sbrk(2 * 24)) == NULL)
        return -1;
    PUT(heap_listp, 0);                          /* alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(24, 1));    /* prologue header */
    PUT(heap_listp + (2*WSIZE), 0);              /* prologue footer */
    PUT(heap_listp + (3*WSIZE), 0);              /* epilogue header */

    PUT(heap_listp + 24, PACK(24,1));
    PUT(heap_listp + WSIZE + 24, PACK(0,1));

    free_listp = heap_listp + DSIZE;    /* initialize free explicit list */

    /* Extend the empty heap witha  free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
      
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 * An application would request a block of size bytes by calling mm_malloc.
 * The allocator must adjust the requested block size and call find_fit to
 * find an address to put the newly allocated block.
 */
void *mm_malloc(size_t size)
{
    size_t asize;       /* adjusted block size */
    size_t extendsize;  /* amount to extend heap if no fit */
    char *bp;

    if (size == 0)      /* ignore silly request */
        return NULL;

    asize = MAX(ALIGN(size) + DSIZE, 24);

    /* search list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* no fit found, get more memory and place on the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Frees a block of memory
 * Also adds the newly freed block to the list of free blocks.
 * Coalesces if possible.
 */
void mm_free(void *bp)
{
    if(!bp) return; 
  	size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/*
 * mm_realloc - Reallocates a block.
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

/*
 * Coalesce - when a block is freed, this function merges its adjacent free
 * blocks to prevent false fragmentation. There exists 3 cases:
 * Case 1: The previous and next blocks are both allocated 
 * Case 2: The previous is allocated and next block is free
 * Case 3: The previous block is free and the next block is allocated.
 * Case 4: The previous and next blocks are both free
 */
static void *coalesce(void *bp)
{
    /* get tags of next and previous blocks */
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))) || PREV_BLKP(bp) == bp;
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));

	size_t size = GET_SIZE(HDRP(bp));
	
    /* case 2 */
	if (prev_alloc && !next_alloc)
	{
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));  /* add size of next free block */
		rmv_from_free(NEXT_BLKP(bp));           /* remove the block from free list */
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
	}

    /* case 3 */
	else if (!prev_alloc && next_alloc)
	{
	  size += GET_SIZE(HDRP(PREV_BLKP(bp)));    /* add size of previous free block */
	  bp = PREV_BLKP(bp);
	  rmv_from_free(bp);                         /* remove the block from free list */
	  PUT(HDRP(bp), PACK(size, 0));
	  PUT(FTRP(bp), PACK(size, 0));
	}

    /* case 4 */
	else if (!prev_alloc && !next_alloc)
	{	
        /* add size of next and previous free block */
		size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
		rmv_from_free(PREV_BLKP(bp));   /* remove the block from free list */
		rmv_from_free(NEXT_BLKP(bp));   /* remove the block from free list */
		bp = PREV_BLKP(bp);
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
	}

    /* if case 1 occurs, it will drop down here without merging with any blocks */
	insert_front(bp);
	return bp;
}

/* 
 * extend_heap - extends the heap by the given number of words
 * This function is called when the free list does not have a block that is large
 * enough to accomodate for the requested payload. This function extends the heap
 * and creates a new free block.
 */
void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* check alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if (size < 24)
        size = 24;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    /* tag new blocks as unallocated */
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));

    /* coalesces if possible */
    return coalesce(bp);            
}

/*
 * place - puts a block of asize at bp block
 * This function is called by malloc after it finds a block to put the 
 * payload in. The minimum block size is 16 bytes. 
 */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    /* difference is at least 24 bytes */
    if ((csize - asize) >= (24)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        rmv_from_free(bp);
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
        coalesce(bp);
    }
    /* not enough space for free block, don't split */
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        rmv_from_free(bp);
    }
}

/*
 * find_fit - traverses the free list to find the first block that is 
 * greater than or equal to in size to requested asize. 
 * NOTE: originally implemented was best_fit search algorithm which would
 * find the smallest block that would accommodate the size, but that 
 * proved to be less efficient than this first-fit search
 */
static void *find_fit(size_t asize)
{
    void *bp;
	
    /* traverse free list */
    for (bp = free_listp; GET_ALLOC(HDRP(bp)) == 0; bp = NEXT_FREE_BLKP(bp)) {
        if (asize <= (size_t)GET_SIZE(HDRP(bp)))
	        return bp;
    }

    return NULL; // No fit
}

/* 
 * insert_front - inserts free block bp at the front of the free_list
 * FILO (first in last out) free linked list, last node points to self
 */
static void insert_front(void *bp)
{
    NEXT_FREE_BLKP(bp) = free_listp;
    PREV_FREE_BLKP(free_listp) = bp;
    PREV_FREE_BLKP(bp) = NULL;
    free_listp = bp;
	return; 
}

/*
 * rmv_from_free - removes a block from the free list once it has been
 * allocated and no longer free to use
 */
static void rmv_from_free(void *bp)
{

    if (PREV_FREE_BLKP(bp)) /* check if bp is the first block in list */
        NEXT_FREE_BLKP(PREV_FREE_BLKP(bp)) = NEXT_FREE_BLKP(bp);
    else 
        free_listp = NEXT_FREE_BLKP(bp);

    PREV_FREE_BLKP(NEXT_FREE_BLKP(bp)) = PREV_FREE_BLKP(bp);

    return;
}

/* 
 * printBlock - prints details of the block, used for debugging purposes
 */
static void printBlock(void *bp)
{
	int hsize, halloc, fsize, falloc;

	hsize = GET_SIZE(HDRP(bp));
	halloc = GET_ALLOC(HDRP(bp));
	fsize = GET_SIZE(FTRP(bp));
	falloc = GET_ALLOC(FTRP(bp));

	if (hsize == 0){
		printf("%p: EOL\n", bp);
		return;
	}

	if (halloc)
		printf("%p: header:[%d:%c] footer:[%d:%c]\n", bp,hsize, (halloc ? 'a' : 'f'),fsize, (falloc ? 'a' : 'f'));
	else
		printf("%p:header:[%d:%c] prev:%p next:%p footer:[%d:%c]\n",bp, hsize, (halloc ? 'a' : 'f'), PREV_FREE_BLKP(bp),NEXT_FREE_BLKP(bp), fsize, (falloc ? 'a' : 'f'));
}

/*
 * checkBlock - used to check the alignment, boundary, and footer/header of a block
 * used for debugging purposes
 */
static void checkBlock(void *bp)
{
	
	if (NEXT_FREE_BLKP(bp)< mem_heap_lo() || NEXT_FREE_BLKP(bp) > mem_heap_hi())
		printf("Next pointer: %p is out of bounds\n", NEXT_FREE_BLKP(bp));

	if (PREV_FREE_BLKP(bp)< mem_heap_lo() || PREV_FREE_BLKP(bp) > mem_heap_hi())
		printf("Previous pointer: %p is out of bounds\n", PREV_FREE_BLKP(bp));
	
	if ((size_t)bp % 8)
		printf("%p is not aligned\n", bp);
	
	if (GET(HDRP(bp)) != GET(FTRP(bp)))
		printf("Header and Footer do not match\n");
}

/*
 * mm_checkheap - calls checkBlock and printBlock to check for errors in the heap
 * used for debugging purposes
 */
void mm_checkheap(int verbose)
{
	void *bp = heap_listp; 

	if (verbose)
		printf("Heap (%p):\n", heap_listp);
		
	if ((GET_SIZE(HDRP(heap_listp)) != 24) ||!GET_ALLOC(HDRP(heap_listp)))
		printf("Bad prologue header\n");
	checkBlock(heap_listp); 
	
	for (bp = free_listp; GET_ALLOC(HDRP(bp))==0; bp = NEXT_FREE_BLKP(bp))
	{
		if (verbose)
			printBlock(bp);
			checkBlock(bp);
	}
	
	if (verbose)
		printBlock(bp);
		
	if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp))))
		printf("Bad epilogue header\n");
}
