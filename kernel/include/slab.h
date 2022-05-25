#ifndef __SLAB_H__
#define __SLAB_H__
#define SLAB_INIT_SIZE (2 * 1024 * 1024)  // 2M

/* order range: [SLAB_MIN_ORDER, SLAB_MAX_ORDER] */
#define SLAB_MIN_ORDER (5)
#define SLAB_MAX_ORDER (11)
#define SLAB_SIZE      (1UL << SLAB_MAX_ORDER)
typedef unsigned long long u64;
typedef struct slab_header slab_header_t;
struct slab_header {
  void *free_list_head;
  slab_header_t *next_slab;
  int order;
};

typedef struct slab_slot_list slab_slot_list_t;
struct slab_slot_list {
  void *next_free;
};

int allocinslab;

void slab_init(void);

void *alloc_in_slab(u64);
void free_in_slab(void *addr);
#endif