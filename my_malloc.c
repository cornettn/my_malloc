#include "my_malloc.h"

#include <pthread.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* Pointer to the location of the heap prior to any sbrk calls */
void *g_base = NULL;

/* Pointer to the head of the free list */
header *g_freelist_head = NULL;

/* Mutex to ensure thread safety for the freelist */
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Pointer to the second fencepost in the most recently allocated chunk from
 * the OS. Used for coalescing chunks
 */

header *g_last_fence_post = NULL;

/*
 * Pointer to the next block in the freelist after the block that was last
 * allocated. If the block pointed to is removed by coalescing, this shouuld be
 * updated to point to the next block after the removed block.
 */
header *g_next_allocate = NULL;

/*
 * Direct the compiler to run the init function before running main
 * this allows initialization of required globals
 */

static void init(void) __attribute__((constructor));

/*
 * Direct the compiler to ignore unused static functions.
 * TODO: Remove these in your code.
 */
static void set_fenceposts(void *mem, size_t size) __attribute__((unused));
static void insert_free_block(header *h) __attribute__((unused));
static header *find_header(size_t size) __attribute__((unused));

/*
 * TODO: implement first_fit
 * Allocate the first available block able to satisfy the request
 * (starting the search at g_freelist_head)
 */

static header *first_fit(size_t size) {
  (void) size;
  assert(false);
  exit(1);
} /* first_fit() */

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
} /* next_fit() */

/*
 * TODO: implement best_fit
 * Allocate the FIRST instance of the smallest block able to satisfy the
 * request
 */

static header *best_fit(size_t size) {
  (void) size;
  assert(false);
  exit(1);
} /* best_fit() */

/*
 * TODO: implement worst_fit
 */

static header *worst_fit(size_t size) {
  (void) size;
  assert(false);
  exit(1);
} /* worst_fit() */

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
} /* find_header() */

/*
 * Calculates the location of the left neighbor given a header.
 */

static inline header *left_neighbor(header *h) {
  return (header *) (((char *) h) - h->left_size - ALLOC_HEADER_SIZE);
} /* left_neighbor() */

/*
 * Calculates the location of the right neighbor given a header.
 */

static inline header *right_neighbor(header *h) {
  return (header *) (((char *) h) + ALLOC_HEADER_SIZE + TRUE_SIZE(h));
} /* right_neighbor() */

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
} /* insert_free_block() */

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
} /* set_fenceposts() */

/*
 * Constructor that runs before main() to initialize the library.
 */

static void init() {
  g_freelist_head = NULL;

  /* Initialize mutex for thread safety */

  pthread_mutex_init(&g_mutex, NULL);

  /* Manually set printf buffer so it won't call malloc */

  setvbuf(stdout, NULL, _IONBF, 0);

  /* Record the starting address of the heap */

  g_base = sbrk(0);
  printf("Initial Value of g_base: %p\n", g_base);
  printf("Initial Address of g_base: %p\n", &g_base);
  
} /* init() */

/*
 * This function is responsible for getting more space from the OS whenever
 * necessary.
 */

int get_more_mem() {
  printf("Getting more space: %d more bytes.\n", ARENA_SIZE);
  g_base = sbrk(ARENA_SIZE);
  return ARENA_SIZE;
} /* get_more_mem() */

/*
 * TODO: implement malloc
 */

void *my_malloc(size_t size) {
  pthread_mutex_lock(&g_mutex);
  if ( g_freelist_head == NULL) {  
    int amount_allocated = get_more_mem();
  
    /* Create the head of the free list in the chunk of space received from 
     * the OS  */
 
    g_freelist_head = g_base;
    g_freelist_head->size = amount_allocated - sizeof(header*);
    g_freelist_head->left_size = 0;
    g_freelist_head->next = NULL;
    g_freelist_head->prev = NULL;
    printf("g_freelist_head address: %p\n", g_freelist_head);
  }

  printf("malloc\n");
  pthread_mutex_unlock(&g_mutex);

/*
  // Remove this code
  (void) size;
  assert(false);
  exit(1);
*/

  return NULL;
} /* my_malloc() */

/*
 * TODO: implement free
 */

void my_free(void *p) {
  pthread_mutex_lock(&g_mutex);
  // Insert code here
  pthread_mutex_unlock(&g_mutex);

  // Remove this code
  (void) p;
  assert(false);
  exit(1);
} /* my_free() */

/*
 * Calls malloc and sets each byte of
 * the allocated memory to a value.
 */

void *my_calloc(size_t nmemb, size_t size) {
  return memset(my_malloc(size * nmemb), 0, size * nmemb);
} /* my_calloc() */

/*
 * Reallocates an allocated block to a new size and
 * copies the contents to the new block.
 */

void *my_realloc(void *ptr, size_t size) {
  void *mem = my_malloc(size);
  memcpy(mem, ptr, size);
  my_free(ptr);
  return mem;
} /* my_realloc() */
