#include "my_malloc.h"
#include "printing.h"

#include <pthread.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
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
  header* current_block = g_freelist_head;
  while (current_block != NULL) {
    if (TRUE_SIZE(current_block) >= size) {
      return current_block;
    }
    current_block = current_block->next;
  }
  return NULL;
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
  printf("left_fence: %p\nright_fence: %p\n", left_fence, right_fence);
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
} /* init() */


/*
 * This function will take a header with an appropirate amount of space
 * and split it to fit exactly that amount.
 * 
 * head: The header that is to be split into a smaller size.
 * needed_size: The size that the header is going to be split to. 
 *
 * return: A pointer to the split header with the correct size;
 */

header* split_header(header* head, size_t needed_size) {
  header* new_header = head;
  char* new_head = (char *) new_header;
  new_head += ALLOC_HEADER_SIZE + needed_size;
  new_header = (header *) new_head;
  new_header->size = TRUE_SIZE(head) - needed_size - ALLOC_HEADER_SIZE;
 
  new_header->prev = head->prev;
  if (new_header->prev != NULL) {
    new_header->prev->next = new_header;
  }

  new_header->next = head->next;
  if (new_header->next != NULL) {
    new_header->next->prev = new_header;
    new_header->next->left_size = TRUE_SIZE(new_header);
 
  }
  new_header->left_size = head->left_size;

  if (head == g_freelist_head) {
    g_freelist_head = new_header;
  }

  head->next = NULL;
  head->prev = NULL;
  head->size = needed_size;

  print_object(head);
  print_object(new_header);


  return head;
} /* split_header()  */

/*
 * This function will round up a number to nearest multiple of another
 * number.
 *
 * num_to_round: This is the number that will be rounded up.
 * multiple: This is the multiple that the num_to_round will be rounded up to.
 *
 * return: The rounded up number.
 */

size_t roundup(size_t num_to_round, size_t multiple) {
  if (num_to_round < multiple) {
    num_to_round = multiple;
  }
  size_t difference = num_to_round % multiple;
  if (difference != 0) {
    num_to_round += (multiple - difference);
  }
  return num_to_round;
} /* roundup() */

/*
 * This function is responsible for getting more space from the OS whenever
 * necessary.
 *
 * needed_mem_size: This is the amount of memory that is required from the
 *   my_malloc() call.
 *
 * return: A pointer to the header of the chunk of memory received.
 */

header* get_more_mem(size_t needed_mem_size) {
  
  /*
   * TODO: Add a check to see if chunks can be coallesed. If they can,
   * make sure that the fenceposts are updated accordingly.
   */

  /* Request more memory from the OS */ 
  
  size_t size = ARENA_SIZE;
  
  while (size < needed_mem_size) {
    size += ARENA_SIZE;
  }
  
  g_base = sbrk(size);
  
  /* Ensures that more mem was created */

  if (g_base == ((void *) -1)) {
    errno = ENOMEM;
    return NULL;
  }

  /* Set the fenceposts in the new chunk of mem */
  
  set_fenceposts(g_base, size);
  
  /* Initialize the header in the new chunk */
  
  header* head = g_base + ALLOC_HEADER_SIZE;
  head->size = size - 3 * ((size_t) ALLOC_HEADER_SIZE);
  head->left_size = 0;
  head->next = NULL;
  head->prev = NULL;
  head->data = NULL;
  return head;
} /* get_more_mem() */

/*
 * TODO: implement malloc
 */

void *my_malloc(size_t requested_size) {
  pthread_mutex_lock(&g_mutex);
  
  size_t needed_size = requested_size + ALLOC_HEADER_SIZE;

  if (requested_size == 0) {
    pthread_mutex_unlock(&g_mutex);
    return NULL;
  }

  /* Ensure that the size allocated is a multiple of MIN_ALLOCATION */
  
  needed_size = roundup(needed_size, MIN_ALLOCATION);


  /* Ensure that there is enough space for next/prev pointers when this
   * header is freed */

  needed_size = requested_size + ALLOC_HEADER_SIZE < sizeof(header) ?
    sizeof(header) - ALLOC_HEADER_SIZE: needed_size;

  if ( g_freelist_head == NULL) {
    needed_size = requested_size + 3 * ALLOC_HEADER_SIZE > ARENA_SIZE ?
      requested_size + 3 * ALLOC_HEADER_SIZE : needed_size;  

    header* newly_allocated_head = get_more_mem(needed_size);

    if (newly_allocated_head == NULL) {
      return NULL;
    }

    /* Create the head of the free list in the chunk of space received from 
     * the OS. 
     *
     * The g_freelist_head can assigned to the newly allocaed memory because
     * this is within the section of the code where there is no free list.
     */

    g_freelist_head = newly_allocated_head;
  }
  

  /* Look for a header with the proper contraints */
 
  header* found_header = find_header(requested_size);
  if (!found_header) {
    needed_size = requested_size + 3 * ALLOC_HEADER_SIZE > ARENA_SIZE ?
      requested_size + 3 * ALLOC_HEADER_SIZE : needed_size;  
    
    header* new_chunk = get_more_mem(needed_size);
    insert_free_block(new_chunk);
    if (new_chunk == NULL) {
      return NULL;
    }

    found_header = find_header(requested_size);
  }

  assert(found_header);

  split_header(found_header, requested_size);

  /* Change the state of the found header to ALOOCATED */

  found_header->size = found_header->size | (state) ALLOCATED;

  pthread_mutex_unlock(&g_mutex);
  return &found_header->data;
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
