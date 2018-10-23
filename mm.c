/*
 * mm.c
 *
 * This is the only file you should modify.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* Basic constants and macros */
#define WSIZE       4       /* word size (bytes) */  
#define DSIZE       8       /* doubleword size (bytes) */
#define CHUNKSIZE  (1<<12)  /* initial heap size (bytes) */
#define OVERHEAD    8       /* overhead of header and footer (bytes) */

#define MAX(x, y) ((x) > (y)? (x) : (y))  

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
/* NB: this code calls a 32-bit quantity a word */
#define GET(p)       (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (unsigned int)(val))  

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x1)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. 
#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif
*/

static void *extend_heap(size_t words);
static void checkblock(void *bp);
static void *coalesce(void *bp);
static void *fit(size_t size);
static void place(void *bp, size_t asize);
static void printblock(void *bp);
unsigned int addrSplit(void *bp, int mode);
void *addrPair(int i1, int i2);

char *heap_listp = 0;  /* pointer to first block */
char *heap_hi;
int i = 0;

/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {
	//printf("--HEAP INIT\n");
  	if (mem_sbrk(7*WSIZE) == NULL)
    	return -1;
    heap_listp = mem_heap_lo();
    PUT(heap_listp, 0);  /* padding */
    PUT(heap_listp+(1*WSIZE), PACK(24, 1));  /* prologue header */ 
	PUT(heap_listp+(2*WSIZE), 0);  /* back pointer 1 */ 
	PUT(heap_listp+(3*WSIZE), 0);   /* back pointer 2 */
	PUT(heap_listp+(4*WSIZE), 0);  /* forward pointer 1 */ 
	PUT(heap_listp+(5*WSIZE), 0);   /* forward pointer 2 */
	PUT(heap_listp+(6*WSIZE), PACK(24, 1));  /* prologue footer */
	heap_listp = mem_heap_lo() + WSIZE;

	/* Extend the empty heap with a free block of CHUNKSIZE bytes */
	if (extend_heap(CHUNKSIZE/WSIZE) == NULL){
		//printf("initialization failed\n");
  		return -1;
  	}
  	//printf("HEAP INITIALIZED: low: %p high: %p\n", heap_listp, (heap_hi + 1));
	return 0;
}

/*
 * malloc
 */
void *mm_malloc(size_t size) {
	//printf("MALLOC CALLED: %d\n", (int)size);
	void* bp;
	size_t extendsize;

	if (heap_listp == 0){
    	mm_init();
  	}
    if(size <= 0){
    	return NULL;
    }

    /* Adjust block size to include overhead and alignment reqs. */
  	if (size <= 4*WSIZE)
    	size = 6*WSIZE;
  	else{
  		//printf("size check %d\n", (int)size);
    	size = (DSIZE * ((size + DSIZE + (DSIZE-1)) / DSIZE));
    	//printf("size check %d\n", (int)size);
    }
    //printf("size check %d\n", (int)size);

    if ((bp = fit(size)) != NULL) {
    	place(bp, size);
    	return (bp + WSIZE);
  	}

    extendsize = MAX(size,CHUNKSIZE);
  	if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
   		return NULL;
  	place(bp, size);
  	//printf("\tMALLOC: asize: %d location: %p\n", (int)size, bp);
  	return (bp + WSIZE);
}

/*
 * free
 */
void mm_free (void *ptr) {
	//printf("FREE CALLED %p\n", ptr);
	if(ptr != NULL){
		ptr = ptr - WSIZE;
	}
    if(!ptr){
    	//printf("\tFREE CALLED, ptr = 0\n");
    	return;
    }
    if (heap_listp == 0){
    	//printf("\tFREE CALLED, heap initialized\n");
    	mm_init();
  	}
    if(ptr == heap_listp){
    	//printf("\tFREE CALLED, tried root\n");
    	return;
    }
    PUT(ptr, PACK(GET_SIZE(ptr),0));
    PUT(ptr + GET_SIZE(ptr) - WSIZE , PACK(GET_SIZE(ptr),0));
    coalesce(ptr);
    return;
}

/*
 * realloc - you may want to look at mm-naive.c
 */
void *mm_realloc(void *ptr, size_t size)
{
	//printf("REALLOC CALLED: free %p size: %d\n", ptr, (int)size);
	if((ptr != NULL) && (size > 0)){
		ptr = ptr - WSIZE;
	}
  	size_t oldsize;
  	void *newptr;

  	/* If size == 0 then this is just free, and we return NULL. */
  	if(size <= 0) {
  		//printf("\tREALLOC CALLED, size <= 0\n");
    	mm_free(ptr);
    	return 0;
  	}

  	/* If oldptr is NULL, then this is just malloc. */
  	if(ptr == NULL) {
  		//printf("\tREALLOC CALLED, null ptr\n");
    	return mm_malloc(size);
  	}

  	newptr = mm_malloc(size);

  	/* If realloc() fails the original block is left untouched  */
  	if(!newptr) {
  		//printf("\tMALLOC IN REALLOC FAILED\n");
    	return 0;
  	}

  	/* Copy the old data. */
  	oldsize = GET_SIZE(ptr) - DSIZE;
  	if(size < oldsize) oldsize = size;
  	memcpy(newptr, ptr + WSIZE, oldsize);

  	/* Free the old block. */
  	mm_free(ptr + WSIZE);

  	return newptr;
}

/*
 * calloc - you may want to look at mm-naive.c
 * This function is not tested by mdriver, but it is
 * needed to run the traces.
 */
void *mm_calloc (size_t nmemb, size_t size)
{
  	void *ptr;
  	if (heap_listp == 0){
  		//printf("CALLOC CALLED, heap initialized\n");
    	mm_init();
  	}

  	ptr = mm_malloc(nmemb*size);

  	bzero(ptr, nmemb*size);


  	return ptr;
}

static void place(void *bp, size_t asize)
{
  	size_t csize = GET_SIZE(bp); 
  	void* prev = addrPair(GET(bp + WSIZE), GET(bp + 2*WSIZE));  
  	void* post = addrPair(GET(bp + 3*WSIZE), GET(bp + 4*WSIZE));

  	//printf("PLACE CALLED, ptr: %p freeSize: %d size: %d\n", bp, (int)csize, (int)asize);

  	if ((csize - asize) >= (4*WSIZE + 8)) {
  		PUT(bp, PACK(asize, 1));
   		PUT(bp + asize - WSIZE, PACK(asize, 1));
    	bp = bp + asize;
    	PUT(bp, PACK(csize-asize, 0));
    	PUT(bp + csize - asize - WSIZE, PACK(csize-asize, 0));

    	if((unsigned long int)prev != 0){
    		//printf("\tpointing prev free to split, prev: %p split: %p\n", prev, bp);
    		PUT(prev + 3*WSIZE, addrSplit(bp, 0));
    		PUT(prev + 4*WSIZE, addrSplit(bp, 1));
    	}

    	if((unsigned long int)post != 0){
    		//printf("\tpointing post free to split, post: %p split %p\n", post, bp);
    		PUT(post + WSIZE, addrSplit(bp, 0));
    		PUT(post + 2*WSIZE, addrSplit(bp, 1));
    	}
    	PUT(bp + WSIZE, addrSplit(prev, 0));
    	PUT(bp + 2*WSIZE, addrSplit(prev, 1));
    	PUT(bp + 3*WSIZE, addrSplit(post, 0));
    	PUT(bp + 4*WSIZE, addrSplit(post, 1));
  	}
  	else { 
    	PUT(bp, PACK(csize, 1));
   		PUT(bp + csize - WSIZE, PACK(csize, 1));
   		if((unsigned long int)post != 0){
   			PUT(post + WSIZE, GET(bp + WSIZE));
   			PUT(post + 2*WSIZE, GET(bp + 2*WSIZE));
   		}
   		PUT(prev + 3*WSIZE, GET(bp + 3*WSIZE));
   		PUT(prev + 4*WSIZE, GET(bp + 4*WSIZE));
  	}
}

static void *fit(size_t size){
	void *bp;
	//printf("FIT CALLED. %d\n", (int)size);

  	for(bp = heap_listp; ((bp != 0) && (GET_SIZE(bp) > 1)); bp = addrPair(GET(bp + 3*WSIZE), GET(bp + 4*WSIZE))){
  		//printf("\t size: %d alloc: %d\n", (int)GET_SIZE(bp), (int)GET_ALLOC(bp));
    	if ((!GET_ALLOC(bp)) && (size <= (int)GET_SIZE(bp))){
    		//printf("\tFIT CALLED: FOUND FIT AT %p\n", bp);
    		return bp;
    	}
  	}
  	//printf("FIT CALLED: NO FIT FOUND\n");
  	return NULL;
}

unsigned int addrSplit(void *bp, int mode){
	if(mode == 0)
		return((unsigned int)((unsigned long)bp & 0xFFFFFFFF));
	else
		return((unsigned int)(((unsigned long)bp & 0xFFFFFFFF00000000) >> 32));
}

//i1 is least sig, i2 is most sig
void *addrPair(int i1, int i2){
	return((void*)((((unsigned long int)i2) << 32)|(unsigned long int)i1));
}

/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 
static int in_heap(const void *p) {
    return p < mem_heap_hi() && p >= mem_heap_lo();
}


 * Return whether the pointer is aligned.
 * May be useful for debugging.
 
static int aligned(const void *p) {
    return (size_t)ALIGN(p) == (size_t)p;
}
*/

/*
 * mm_checkheap
 */
void mm_checkheap(int verbose)
{
  char *bp = heap_listp;

  if (verbose)
    printf("Heap (%p):\n", heap_listp);

  if ((GET_SIZE(heap_listp) != 24) || !GET_ALLOC(heap_listp))
    printf("Bad prologue header\n");
  checkblock(heap_listp);

  for(bp = heap_listp; ((bp != 0) && (GET_SIZE(bp) > 1)); bp = addrPair(GET(bp + 3*WSIZE), GET(bp + 4*WSIZE))) {
      printblock(bp);
    checkblock(bp);
  }
}

static void checkblock(void *bp) 
{
  if ((size_t)(bp + WSIZE) % 8)
    printf("Error: %p is not doubleword aligned\n", bp);
/*
  if (GET(bp) != GET(bp + GET_SIZE(bp) - WSIZE)){
    printf("Error: header does not match footer\n");
    printf("%lu\n", (unsigned long int)bp);
    printf("%d\n", (int)GET_SIZE(bp));
    printf("%lu\n", (unsigned long int)bp + GET_SIZE(bp) - WSIZE);
    printf("%lu\n", (unsigned long int)(mem_heap_hi() - 3));
	}
	*/
}

//bp is pointer to a block's prologue
void *coalesce(void *bp){
	int L = 0;
	int H = 0;
	//printf("COALESCE CALLED: %p\n", bp);

	if(GET(bp) & 1){
		//printf("\t coalesce failed, bp allocated\n");
		return NULL;
	}

	if(!GET_ALLOC(bp - WSIZE) && GET_SIZE(bp - WSIZE)){
		L = 1;
		//printf("\t Lblock free, %p, LblockSize: %d\n", (bp - GET_SIZE(bp - WSIZE)), (int)GET_SIZE(bp - WSIZE));
	}

	if((unsigned long int)(bp + GET_SIZE(bp)) <= (unsigned long int)(heap_hi)){
		if(!GET_ALLOC(bp + GET_SIZE(bp)) && GET_SIZE(bp + GET_SIZE(bp))){
			H = 1;
			//printf("\t Hblock free, %p, HblockSize: %d\n", (bp + GET_SIZE(bp)), (int)GET_SIZE(bp + GET_SIZE(bp)));
		}
	}
	if(!(H || L)){
		//back pointers point to root
		PUT(bp + WSIZE, addrSplit(heap_listp, 0));
		PUT(bp + 2*WSIZE, addrSplit(heap_listp, 1));

		//forward pointers point to fpoints of root
		PUT(bp + 3*WSIZE, GET(heap_listp + 3*WSIZE));
		PUT(bp + 4*WSIZE, GET(heap_listp + 4*WSIZE));

		void* back;
		back = addrPair(GET(heap_listp + 3*WSIZE), GET(heap_listp + 4*WSIZE));
		if(back != 0)
		{
			PUT(back + WSIZE, addrSplit(bp, 0));
			PUT(back + 2*WSIZE, addrSplit(bp, 1));
		}

		//root fpoints to block
		PUT(heap_listp + 3*WSIZE, addrSplit(bp, 0));
		PUT(heap_listp + 4*WSIZE, addrSplit(bp, 1));
		return bp;
	}

	if(H){
		void* bH;
		void* aH;
		unsigned int s = GET_SIZE(bp);

		bH = addrPair(GET(bp + s + WSIZE), GET(bp + s + 2*WSIZE));
		aH = addrPair(GET(bp + s + 3*WSIZE), GET(bp + s + 4*WSIZE));

		if(bH != 0){
			PUT(bH + 3*WSIZE, GET(bp + s + 3*WSIZE));
			PUT(bH + 4*WSIZE, GET(bp + s + 4*WSIZE));
		}
		if(aH != 0){
			PUT(aH + 1*WSIZE, GET(bp + s + 1*WSIZE));
			PUT(aH + 2*WSIZE, GET(bp + s + 2*WSIZE));
		}

		PUT(bp + s + GET_SIZE(bp + s) - WSIZE, PACK(s + GET_SIZE(bp + s), 0));
		PUT(bp, PACK(s + GET_SIZE(bp + s), 0));

		PUT(bp + 3*WSIZE, GET(heap_listp + 3*WSIZE));
		PUT(bp + 4*WSIZE, GET(heap_listp + 4*WSIZE));

		PUT(bp + WSIZE, addrSplit(heap_listp,0));
		PUT(bp + 2*WSIZE, addrSplit(heap_listp,1));

		void* back;
		back = addrPair(GET(heap_listp + 3*WSIZE), GET(heap_listp + 4*WSIZE));
		if(back != 0){
			PUT(back + WSIZE, addrSplit(bp, 0));
			PUT(back + 2*WSIZE, addrSplit(bp, 1));
		}

		PUT(heap_listp + 3*WSIZE, addrSplit(bp, 0));
		PUT(heap_listp + 4*WSIZE, addrSplit(bp, 1));
		return bp;
	}
	

	if(L){
		void* bL;
		void* aL;
		unsigned int s = GET_SIZE(bp - WSIZE);

		bL = addrPair(GET(bp - s + WSIZE), GET(bp - s + 2*WSIZE));
		aL = addrPair(GET(bp - s + 3*WSIZE), GET(bp - s + 4*WSIZE));

		if(bL != 0){
			PUT(bL + 3*WSIZE, GET(bp - s + 3*WSIZE));
			PUT(bL + 4*WSIZE, GET(bp - s + 4*WSIZE));
		}

		if(aL != 0){
			PUT(aL + 1*WSIZE, GET(bp - s + 1*WSIZE));
			PUT(aL + 2*WSIZE, GET(bp - s + 2*WSIZE));
		}

		PUT(bp + GET_SIZE(bp) - WSIZE, PACK(s + GET_SIZE(bp), 0));
		PUT(bp - s, PACK(s + GET_SIZE(bp), 0));

		PUT(bp - s + 3*WSIZE, GET(heap_listp + 3*WSIZE));
		PUT(bp - s + 4*WSIZE, GET(heap_listp + 4*WSIZE));

		PUT(bp - s + WSIZE, addrSplit(heap_listp, 0));
		PUT(bp - s + 2*WSIZE, addrSplit(heap_listp, 1));

		void* back;
		back = addrPair(GET(heap_listp + 3*WSIZE), GET(heap_listp + 4*WSIZE));
		if(back != 0){
			PUT(back + WSIZE, addrSplit(bp - s, 0));
			PUT(back + 2*WSIZE, addrSplit(bp - s, 1));
		}

		PUT(heap_listp + 3*WSIZE, addrSplit(bp - s, 0));
		PUT(heap_listp + 4*WSIZE, addrSplit(bp - s, 1));
		return (bp - s);
	}

	if(H && L){
		void* bL;
		void* aL;
		void* bH;
		void* aH;
		unsigned int sL = GET_SIZE(bp - 4);
		unsigned int s = GET_SIZE(bp);

		bL = addrPair(GET(bp - sL + WSIZE), GET(bp - sL + 2*WSIZE));
		aL = addrPair(GET(bp - sL + 3*WSIZE), GET(bp - sL + 4*WSIZE));
		bH = addrPair(GET(bp + s + WSIZE), GET(bp + s + 2*WSIZE));
		aH = addrPair(GET(bp + s + 3*WSIZE), GET(bp + s + 4*WSIZE));

		if(bL != 0){
			PUT(bL + 3*WSIZE, GET(bp - sL + 3*WSIZE));
			PUT(bL + 4*WSIZE, GET(bp - sL + 4*WSIZE));
		}
		if(aL != 0){
			PUT(aL + 1*WSIZE, GET(bp - sL + 1*WSIZE));
			PUT(aL + 2*WSIZE, GET(bp - sL + 2*WSIZE));
		}

		if(bH != 0){
			PUT(bH + 3*WSIZE, GET(bp + s + 3*WSIZE));
			PUT(bH + 4*WSIZE, GET(bp + s + 4*WSIZE));
		}
		if(aH != 0){
			PUT(aH + 1*WSIZE, GET(bp + s + 1*WSIZE));
			PUT(aH + 2*WSIZE, GET(bp + s + 2*WSIZE));
		}

		PUT(bp + s + GET_SIZE(bp + s) - WSIZE, PACK(sL + s + GET_SIZE(bp + s), 0));
		PUT(bp - sL, PACK(sL + s + GET_SIZE(bp + s), 0));

		PUT(bp - sL + 3*WSIZE, GET(heap_listp + 3*WSIZE));
		PUT(bp - sL + 4*WSIZE, GET(heap_listp + 4*WSIZE));

		PUT(bp - sL + WSIZE, addrSplit(heap_listp,0));
		PUT(bp - sL + 2*WSIZE, addrSplit(heap_listp,1));

		void* back;
		back = addrPair(GET(heap_listp + 3*WSIZE), GET(heap_listp + 4*WSIZE));
		if(back != 0){
			PUT(back + WSIZE, addrSplit(bp - sL, 0));
			PUT(back + 2*WSIZE, addrSplit(bp - sL, 1));
		}

		PUT(heap_listp + 3*WSIZE, addrSplit(bp - sL, 0));
		PUT(heap_listp + 4*WSIZE, addrSplit(bp - sL, 1));
		return (bp - s);
	}
	
	return NULL;
}


//extends heap, returns pointer of new block
static void *extend_heap(size_t words){
	//printf("EXTEND CALLED\n");
	words = (words + (words & 1))*WSIZE;
	words = MAX(words, (1 << 12));
	void *return_ptr;
	void *bp;

	if ((long)(bp = mem_sbrk(words)) < 0) 
    	return NULL;
    memset(bp, 0, words);
    heap_hi = mem_heap_hi();
    PUT(bp, PACK(words, 0));
    PUT(heap_hi, PACK(words, 0));

    //printf("\t EXTEND CALLING COALESCE: BYTES: %d\n", (int)words);
    return_ptr = coalesce(bp);
  	mm_checkheap(0);
  	//printf("\tEXTEND: %d, new high byte: %p\n", (int)words, heap_hi);
  	return return_ptr;
}

static void printblock(void *bp) 
{
  size_t hsize;//, halloc, fsize, falloc;

  hsize = GET_SIZE(bp);
  //halloc = GET_ALLOC(HDRP(bp));  
  //fsize = GET_SIZE(FTRP(bp));
  //falloc = GET_ALLOC(FTRP(bp));  

  if (hsize == 0) {
    printf("%p: EOL\n", bp);
    return;
  }

  /*  printf("%p: header: [%p:%c] footer: [%p:%c]\n", bp, 
      hsize, (halloc ? 'a' : 'f'), 
      fsize, (falloc ? 'a' : 'f')); */
}

