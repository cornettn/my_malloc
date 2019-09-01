#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "my_malloc.h"

/* Pointer to the location of the heap prior to any sbrk calls */
void *g_base;

/* Pointer to the head of the free list */
header *g_freelist_head;

/* Mutex to ensure thread safety for the freelist */
static pthread_mutex_t g_mutex;

/*
 * Pointer to the second fencepost in the most recently allocated chunk from
 * the OS. Used for coalescing chunks
 */

header *g_last_fence_post;

/*
 * Pointer to the next block in the freelist after the block that was last
 * allocated. If the block pointed to is removed by coalescing, this shouuld be
 * updated to point to the next block after the removed block.
 */
header *g_next_allocate;

/*
 * Direct the compiler to run the init function before running main
 * this allows initialization of required globals
 */

static void init (void) __attribute__((constructor));

/*
 * TODO: implement first_fit
 * Allocate the first available block able to satisfy the request
 * (starting the search at g_freelist_head)
 */

static header *first_fit(size_t size) {
  (void) size;
  assert(false);
  exit(1);
}

/*
 *  TODO: implement next_fit
 *  Allocate the first available block able to satisfy the request
 *  (starting the search at the next free header after the header that was most
 *  recently allocated)
 */

static header *next_fit(size_t size) {
  (void) size;
  assert(false);
  exit(1);

}

/*
 * TODO: implement best_fit
 * Allocate the FIRST instance of the smallest block able to satisfy the 
 * request
 */

static header *best_fit(size_t size) {
  (void) size;
  assert(false);
  exit(1);
}

/*
 * TODO: implement worst_fit
 */

static header *worst_fit(size_t size) {
  (void) size;
  assert(false);
  exit(1);
}

/*
 * Returns the address of the block to allocate 
 * based on the specified algorithm.
 *
 * If no block is available, returns NULL.
 */

static header *find_header(size_t size) {
  if (g_freelist_head == NULL) {
    return NULL;
  }

  switch (FIT_ALGORITHM) {
    case 1:
      return first_fit(size);
      
    case 2:
      return next_fit(size);

    case 3:
      return best_fit(size);

    case 4:
      return worst_fit(size);
  }
  assert(false);
}

/*
 * Calculates the location of the left neighbor given a header.
 */

static inline header *left_neighbor(header *h) {
  return (header *) (((char *) h) - h->left_size - ALLOC_HEADER_SIZE);
}

/* 
 * Calculates the location of the right neighbor given a header.
 */

static inline header *right_neighbor(header *h) {
  return (header *) (((char *) h) + ALLOC_HEADER_SIZE + TRUE_SIZE(h));
}

/*
 * Insert a block at the beginning of the freelist.
 * The block is located after its left header, h.
 */

static void insert_free_block(header *h) {
  h->prev = NULL;

  if (g_freelist_head != NULL) {
    g_freelist_head->prev = h;
  }

  h->next = g_freelist_head;
  g_freelist_head = h;
}

/*
 * Instantiates fenceposts at the left and right side of a block.
 */

static void set_fenceposts(void *mem, size_t size) {
  header *left_fence = (header *) mem;
  header *right_fence = (header *) (((char *) mem) + 
                         (size - ALLOC_HEADER_SIZE));

  left_fence->size = (state) FENCEPOST;
  right_fence->size = (state) FENCEPOST;

  right_fence->left_size = size - 3 * ALLOC_HEADER_SIZE;
}

/*
 * Constructor that runs before main() to initialize the library.
 */

__attribute__((constructor))
void init() {
  g_freelist_head = NULL;

  /* Initialize mutex for thread safety */

  pthread_mutex_init(&g_mutex, NULL);

  /* Manually set printf buffer so it won't call malloc */

  setvbuf(stdout, NULL, _IONBF, 0);

  /* Record the starting address of the heap */

  g_base = sbrk(0);
}

/*
 * TODO: implement malloc
 */

void * my_malloc(size_t size) {
  pthread_mutex_lock(&g_mutex);
  // Insert code here
  pthread_mutex_unlock(&g_mutex);

  // Remove this code
  (void) size;
  assert(false);
  exit(1);
}

/*
 * TODO: implement free
 */

void my_free(void * p) {
  pthread_mutex_lock(&g_mutex);  
  // Insert code here
  pthread_mutex_unlock(&g_mutex);

  // Remove this code
  (void) p;
  assert(false);
  exit(1);
}

/*
 * Calls malloc and sets each byte of 
 * the allocated memory to a value.
 */

void *my_calloc(size_t nmemb, size_t size) {
  return memset(my_malloc(size * nmemb), 0, size * nmemb);
}

/*
 * Reallocates an allocated block to a new size and
 * copies the contents to the new block.
 */

void *my_realloc(void *ptr, size_t size) {
  void *mem = my_malloc(size);
  memcpy(mem, ptr, size);
  my_free(ptr);
  return mem; 
}
