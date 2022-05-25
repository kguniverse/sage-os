#include "slab.h"
#include "buddy.h"

/* local variables */
slab_header_t *slabs[SLAB_MAX_ORDER + 1];

/* local functions */
static inline u64 size_to_order(u64 size) {
  u64 order = 0;
  int tmp   = size;

  while (tmp > 1) {
    tmp >>= 1;
    order += 1;
  }
  if (size > (1 << order)) order += 1;

  return order;
}

static inline u64 order_to_size(u64 order) {
  return 1UL << order;
}

static void *alloc_slab_memory(uint64_t size)  // ok
{
  struct chunk *p_chunk, *chunk;
  void *addr;
  uint64_t order, chunk_num;
  void *chunk_addr;
  int i;

  order   = size_to_order(size / SZ_PAGE);
  p_chunk = chunk_alloc(&global_mm_pool, order);
  // global_mm_pool.occupied += order_to_size(order) * SZ_PAGE;
  // printf("slab_alloc:\nsize:%d\noccupied:%d\norder to
  // size:%d\norder:%d\n",size,global_mm_pool.occupied,order_to_size(order),order);
  if (p_chunk == NULL) {
    // kwarn("failed to alloc_slab_memory: out of memory\n");
    // BUG_ON(1);
  }
  addr = chunk2virt(&global_mm_pool, p_chunk);

  chunk_num = order_to_size(order);
  for (i = 0; i < chunk_num; i++) {
    chunk_addr  = (void *)((uint64_t)addr + i * SZ_PAGE);
    chunk       = virt2chunk(&global_mm_pool, chunk_addr);
    chunk->slab = addr;
  }
  // printf("successfully alloc slab memory:%llu\n",size);
  return addr;
}

static slab_header_t *init_slab_cache(int order, int size)  // ok
{
  void *addr;
  slab_slot_list_t *slot;
  slab_header_t *slab;
  u64 cnt, obj_size;
  int i;

  addr = alloc_slab_memory(size);
  slab = (slab_header_t *)addr;

  obj_size = order_to_size(order);
  /* the first slot is used as metadata */
  cnt = size / obj_size - 1;

  slot                 = (slab_slot_list_t *)(addr + obj_size);
  slab->free_list_head = (void *)slot;
  slab->next_slab      = NULL;
  slab->order          = order;

  /* the last slot has no next one */
  for (i = 0; i < cnt - 1; i++) {
    slot->next_free = (void *)((u64)slot + obj_size);
    slot            = (slab_slot_list_t *)((u64)slot + obj_size);
  }
  slot->next_free = NULL;

  // printf("successfully init slab cache:slabs[%d]\n",order);

  return slab;
}

static void *_alloc_in_slab_nolock(slab_header_t *slab_header, int order) {
  slab_slot_list_t *first_slot;
  void *next_slot;
  slab_header_t *next_slab;
  slab_header_t *new_slab;

  first_slot = (slab_slot_list_t *)(slab_header->free_list_head);
  if (first_slot != NULL) {
    next_slot                   = first_slot->next_free;
    slab_header->free_list_head = next_slot;
    // printf("successfully alloc in slab:%llu\n",order_to_size(order));
    allocinslab += (int)order_to_size(order);
    return first_slot;
  }
  // first_slot == NULL
  next_slab = slab_header->next_slab;
  while (next_slab != NULL) {
    first_slot = (slab_slot_list_t *)(next_slab->free_list_head);
    if (first_slot != NULL) {
      next_slot                 = first_slot->next_free;
      next_slab->free_list_head = next_slot;
      // printf("successfully alloc in slab:%llu\n",order_to_size(order));
      allocinslab += (int)order_to_size(order);
      return first_slot;
    }
    next_slab = next_slab->next_slab;
  }

  // alloc new slab
  new_slab            = init_slab_cache(order, SLAB_INIT_SIZE);
  new_slab->next_slab = slab_header;
  slabs[order]        = new_slab;

  return _alloc_in_slab_nolock(new_slab, order);
}

static void *_alloc_in_slab(slab_header_t *slab_header, int order) {
  void *free_slot;

  free_slot = _alloc_in_slab_nolock(slab_header, order);

  // printf("successfully alloc in slab:%p\n",free_slot);
  return free_slot;
}

void slab_init()  // ok
{
  int order;

  /* slab obj size: 32, 64, 128, 256, 512, 1024, 2048 */
  for (order = SLAB_MIN_ORDER; order <= SLAB_MAX_ORDER; order++) {
    slabs[order] = init_slab_cache(order, SLAB_INIT_SIZE);
  }
  // printf("successfully init slab\n");
  // kdebug("mm: finish initing slab allocators\n");
}

void *alloc_in_slab(u64 size)  // ok
{
  int order;

  // BUG_ON(size > order_to_size(SLAB_MAX_ORDER));

  order = (int)size_to_order(size);
  if (order < SLAB_MIN_ORDER) order = SLAB_MIN_ORDER;

  // printf("begin alloc in slab:%llu\n",size);
  return _alloc_in_slab(slabs[order], order);
}

void free_in_slab(void *addr)  // ok
{
  struct chunk *chunk;
  slab_header_t *slab;
  slab_slot_list_t *slot;

  slot  = (slab_slot_list_t *)addr;
  chunk = virt2chunk(&global_mm_pool, addr);
  // BUG_ON(page == NULL);

  slab                 = chunk->slab;
  slot->next_free      = slab->free_list_head;
  slab->free_list_head = slot;
  // printf("successfullt free in slab:%p\n",addr);
}