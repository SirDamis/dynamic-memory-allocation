/*
 * Simple Malloc - Implicit Free List Allocator
 * Based on the CS:APP (Computer Systems: A Programmer's Perspective) textbook
 *
 * Design:
 *   - Uses an implicit free list with boundary tags (headers & footers)
 *   - First-fit search strategy for finding free blocks
 *   - Immediate coalescing on free to reduce fragmentation
 *   - Block splitting when remainder >= minimum block size (2 * DSIZE)
 *
 * Block Structure:
 *   +--------+---------------------------+--------+
 *   | Header |        Payload            | Footer |
 *   | 4 bytes|       (user data)         | 4 bytes|
 *   +--------+---------------------------+--------+
 *   ^        ^
 *   |        +-- bp (block pointer returned to user)
 *   +-- Header stores: [size (29 bits) | unused (2 bits) | alloc (1 bit)]
 *
 * Heap Layout:
 *   [Padding | Prologue Hdr | Prologue Ftr | Block(s)... | Epilogue Hdr]
 *      4B         4B             4B                           4B
 *
 * API:
 *   - mm_init()  : Initialize the heap with prologue/epilogue blocks
 *   - mm_malloc(): Allocate a block of the requested size
 *   - mm_free()  : Free a previously allocated block
 */

#define _DEFAULT_SOURCE // Required for sbrk() declaration in glibc

#include "simple_malloc.h"
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

static void *heap_ptr = NULL; // Global heap pointer

#define WSIZE 4             // Word size
#define DSIZE 8             // Double word size
#define CHUNKSIZE (1 << 12) // Extend heap by this amount

/* Key idea:
 * Block sizes are aligned (to 8 or 16 bytes),
 * so the lower 3 or 4 least-significant bits of the size are always 0.
 * The allocation status uses one of these low bits:
 *   0 = free, 1 = allocated.
 */
#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

// Get size and allocation status
#define GET_SIZE(p) (GET(p) & ~0x7) // Block size
#define GET_ALLOC(p) (GET(p) & 0x1) // Allocation status

// Get address of header and footer
// bp: block pointer
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// Get address of next and previous blocks
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

// Todo: Implement freelist

/* Coalesce based on these conditions
 * Condition 1: Previous block is allocated and next block is allocated
 * Condition 2: Previous block is allocated and next block is free
 * Condition 3: Previous block is free and next block is allocated
 * Condition 4: Previous block is free and next block is free
 */
static void *coalesce(void *bp) {
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));

  if (prev_alloc && next_alloc) {
    return bp;
  } else if (prev_alloc && !next_alloc) {
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    return bp;
  } else if (!prev_alloc && next_alloc) {
    size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    return PREV_BLKP(bp);
  } else {
    size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
    return PREV_BLKP(bp);
  }
}

// Free
void mm_free(void *bp) {
  size_t size = GET_SIZE(HDRP(bp));
  PUT(HDRP(bp), PACK(size, 0));
  PUT(FTRP(bp), PACK(size, 0));
  coalesce(bp);
}

// Extend the heap
static void *extend_heap(size_t words) {
  size_t size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; // Alignment
  void *bp = sbrk(size);
  if (bp == (void *)-1) {
    return NULL;
  }

  PUT(HDRP(bp), PACK(size, 0));         // Free block header
  PUT(FTRP(bp), PACK(size, 0));         // Free block footer
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // New epilogue header

  return coalesce(bp);
}

// Heap initialization
int mm_init(void) {
  // Implement the epilogue (header and footer) block and also the prologue
  // block
  heap_ptr = sbrk(4 * WSIZE);
  if (heap_ptr == (void *)-1) {
    return -1;
  }

  PUT(heap_ptr, 0);                          // Alignment padding
  PUT(heap_ptr + WSIZE, PACK(DSIZE, 1));     // prologue header
  PUT(heap_ptr + 2 * WSIZE, PACK(DSIZE, 1)); // prologue footer
  PUT(heap_ptr + 3 * WSIZE, PACK(0, 1));     // epilogue header

  heap_ptr = heap_ptr + 2 * WSIZE; // Point to the payload

  void *extended_heap_ptr = extend_heap(CHUNKSIZE / WSIZE);
  if (extended_heap_ptr == NULL) {
    return -1;
  }

  return 0;
}

// Place the block: mark it as allocated and split if possible
static void place(void *bp, size_t requested_size) {
  size_t free_size = GET_SIZE(HDRP(bp));

  if (free_size - requested_size >= 2 * DSIZE) {
    // Split the block
    PUT(HDRP(bp), PACK(requested_size, 1));
    PUT(FTRP(bp), PACK(requested_size, 1));
    bp = NEXT_BLKP(bp);
    PUT(HDRP(bp), PACK(free_size - requested_size, 0));
    PUT(FTRP(bp), PACK(free_size - requested_size, 0));
  } else {
    // Mark the block as allocated
    PUT(HDRP(bp), PACK(free_size, 1));
    PUT(FTRP(bp), PACK(free_size, 1));
  }
}

// First fit strategy
static void *find_fit(size_t size) {
  void *bp;

  for (bp = heap_ptr; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
    if (!GET_ALLOC(HDRP(bp)) && (size <= GET_SIZE(HDRP(bp)))) {
      return bp;
    }
  }

  return NULL;
}

// Malloc
void *mm_malloc(size_t size) {
  size_t new_size;
  if (size == 0) {
    return NULL;
  }

  if (size <= DSIZE) {
    // Adjust size to account for header and footer and block alignment
    new_size = 2 * DSIZE;
  } else {
    // Add overhead
    new_size = DSIZE + size;
    // Round up to the nearest multiple of DSIZE
    new_size = (new_size + DSIZE - 1) / DSIZE; // Ceiling division
    new_size *= DSIZE;

    // Equivalent to this
    // new_size = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);
  }

  void *bp = find_fit(new_size);
  if (bp != NULL) {
    place(bp, new_size);
    return bp;
  }

  // Request more memory from the OS (extend by more if needed for large
  // allocations)
  size_t extend_words =
      (new_size > CHUNKSIZE) ? (new_size / WSIZE + 1) : (CHUNKSIZE / WSIZE);
  void *extended_heap_ptr = extend_heap(extend_words);
  if (extended_heap_ptr == NULL) {
    return NULL;
  }

  // Place the block
  place(extended_heap_ptr, new_size);
  return extended_heap_ptr;
}

int main() {
  mm_init();
  void *ptr = mm_malloc(48);

  // Use the allocated memory
  for (int i = 0; i < 48; i++) {
    ((char *)ptr)[i] = 'a' + (i % 26);
  }

  // Print the allocated memory
  for (int i = 0; i < 48; i++) {
    printf("%c", ((char *)ptr)[i]);
  }
  printf("\n");

  mm_free(ptr);
  return 0;
}