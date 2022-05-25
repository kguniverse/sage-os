#include <common.h>
#include <buddy.h>
#include <logger.h>
#include <slab.h>
#include <thread.h>

static void* kalloc(size_t size) {
  assert((int)size > 0);
  if (size <= SLAB_SIZE) {
    // printf("small:alloc_in_slab\n");
    return alloc_in_slab(size);
  }
  int npage               = (size - 1) / SZ_PAGE + 1;
  int acquire_order       = power2ify(npage);
  struct chunk* page_addr = chunk_alloc(&global_mm_pool, acquire_order);
  if (page_addr != NULL) {
    success("allocate addr: 0x%x", chunk2virt(&global_mm_pool, page_addr));
    global_mm_pool.occupied += npage * SZ_PAGE;
    return chunk2virt(&global_mm_pool, page_addr);
  }
  warn("fail to alloc addr");
  return NULL;
}

static void* pgalloc() {
  return kalloc(SZ_PAGE);
}

static void* kalloc_safe(size_t size) {
  bool i = ienabled();
  iset(false);
  void* ret = kalloc(size);
  if (i) iset(true);
  return ret;
}

static void kfree(void* ptr) {
  struct chunk* chunk = virt2chunk(&global_mm_pool, ptr);
  if (chunk && chunk->slab) {
    // printf("small:free in slab\n");
    free_in_slab(ptr);
  } else {
    global_mm_pool.occupied -= (1 << chunk->order) * SZ_PAGE;
    chunk_free(&global_mm_pool, chunk);
    success("free successfully, address: 0x%x", ptr);
  }
}

static void kfree_safe(void* ptr) {
  int i = ienabled();
  iset(false);
  kfree(ptr);
  if (i) iset(true);
}

static void pmm_init() {
  uintptr_t pmsize = ((uintptr_t)heap.end - (uintptr_t)heap.start);
  info("Got %d MiB heap: [%p, %p)\n", pmsize >> 20, heap.start, heap.end);

  void* pg_start = NULL;
  void* pi_start = NULL;
  allocinslab    = 0;
  int nr_page;
  nr_page =
      (uintptr_t)(heap.end - heap.start) / (SZ_PAGE + sizeof(struct chunk));
  pg_start = heap.start;
  pi_start = (bool*)(pg_start + nr_page * SZ_PAGE);
  // global_mm_pool = pi_start;
  // pi_start       = (struct pmm_pool*)pi_start + 1;
  info("page start addr: 0x%x, page indicator addr: 0x%x, available page: %d",
       pg_start, pi_start, nr_page);
  buddy_init(&global_mm_pool, pi_start, pg_start, nr_page);
  slab_init();
}

MODULE_DEF(pmm) = {
    .init    = pmm_init,
    .alloc   = kalloc_safe,
    .free    = kfree_safe,
    .pgalloc = pgalloc,
};
