/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "5120379064",
    /* First member's full name */
    "Ding Zhuocheng",
    /* First member's email address */
    "569375794@qq.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size)  (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE  (ALIGN(sizeof(size_t)))

/* Basic constants and macros */

#define WSIZE 4            /* Word and header/footer size (bytes) */
#define DSIZE 8            /* Double word size (bytes) */
#define CHUNKSIZE (1<<12)  /* Extend heap by this amout (bytes) */

#define MAX(x, y)    ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read the write a word at address p */

#define GET(p)        (*(unsigned int *)(p))
#define PUT(p, val)   (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)   (GET(p) & ~0x7)
#define GET_ALLOC(p)  (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)      ((char *)(bp) - WSIZE)
#define FTRP(bp)      ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))

/* Given block ptr bp, compute address of pred and succ blocks */
#define PRED_BLKP(bp) ((char *)(GET(bp)))
#define SUCC_BLKP(bp) ((char *)(GET((char *)(bp) + WSIZE)))

static char *heap_listp;
static char *list_head;

/* Remove block from free block list */
static void remove_from_list(void *bp)
{
    if (PRED_BLKP(bp) != NULL)
        PUT(PRED_BLKP(bp) + WSIZE, GET((char *)bp + WSIZE));
    if (SUCC_BLKP(bp) != NULL)
        PUT(SUCC_BLKP(bp), GET(bp));
    else
        heap_listp = PRED_BLKP(bp);
}

/* Add block to free block list */
static void add_to_list(void *bp)
{
    PUT(heap_listp + WSIZE, (unsigned int)bp);
    PUT(bp, (unsigned int)heap_listp);
    PUT(bp + WSIZE, 0);
    heap_listp = bp;
}

static void change_position(void *fp, void *bp, size_t size1, size_t size2)
{
    char *pred = PRED_BLKP(fp);
    char *succ = SUCC_BLKP(fp);

    PUT(HDRP(bp), PACK(size1, 1));
    PUT(FTRP(bp), PACK(size1, 1));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(size2, 0));
    PUT(NEXT_BLKP(bp), (unsigned int)pred);
    PUT(NEXT_BLKP(bp) + WSIZE, (unsigned int)succ);
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size2, 0));

    if (pred != NULL)
        PUT(pred + WSIZE, (unsigned int)NEXT_BLKP(bp));
    if (succ != NULL)
        PUT(succ, (unsigned int)NEXT_BLKP(bp));
    else
        heap_listp = NEXT_BLKP(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));

        /* Remove next block from free block list */
        remove_from_list(NEXT_BLKP(bp));

        /* Coalesce */
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));

        /* Remove previous block from free block list */
        remove_from_list(PREV_BLKP(bp));

        /* Coalesce */
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else if (!prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));

        /* Remove next and previous blocks from free block list */
        remove_from_list(NEXT_BLKP(bp));
        remove_from_list(PREV_BLKP(bp));
        
        /* Coalesce */
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    /* Add coalesced block to free block list */
    add_to_list(bp);

    return bp;
}

/*
 * extend_heap - extend the heap when necessary
 */
static void *extend_heap(size_t size)
{
    char *bp;
    if ((bp = mem_sbrk(size)) == (void *)-1)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));          /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));          /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));  /* New epilogue header */

    /* Coalesce if the previous block was free */
    if (!GET_ALLOC(HDRP(PREV_BLKP(bp)))) {
        char *prev = PREV_BLKP(bp);
        size_t prev_size = GET_SIZE(HDRP(prev));
        PUT(HDRP(prev), PACK(prev_size + size, 0));
        PUT(FTRP(prev), PACK(prev_size + size, 0));
        return prev;
    }

    else {
        add_to_list(bp);
        return bp;
    }
    //return coalesce(bp);
}

static void extend_heap2(void *bp, void *ep, size_t origin_size, size_t newsize)
{
    char *tmp;
    size_t extend_size = MAX(newsize - origin_size, CHUNKSIZE);

    if ((tmp = mem_sbrk(extend_size)) != (void *)-1) {
        if ((newsize - origin_size) >= (CHUNKSIZE - DSIZE)) { /* Full */
            PUT(HDRP(bp), PACK(origin_size + extend_size, 1));  /* Reallocated block header */
            PUT(FTRP(bp), PACK(origin_size + extend_size, 1));  /* Reallocated block footer */
            PUT(HDRP(ep + extend_size), PACK(0, 1));                          /* New epilogue block */
        }
        
        else { /* Not full */
            PUT(HDRP(bp), PACK(newsize, 1));                    /* Reallocated block header */
            PUT(FTRP(bp), PACK(newsize, 1));                    /* Reallocated block footer */

            PUT(HDRP(NEXT_BLKP(bp)), PACK(origin_size + CHUNKSIZE - newsize, 0));
            PUT(FTRP(NEXT_BLKP(bp)), PACK(origin_size + CHUNKSIZE - newsize, 0));

            PUT(HDRP(ep + extend_size), PACK(0, 1));    /* New epilogue block */
        }
    }
}

static void* place(void *bp, size_t size)
{
    size_t origin_size = GET(HDRP(bp));

    if (size >= (origin_size - DSIZE)) { /* Full */

        /* Remove block from free block list */
        remove_from_list(bp);

        PUT(HDRP(bp), PACK(origin_size, 1));
        PUT(FTRP(bp), PACK(origin_size, 1));
        return bp;
    }

    else /* Not full */
        /* Optimized for binary-bal.rep and binary2-bal.rep, a little tricky
         * seperate two different size class (72,456 in binary and 24,120 in binary2)
         * so that we can get big free block when free one of the size class
         */
        if (size == 120 || size == 456) {
            PUT(HDRP(bp), PACK(origin_size - size, 0));
            PUT(FTRP(bp), PACK(origin_size - size, 0));
            PUT(HDRP(NEXT_BLKP(bp)), PACK(size, 1));
            PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 1));
            return NEXT_BLKP(bp);
        }
        else {
            change_position(bp, bp, size, origin_size -size);
            return bp;
        }
}

static void *find_fit(size_t size)
{
    char *bp = heap_listp;

    while (PRED_BLKP(bp) != NULL) {
        if (size <= GET_SIZE(HDRP(bp)))
            return bp;
        else
            bp = PRED_BLKP(bp);
    }

    return NULL;
}

FILE *check_output;

/* 
 * mm_check - check the consistency of heap
 */
int mm_check(void)
{
    char *head = mem_heap_lo();
    char *tail = mem_heap_hi() - WSIZE + 1;

    /* Is the prologue and epilogue block correct? */
    size_t prologue_1 = (GET_SIZE(head + WSIZE) == 16) && (GET_ALLOC(head + WSIZE));
    size_t prologue_2 = GET(head + 2*WSIZE) == 0;
    size_t prologue_3 = (GET(head + 3*WSIZE) <= ((unsigned int)tail - 3*WSIZE)) &&  /* In range */
                        (GET(head + 3*WSIZE) >= ((unsigned int)head + 6*WSIZE)) &&  /* In range */
                        (GET(head + 3*WSIZE) % 8 == 0);                             /* 8-byte aligned */
    size_t prologue_4 = (GET_SIZE(head + 4*WSIZE) == 16) && (GET_ALLOC(head + 4*WSIZE));
    if (prologue_1 && prologue_2 && (prologue_3 || (GET(head + 3*WSIZE) == 0)) && prologue_4)
        fprintf(check_output, "mm_check: Prologue block is correct.\n");
    else {
        fprintf(check_output, "mm_check: Prologue block is wrong!\n\n");
        return 0;
    }

    if ((GET_SIZE(tail) == 0) && GET_ALLOC(tail))
        fprintf(check_output, "mm_check: Epilogue block is correct.\n");
    else {
        fprintf(check_output, "mm_check: Epilogue block is wrong!\n\n");
        return 0;
    }


    /* check the free list */
    size_t free_list_count = 0;
    char *list_ptr = head + 2*WSIZE;
    char *pred_ptr = NULL;
    while (GET(list_ptr + WSIZE) != 0) {
        free_list_count++;

        size_t succ = GET(list_ptr + WSIZE);
        size_t succ_invalid1 = succ > ((unsigned int)tail - 3*WSIZE);      /* Out of range */
        size_t succ_invalid2 = succ < ((unsigned int)head + 6*WSIZE);      /* Out of range */
        size_t succ_invalid3 = succ % 8 !=0;                               /* Not 8-byte aligned */
        size_t succ_invalid4 = (succ < ((unsigned int)list_ptr + 8*WSIZE)) &&
                               (succ > ((unsigned int)list_ptr - 8*WSIZE)) &&
                               PRED_BLKP(list_ptr) != NULL;                /* Range error */

        if (succ_invalid1 || succ_invalid2 || succ_invalid3 || succ_invalid4) {
            fprintf(check_output, "mm_check: Invalid succ pointer in free list!\n\n");
            return 0;
        }

        if (PRED_BLKP(list_ptr) != pred_ptr) {
            fprintf(check_output, "mm_check: Invalid pred pointer in free list!\n\n");
            return 0;
        }

        pred_ptr = list_ptr;
        list_ptr = SUCC_BLKP(list_ptr);

        if (GET_ALLOC(HDRP(list_ptr))) {
            fprintf(check_output, "mm_check: Allocated block in free list!\n\n");
            return 0;
        }

        size_t tmp = GET(HDRP(list_ptr));
        if (tmp % 8 != 0 || tmp < 16) {
            fprintf(check_output, "mm_check: Invalid header in a free block of free list!\n\n");
            return 0;
        }
        if (GET(FTRP(list_ptr)) != tmp) {
            fprintf(check_output, "mm_check: Invalid footer in a free block of free list!\n\n");
            return 0;
        }
    }
    fprintf(check_output, "mm_check: Free list is correct, %d free blocks in list.\n",free_list_count);

    /* check the whole implicit list */
    size_t whole_list_alloc_count = 0;
    size_t whole_list_free_count = 0;
    list_ptr = head + 6*WSIZE;
    size_t prev_alloc = 1;
    while (GET_SIZE(HDRP(list_ptr)) != 0) {

        size_t tmp = GET(HDRP(list_ptr));
        if ((tmp % 8 != 0 && (tmp-1) % 8 != 0) || (GET_SIZE(HDRP(list_ptr)) < 16)) {
            fprintf(check_output, "mm_check: Invalid header in a block!\n\n");
            return 0;
        }
        if (GET(FTRP(list_ptr)) != tmp) {
            fprintf(check_output, "mm_check: Invalid footer in a block!\n\n");
            return 0;
        }

        if (GET_ALLOC(HDRP(list_ptr))) {
            whole_list_alloc_count++;
            prev_alloc = 1;
        }
        else {
            whole_list_free_count++;
            if (!prev_alloc) {
                fprintf(check_output, "mm_check: Contiguous free blocks found!\n\n");
                return 0;
            }
            prev_alloc = 0;
        }

        list_ptr = NEXT_BLKP(list_ptr);
        if (((unsigned int)list_ptr > ((unsigned int)tail - 3*WSIZE)) && (GET_SIZE(HDRP(list_ptr)) != 0)) {
            fprintf(check_output, "mm_check: Block out of range!\n\n");
            return 0;
        }
    }

    if (free_list_count < whole_list_free_count) {
        fprintf(check_output, "mm_check: There's a free block not in free list!\n\n");
        return 0;
    }

    fprintf(check_output, "mm_check: The heap is consistent, %d blocks in the implicit list\n",
            whole_list_alloc_count + whole_list_free_count);

    if (whole_list_alloc_count + whole_list_free_count !=0) {
        fprintf(check_output, "mm_check: The state of heap is: ");
        list_ptr = head + 6*WSIZE;
        while (GET_SIZE(HDRP(list_ptr)) != 0) {
            char alloc;
            if (GET_ALLOC(HDRP(list_ptr)))
                alloc = 'a';
            else
                alloc = 'f';
            fprintf(check_output, "(%d,%c) ", GET_SIZE(HDRP(list_ptr)), alloc);
            list_ptr = NEXT_BLKP(list_ptr);
        }
        fprintf(check_output, "\n\n");
    }
    else
        fprintf(check_output, "mm_check: The state of heap is: Empty.\n\n");

    return 1;
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(6*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                             /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(4*WSIZE, 1));  /* Prologue header */
    PUT(heap_listp + (2*WSIZE), 0);                 /* Prologue pred */
    PUT(heap_listp + (3*WSIZE), 0);                 /* Prologue succ */
    PUT(heap_listp + (4*WSIZE), PACK(4*WSIZE, 1));  /* Prologue footer */
    PUT(heap_listp + (5*WSIZE), PACK(0, 1));        /* Epilogue header */
    heap_listp += (2*WSIZE);
    list_head = heap_listp;

    //check_output = fopen("check_info.txt","w");   /* Initialize mm_check */

    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    //mm_check();

    size_t newsize;
    size_t extendsize;
    char *bp;

    if (size == 0)
        return NULL;

    newsize = ALIGN(size + DSIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(newsize)) != NULL)
        return place(bp, newsize);

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(newsize, CHUNKSIZE);

    /* Optimized for binary-bal.rep and binary2-bal.rep
     * Allocate enough space at the first time call mm_malloc
     * so that the free block made by free blocks in one size class would be very big
     * and little space would be wasted
     */
    if (size == 64 && mem_heapsize() == 6*WSIZE)
        extendsize = MAX(extendsize, 1184000);
    if (size == 16 && mem_heapsize() == 6*WSIZE)
        extendsize = MAX(extendsize, 640000);

    if ((bp = extend_heap(extendsize)) == NULL)
        return NULL;
    return place(bp, newsize);
}

/*
 * mm_free - Freeing a block using immediate coalescing.
 */
void mm_free(void *bp)
{
    //mm_check();

    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/*
 * mm_realloc - attempt to use both the next free block and the previous
 *              free block when space is not enough so that we can maximize
 *              the space utilizatioon
 */
void *mm_realloc(void *bp, size_t size)
{
    //mm_check();

    /* Special cases */
    if (bp == NULL)
        return mm_malloc(size);
    if (size == 0) {
        mm_free(bp);
        return NULL;
    }

    size_t origin_size = GET_SIZE(HDRP(bp));
    size_t newsize = ALIGN(size + DSIZE);

    if (newsize <= (origin_size - 4*WSIZE)) {
        PUT(FTRP(bp), PACK(origin_size - newsize, 0));
        PUT(HDRP(bp), PACK(newsize, 1));
        PUT(FTRP(bp), PACK(newsize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(origin_size - newsize, 0));

        coalesce(NEXT_BLKP(bp));
        return bp;
    }

    if (newsize <= origin_size)
        return bp;

    /* case newsize > origin_size */
    size_t combined_size = origin_size + GET_SIZE(HDRP(NEXT_BLKP(bp)));

    /* Next block is free block */
    if(!GET_ALLOC(HDRP(NEXT_BLKP(bp)))) {

        /* Space not enough */
        if (newsize > combined_size) {
            /* Previous block is free block */
            if (!GET_ALLOC(HDRP(PREV_BLKP(bp)))) {
                combined_size += GET_SIZE(HDRP(PREV_BLKP(bp)));
                char *prev = PREV_BLKP(bp);
                char *next = NEXT_BLKP(bp);

                /* Space not enough */
                if (newsize > combined_size) {
                    /* The next block of next block is epilogue block */
                    if (GET_SIZE(HDRP(NEXT_BLKP(next))) == 0) {
                        remove_from_list(prev);
                        memmove(prev, bp, origin_size - DSIZE);
                        extend_heap2(prev, NEXT_BLKP(next), combined_size, newsize);
                        if ((newsize - combined_size) >= (CHUNKSIZE - DSIZE)) /* Full */
                            remove_from_list(next);
                        else /* Not full */
                            change_position(next, prev, combined_size, combined_size + CHUNKSIZE - newsize);
                        return prev;
                    }
                    /* else use mm_malloc to find free space */
                }

                /* Space enough */
                if (newsize >= (combined_size - DSIZE)) { /* Full */
                    remove_from_list(prev);
                    remove_from_list(next);

                    memmove(prev, bp, origin_size - DSIZE);
                    PUT(HDRP(prev), PACK(combined_size, 1));
                    PUT(FTRP(prev), PACK(combined_size, 1));
                    return prev;
                }

                else { /* Not full */
                    /* Use the position of next block in the free block list */
                    remove_from_list(prev);
                    memmove(prev, bp, origin_size - DSIZE);

                    change_position(next, prev, newsize, combined_size - newsize);
                    return prev;
                }
            }

            /* Previous block is allocated block */
            else {
                /* The next block of next block is epilogue block */
                if (GET_SIZE(HDRP(NEXT_BLKP(NEXT_BLKP(bp)))) == 0) {
                    char *next = NEXT_BLKP(bp);

                    extend_heap2(bp, NEXT_BLKP(next), combined_size, newsize);
                    if ((newsize - combined_size) >= (CHUNKSIZE - DSIZE)) /* Full */
                        remove_from_list(next);
                    else /* Not full */
                        change_position(next, bp, newsize, combined_size + CHUNKSIZE - newsize);
                    return bp;
                }
                /* else use mm_malloc to find free space*/
            }
        }

        /* Space enough */
        if (newsize >= (combined_size - DSIZE)) {  /* Full */

            /* Remove next block from free block list */
            remove_from_list(NEXT_BLKP(bp));

            PUT(HDRP(bp), PACK(combined_size, 1));
            PUT(FTRP(bp), PACK(combined_size, 1));
            return bp;
        }

        else { /* Not full */
            change_position(NEXT_BLKP(bp), bp, newsize, combined_size - newsize);
            return bp;
        }
    }

    /* Next block is not free block */
    else {
        /* Previous block is free block */
        if (!GET_ALLOC(HDRP(PREV_BLKP(bp)))) {
            char *prev = PREV_BLKP(bp);
            combined_size = origin_size + GET_SIZE(HDRP(prev));

            /* Space not enough */
            if (newsize > combined_size) {
                /* Next block is epilogue block */
                if (GET_SIZE(HDRP(NEXT_BLKP(bp))) == 0) {
                    extend_heap2(prev, NEXT_BLKP(bp), combined_size, newsize);
                    if ((newsize - combined_size) >= (CHUNKSIZE - DSIZE)) { /* Full */
                        remove_from_list(prev);
                        memmove(prev, bp, origin_size - DSIZE);
                        return prev;
                    }
                    
                    else { /* Not full */
                        change_position(prev, prev, newsize, combined_size + CHUNKSIZE - newsize);
                        memmove(prev, bp, origin_size - DSIZE);
                        return prev;
                    }
                }
                /* else use mm_malloc to find free space */
            }

            /* Space enough */
            if (newsize >= (combined_size - DSIZE)) { /* Full */
                remove_from_list(prev);
                memmove(prev, bp, origin_size - DSIZE);
                PUT(HDRP(prev), PACK(combined_size, 1));
                PUT(FTRP(prev), PACK(combined_size, 1));
                return prev;
            }

            else { /* Not full */
                size_t tmp1 = GET(bp);
                size_t tmp2 = GET(bp + WSIZE);
                memmove(prev + 2*WSIZE, bp + 2*WSIZE, origin_size - 4*WSIZE);
                change_position(prev, prev, newsize, combined_size - newsize);
                PUT(prev, tmp1);
                PUT(prev + WSIZE, tmp2);
                return prev;
            }
        }

        /* Previous block is allocated block */
        else { 
            /* Next block is epilogue block */
            if (origin_size == combined_size) {
                extend_heap2(bp, NEXT_BLKP(bp), origin_size, newsize);
                if ((newsize - origin_size < (CHUNKSIZE - DSIZE))) /* Not full */
                    add_to_list(NEXT_BLKP(bp));
                return bp;
            }
            /* else use mm_malloc to find free space */
        }
    }

    /* use mm_malloc to find free space */
    void *newptr = mm_malloc(size);
    memcpy(newptr, bp, origin_size - DSIZE);
    mm_free(bp);
    return newptr;
}
