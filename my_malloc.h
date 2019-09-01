#include <sys/types.h>

#ifndef MY_MALLOC_H
#define MY_MALLOC_H

#ifndef MIN_ALLOCATION
#define MIN_ALLOCATION (8)
#endif

#ifndef ARENA_SIZE
#define ARENA_SIZE (4096)
#endif

/*
 * Defines the algorithm that will
 * be used to select the free block.
 *
 * 1 = First Fit
 * 2 = Next Fit
 * 3 = Best Fit
 * 4 = Worst Fit
 */
#ifndef FIT_ALGORITHM
#define FIT_ALGORITHM (1)
#endif

#define ALLOC_HEADER_SIZE (sizeof(header) - (2 * sizeof(header *)))

#define TRUE_SIZE(x) ((x->size) & ~0b111)

/* The 3 least significant bits in
 * the block size field are used
 * to store allocation state.
 */

typedef enum state {
  UNALLOCATED = 0b000,
  ALLOCATED = 0b001,
  FENCEPOST = 0b010,
} state;

typedef struct header {

  /* Size variable based upon assumption
   * that header takes ALLOC_HEADER_SIZE
   */

  size_t size;
  size_t left_size;
  union {
    struct {
      struct header *next;
      struct header *prev;
    };
    char *data;
  };
} header;

/* Variations of the malloc function.
 * You only need to implement malloc and free.
 */

void *my_malloc(size_t size);
void *my_calloc(size_t nmemb, size_t size);
void *my_realloc(void *ptr, size_t size);
void my_free(void *p);

/*
 * Block finding algorithm functions.
 */

static header *first_fit(size_t size);
static header *next_fit(size_t size);
static header *best_fit(size_t size);
static header *worst_fit(size_t size);
static header *find_header(size_t size);


/*
 * Helper functions to find neighboring headers
 */

static inline header *left_neighbor(header *h);
static void set_fenceposts(void *mem, size_t size);

/*
 * Helper functions for allocating a block
 */

static void insert_free_block(header *h);
static void set_fenceposts(void *mem, size_t size);

/*
 * Library initialization
 */

static void init();

/*
 * Global variable declarations
 */

extern header *g_freelist_head;
extern header *g_last_fencepost;
extern header * g_next_allocate;
extern void *g_base;

#endif
