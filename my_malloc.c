#include "my_malloc.h"

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
 *  Allocate the first available block able to satisfy the request
 *  (starting the search at the next free header after the header that was most
 *  recently allocated)
 */

static header *next_fit(size_t size) {
  header* current_block = g_next_allocate;
  if (current_block == NULL) {
    current_block = g_freelist_head;
    if (current_block == NULL) {
      return NULL;
    }
  }

  header * starting_block = current_block;

  do {
    if (TRUE_SIZE(current_block) >= size) {
      return current_block;
    }

    /* Iterate to the next block */

    current_block = current_block->next;
    if (current_block == NULL) {
      current_block = g_freelist_head;
    }

  } while (current_block != starting_block);
  return NULL;
} /* next_fit() */

/*
 * Allocate the FIRST instance of the smallest block able to satisfy the
 * request
 */

static header *best_fit(size_t size) {
  header *best_fit = g_freelist_head;
  header *current_block = g_freelist_head;
  while (current_block != NULL) {
    size_t curr_size = TRUE_SIZE(current_block);
    if ( curr_size >= size ) {
      if (curr_size < best_fit->size) {
        best_fit = current_block;
      }
    }
    current_block = current_block->next;
  }
  if (best_fit->size >= size) {
    return best_fit;
  }
  return NULL;
} /* best_fit() */

/*
 * This function finds the worst block to put the requested chunk of memory
 * in.
 */

static header *worst_fit(size_t size) {
  header *worst_fit = g_freelist_head;
  header *current_block = g_freelist_head;
  while (current_block != NULL) {
    size_t curr_size = TRUE_SIZE(current_block);
    if ( curr_size >= size ) {
      if (curr_size >= worst_fit->size) {
        worst_fit = current_block;
      }
    }
    current_block = current_block->next;
  }

  if (worst_fit->size >= size) {
    return worst_fit;
  }
  return NULL;
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

  if (h != g_freelist_head) {
    if (g_freelist_head != NULL) {
      g_freelist_head->prev = h;
    }

    h->next = g_freelist_head;
    g_freelist_head = h;
  }

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
} /* init() */

/*
 * This function is used to determined if a header is unallocated or not.
 *
 * head: The header that is being checked;
 *
 * return: 0 is passed header is allocated, 1 if unallocated
 */

static size_t isUnallocated(header * head) {
  if (head == NULL) {
    return 0;
  }

  if (TRUE_SIZE(head) == head->size) {
    return 1;
  }
  return 0;
} /* isUnallocated() */

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

  /* Set the next_allocate block for the next_fit function */

  g_next_allocate = head->next;

  /* If the size of the found_header is a perfect match or the remaining
   * memory after splitting is too small */

  if ((head->size == needed_size) ||
      ((TRUE_SIZE(head) - needed_size - ALLOC_HEADER_SIZE) <=
        ALLOC_HEADER_SIZE + sizeof(header *) * 2)) {

    /* Remove head from the Free List */

    if (head != g_freelist_head) {
      head->prev->next = head->next;
      if (head->next != NULL) {
        head->next->prev = head->prev;
      }
    }
    else {
      g_freelist_head = head->next;
      if (g_freelist_head != NULL) {
        g_freelist_head->prev = NULL;
      }
    }

    head->next = NULL;
    head->prev = NULL;

    return head;
  }

  /* Split the header */

  header* new_header = (header *) (((char *) head) +
      ALLOC_HEADER_SIZE + needed_size);
  new_header->size = TRUE_SIZE(head) - needed_size - ALLOC_HEADER_SIZE;

  if (right_neighbor(head) != NULL) {
    right_neighbor(head)->left_size = needed_size;
  }

  if (right_neighbor(new_header) != NULL) {
    right_neighbor(new_header)->left_size = new_header->size;
  }

  if ((head != g_freelist_head) && (head->prev != NULL)) {
    head->prev->next = head->next;
  }
  else {
    g_freelist_head = head->next;
  }

  if ((head->next != NULL) && (head->next != g_freelist_head)) {
    head->next->prev = head->prev;
  }

  head->size = needed_size;
  new_header->left_size = needed_size;
  insert_free_block(new_header);

  head->next = NULL;
  head->prev = NULL;
  head->size = needed_size;

  return new_header;
} /* split_header() */

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

  /* Request more memory from the OS */

  size_t size = ARENA_SIZE;

  while (size < needed_mem_size) {
    size += ARENA_SIZE;
  }

  void* location = sbrk(size);

  /* Ensures that more mem was created */

  if (location == ((void *) -1)) {
    errno = ENOMEM;
    return NULL;
  }

  /* Set the fenceposts in the new chunk of mem */

  set_fenceposts(location, size);

  /* Coalesce if needed */

  if (g_base != location) {

    /* If statement ensures that there has been more than one call to
     * sbrk() so there should be multiple set of fenceposts. */

    header* possible_fencepost = location - ALLOC_HEADER_SIZE;

    if (possible_fencepost == g_last_fence_post) {
      header* left_header = left_neighbor(g_last_fence_post);

      left_header->size = left_header->size + size;
      g_last_fence_post = location + size - ALLOC_HEADER_SIZE;
      return left_header;
    }
  }

  /* Set the g_last_fencepost variable the most recent right fencepost */

  g_last_fence_post = location + size - ALLOC_HEADER_SIZE;

  /* Initialize the header in the new chunk */

  header* head = location + ALLOC_HEADER_SIZE;
  head->size = size - 3 * ((size_t) ALLOC_HEADER_SIZE);
  head->left_size = 0;
  head->next = NULL;
  head->prev = NULL;
  head->data = NULL;
  return head;
} /* get_more_mem() */

/*
 * This is my version of malloc().
 *
 * Allocates the requested memory to the user.
 */

void *my_malloc(size_t requested_size) {
  pthread_mutex_lock(&g_mutex);

  /* Make sure that NULL is returned when allocating no mem. */

  if (requested_size == 0) {
    pthread_mutex_unlock(&g_mutex);
    return NULL;
  }

  /* Ensure that the requested size is a multiple of MIN_ALLOCATION */

  requested_size = roundup(requested_size, MIN_ALLOCATION);

  /* Ensure that there is enough space for next/prev pointers when this
   * header is freed */

  requested_size = requested_size + ALLOC_HEADER_SIZE < sizeof(header) ?
    sizeof(header) - ALLOC_HEADER_SIZE: requested_size;

  size_t needed_size = requested_size + ALLOC_HEADER_SIZE;
  needed_size = roundup(needed_size, MIN_ALLOCATION);

  /* Ensures that the amount of memory being allocated has enough room
   * for two fenceposts and a header */

  needed_size = requested_size + 3 * ALLOC_HEADER_SIZE > ARENA_SIZE ?
    requested_size + 3 * ALLOC_HEADER_SIZE : needed_size;

  if ( g_freelist_head == NULL) {
    header* newly_allocated_head = get_more_mem(needed_size);

    if (newly_allocated_head == NULL) {
      return NULL;
    }

    g_freelist_head = newly_allocated_head;
  }

  /* Look for a header with the proper contraints */

  header* found_header = find_header(requested_size);
  if (!found_header) {
    header* new_chunk = get_more_mem(needed_size);
    insert_free_block(new_chunk);
    if (new_chunk == NULL) {
      return NULL;
    }

    found_header = find_header(needed_size);
  }

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

  if (p == NULL) {
    pthread_mutex_unlock(&g_mutex);
    return;
  }

  header *head = (header *) (((char *) p) - ALLOC_HEADER_SIZE);

  /* Ensures that the block is not unallocated */

  if (isUnallocated(head)) {
    pthread_mutex_unlock(&g_mutex);
    assert(false);
    exit(1);
  }

  /* Change the state to Unallocated */

  head->size = TRUE_SIZE(head);

  if (isUnallocated(left_neighbor(head)) &&
      isUnallocated(right_neighbor(head))) {

    /* Coalesce with left and right neighbors */

    if (right_neighbor(head) == g_next_allocate) {
      g_next_allocate = left_neighbor(head);
    }

    size_t new_size = TRUE_SIZE(left_neighbor(head)) + TRUE_SIZE(head) +
      TRUE_SIZE(right_neighbor(head)) + ALLOC_HEADER_SIZE * 2;

    /* This tests to see where to put the coalesced block in the free list */

    if (left_neighbor(head)->next == right_neighbor(head)) {
      left_neighbor(head)->next = right_neighbor(head)->next;
      if (left_neighbor(head)->next != NULL) {
        left_neighbor(head)->next->left_size = new_size;
        left_neighbor(head)->next->prev = left_neighbor(head);
      }
    }
    else {
      if (right_neighbor(head)->prev != NULL) {
        right_neighbor(head)->prev->next = left_neighbor(head);
      }
      left_neighbor(head)->prev = right_neighbor(head)->prev;
    }

    left_neighbor(head)->size = new_size;
    right_neighbor(head)->next = NULL;
    right_neighbor(head)->prev = NULL;
  }
  else if (isUnallocated(left_neighbor(head))) {

    /* Coalesce with just the left neighbor  */

    left_neighbor(head)->size = TRUE_SIZE(left_neighbor(head)) +
      TRUE_SIZE(head) + ALLOC_HEADER_SIZE;
    right_neighbor(head)->left_size = left_neighbor(head)->size;
  }
  else if (isUnallocated(right_neighbor(head))) {

    /* Coalesce with the right neighbor  */

    if (right_neighbor(head) == g_next_allocate) {
      g_next_allocate = head;
    }

    head->next = right_neighbor(head)->next;
    head->prev = right_neighbor(head)->prev;

    if (right_neighbor(head)->next != NULL) {
      right_neighbor(head)->next->left_size = head->size;
      right_neighbor(head)->next->prev = head;
    }

    if (right_neighbor(head) == g_freelist_head) {
      g_freelist_head = head;
    }
    else if (head->prev != NULL) {
      head->prev->next = head;
    }

    head->size = head->size + ALLOC_HEADER_SIZE +
      TRUE_SIZE(right_neighbor(head));
  }
  else {

    /* Neither neighbor is unallocated, so just add to free list */

    insert_free_block(head);
  }
  pthread_mutex_unlock(&g_mutex);
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
