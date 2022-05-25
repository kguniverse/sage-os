#include <common.h>
#include <trap.h>
#include <logger.h>
#include <buddy.h>
#include <slab.h>

void* alloc_addr[16];

int total_mem() {
  return global_mm_pool.occupied + allocinslab;
}

// static int available_mem() {
//   return global_mm_pool.size - global_mm_pool.occupied;
// }

// static int mem_sz(int pid) {
//   return mem_size[pid];
// }

bool check_overlap(int num, intptr_t addr) {
  struct chunk* chunk0 = virt2chunk(&global_mm_pool, (void*)addr);
  for (int i = 0; i < num; i++) {
    struct chunk* chunk = virt2chunk(&global_mm_pool, alloc_addr[i]);
    if ((intptr_t)alloc_addr[i] >= addr) {
      if (addr + (1 << (chunk0->order + 12)) > (intptr_t)alloc_addr[i]) {
        return false;
      }
    } else {
      if ((intptr_t)alloc_addr[i] + (1 << (chunk->order + 12)) > addr) {
        return false;
      }
    }
  }
  return true;
}
int main() {
  pmm->init();
  int apply = 0;
  // test 4KB aligned
  for (int i = 0; i < 16; i++) {
    alloc_addr[i] = pmm->alloc(((i % 16) + 1) * 4096);
    apply += ((i % 16) + 1) * 4096;
    check(alloc_addr[i] && (intptr_t)alloc_addr[i] % 4096 == 0);
    bool overlap = check_overlap(i, (intptr_t)alloc_addr[i]);
    check(overlap);
    success("%d allocation : 0x%x size: %dKB", i, alloc_addr[i],
            ((i % 16) + 1) * 4);
  }
  info("apply: %d, alloc: %d", apply, total_mem());
  for (int i = 0; i < 16; i++) {
    pmm->free(alloc_addr[i]);
    success("free %d: 0x%x", i, alloc_addr[i]);
  }
  global_mm_pool.occupied = 0;
  // test < 4KB space
  apply = 0;
  for (int i = 0; i < 16; i++) {
    alloc_addr[i] = pmm->alloc((i + 1) * 64);
    apply += (i + 1) * 64;
    check(alloc_addr[i]);
    // bool overlap = check_overlap(i, alloc_addr[i]);
    // check(!overlap);
  }
  info("apply: %d, alloc: %d", apply, total_mem());
  for (int i = 0; i < 16; i++) {
    pmm->free(alloc_addr[i]);
    success("free %d: 0x%x", i, alloc_addr[i]);
  }
  global_mm_pool.occupied = 0;
  // test trivial allocation
  apply = 0;
  for (int i = 0; i < 16; i++) {
    alloc_addr[i] = pmm->alloc((i + 1) * 512);
    check(alloc_addr[i]);
    apply += (i + 1) * 512;
    if ((i + 1) * 512 >= 4096) {
      check((intptr_t)alloc_addr[i] % 4096 == 0);
    }
  }
  info("apply: %d, alloc: %d", apply, total_mem());
  for (int i = 0; i < 16; i++) {
    pmm->free(alloc_addr[i]);
    success("free %d: 0x%x", i, alloc_addr[i]);
  }
  global_mm_pool.occupied = 0;
  printf("\ntest slab\n");
  void* small = pmm->alloc(1000);
  pmm->free(small);
  return 0;
}