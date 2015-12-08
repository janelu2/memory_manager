/*
 * mm.c
 * 
 *
 * Todo: add a high level description of implementation 
 *
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
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* initialize an empty heap */
    if ((heap_listp = mem_sbrk(2 * 24)) == NULL)
        return -1;
    PUT(heap_listp, 0);                                  /* alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(24, 1));        /* prologue header */
    PUT(heap_listp + (2*WSIZE), 0);        /* prologue footer */
    PUT(heap_listp + (3*WSIZE), 0);            /* epilogue header */

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
 *     Always allocate a block whose size is a multiple of the alignment.
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
 * mm_free - Freeing a block does nothing.
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
 * mm_realloc - not written for now for sake of time
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

static void *coalesce(void *bp)
{
	size_t prev_alloc;
	prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))) || PREV_BLKP(bp) == bp;
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));
	
	if (prev_alloc && !next_alloc)
	{
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		rmv_from_free(NEXT_BLKP(bp));
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
	}

	else if (!prev_alloc && next_alloc)
	{
	  size += GET_SIZE(HDRP(PREV_BLKP(bp)));
	  bp = PREV_BLKP(bp);
	  rmv_from_free(bp);
	  PUT(HDRP(bp), PACK(size, 0));
	  PUT(FTRP(bp), PACK(size, 0));
	}

	else if (!prev_alloc && !next_alloc)
	{	
		size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
		GET_SIZE(HDRP(NEXT_BLKP(bp)));
		rmv_from_free(PREV_BLKP(bp));
		rmv_from_free(NEXT_BLKP(bp));
		bp = PREV_BLKP(bp);
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
	}
	insert_front(bp);
	return bp;
}

//given to us
void *extend_heap(size_t words)
{
    char *bp;
    size_t size;
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if (size < 24)
        size = 24;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));
    return coalesce(bp);
}

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

static void *find_fit(size_t asize)
{
    void *bp;
	
    for (bp = free_listp; GET_ALLOC(HDRP(bp)) == 0; bp = NEXT_FREE_BLKP(bp))
        {
        if (asize <= (size_t)GET_SIZE(HDRP(bp)))
	        return bp;
    }

    return NULL; // No fit
}

static void insert_front(void *bp)
{
    NEXT_FREE_BLKP(bp) = free_listp;
    PREV_FREE_BLKP(free_listp) = bp;
    PREV_FREE_BLKP(bp) = NULL;
    free_listp = bp;
	return; 
}

static void rmv_from_free(void *bp)
{

    if (PREV_FREE_BLKP(bp)) {
        NEXT_FREE_BLKP(PREV_FREE_BLKP(bp)) = NEXT_FREE_BLKP(bp);
    }

    else {
        free_listp = NEXT_FREE_BLKP(bp);
    }

    PREV_FREE_BLKP(NEXT_FREE_BLKP(bp)) = PREV_FREE_BLKP(bp);

    return;
}







