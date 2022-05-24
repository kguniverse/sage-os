#ifndef __BUDDY_H__
#define __BUDDY_H__
#include <common.h>
#include <list.h>
#include <thread.h>

#define BUDDY_MAX_ORDER 24

struct pmm_pool global_mm_pool;

/**
 * @brief page metadata
 */
struct page {
  void* start_addr;
  struct chunk* _chunk;
  // struct slab*
};

/**
 * @brief free list api, including counter of free page.
 *
 */
struct free_list {
  int64_t nr_free;
  struct list_head free_list;
};

/**
 * @brief chunk structure in buddy system
 */
struct chunk {
  struct list_head node;
  uint8_t order;
  bool used;
  void* slab;
};

/**
 * @brief memory manage pool
 *
 */
struct pmm_pool {
  uintptr_t begin_addr;
  uint64_t page_num;
  uint64_t size;
  uint64_t occupied;
  struct chunk* chunk_metadata;
  struct free_list free_lists[BUDDY_MAX_ORDER];
};

int total_mem() {
  return global_mm_pool.occupied;
}

int available_mem() {
  return global_mm_pool.size - global_mm_pool.occupied;
}
int mem_sz(int pid) {
  return mem_size[pid];
}

struct pmm_pool global_mm_pool;

void* chunk2virt(struct pmm_pool* mm_pool, struct chunk* chunk);

struct chunk* virt2chunk(struct pmm_pool* mm_pool, void* virt);

struct chunk* chunk_merge(struct pmm_pool* mm_pool, struct chunk* chunk);

void chunk_append(struct pmm_pool* mm_pool, struct chunk* chunk);

void chunk_free(struct pmm_pool* mm_pool, struct chunk* chunk);

void buddy_init(struct pmm_pool* mm_pool, struct chunk* start_chunk,
                void* start_addr, uint64_t page_num);

struct chunk* get_buddy_chunk(struct pmm_pool* mm_pool, struct chunk* chunk);

void chunk_del(struct pmm_pool* mm_pool, struct chunk* chunk);

struct chunk* chunk_split(struct pmm_pool* mm_pool, uint8_t order,
                          struct chunk* chunk);

struct chunk* chunk_alloc(struct pmm_pool* mm_pool, uint8_t order);

#endif