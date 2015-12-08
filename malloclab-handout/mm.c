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

team_t team = {
    /* Team name */
    "riding_solo",
    /* Name */
    "Jane Lu",
    /* Email */
    "jlu00@nmt.edu",
    "",
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* basic constants and macros */
#define WSIZE   4
#define DSIZE   8
#define CHUNKSIZE (1 << 12)

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* pack a size and allocated bit into a word */
#define PACK(size, alloc)       ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)          (*(unsigned int *)(p))
#define PUT(p, val)     (*(unsigned int *)(p) = (val))

/* read the size and allocated fields from p */
#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* given block ptr bp, compute address of header and footer */
#define HDRP(bp)        ((char *)(bp) - WSIZE)
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* global variables */
    void *heap_listp;
    void *largest = NULL;

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* initialize an empty heap */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                                  /* alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));        /* prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));        /* prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));            /* epilogue header */
    heap_listp += (2*WSIZE);

    /* Extend the empty heap witha  free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
      
    return 0;
}

void *extend_heap(size_t words)
{
    char *bp;
    size_t size;
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));
    return coalesce(bp);
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

    /* adjust block size to include overhead and alignment */
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else 
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/DSIZE);

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

//implement best-fit search
void *find_fit(size_t asize)
{
    /* first-fit search */
    void *bp = NULL;
    void *tmp = NULL;
    int best_fit = 1000000000;
    void *best_fitp = NULL;
    int max = 0; 
    int curr_size = 0;

    //NEED TO make this to work
    /*if (largest != NULL) {
        if (GET_SIZE(HDRP(largest)) < asize)
            return NULL;
        else if (GET_SIZE(HDRP(largest)) == asize) {
            tmp = largest;
            largest = NULL;
            return tmp;
        }
    }*/

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!(GET_ALLOC(HDRP(bp)))) {
            curr_size = GET_SIZE(HDRP(bp));
            if (curr_size > max) {
                max = curr_size;
                //largest = bp;
            }
            if (curr_size >= asize) {           
                if((curr_size - asize) < (best_fit - asize)) {
                    best_fit = curr_size;
                    best_fitp = bp;
                }
            }
        }
    }

    return best_fitp; 
}

void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2*DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
    }

    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    /* the previous and next blocks are both allocated */
    if (prev_alloc && next_alloc) { 
        return bp;
    }

    /* the previous block is allocated and the next blocks is free */
    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    /* the previous block is free and teh next block is allocated */
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    /* the previous and next blocks are both free */
    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    return bp;

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














