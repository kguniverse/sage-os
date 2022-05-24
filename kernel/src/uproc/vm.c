#include <kernel.h>
#include <thread.h>
#include <buddy.h>
#include <user.h>
#include <vm.h>

void uproc_pgmap(task_t* proc, void* vaddr, void* paddr, int prot);
void uporc_pgunmap(task_t* proc, void* vaddr);
void inituvm(task_t* proc, unsigned char* init, int sz);
int allocuvm(task_t* proc, int newsz, int oldsz);
void copyuvm(task_t* proc, task_t* src, int sz);
int deallocuvm(task_t* proc, int newsz, int oldsz);

#ifdef UNIT_VM
struct frame {
  intptr_t paddr;
  struct list_head list;
};

struct frame frames[20][FRAME_NUM];
struct list_head head[20];
int occupied_frame[20];

intptr_t pick_next_LRU(task_t* proc) {
  if (occupied_frame[proc->pid] == 0) {
    INIT_LIST_HEAD(&head[proc->pid]);
  }
  if (occupied_frame[proc->pid] < FRAME_NUM) {
    intptr_t ptr                               = pmm->pgalloc();
    frames[occupied_frame[proc->pid]++]->paddr = ptr;
    return ptr;
  } else {
    struct frame* frame = list_entry(&head[proc->pid].prev, struct frame, list);
    list_del(&frame->list);
    list_add(&frame->list, &head[proc->pid]);
    return frame->paddr;
  }
}
#endif

/**
 * @brief map one physical page to one virtual page.
 *
 * @param proc PCB
 * @param vaddr virtual page addr
 * @param paddr physical page addr
 * @param prot protection bit
 */
void uproc_pgmap(task_t* proc, void* vaddr, void* paddr, int prot) {
  if (prot == MMAP_NONE) {
    error("Try to unmap a vaddr in mapping function");
    return;
  }
  uintptr_t va   = (uintptr_t)vaddr;
  mapnode_t* pos = NULL;
  list_for_each_entry(pos, &proc->pg_map, list) {
    if ((intptr_t)pos->va == va) {
      error("try to map mapping va");
    }
  }
  // need to introduce slab system, otherwise the waste of space is huge.
  mem_size[proc->pid] += sizeof(mapnode_t);
  mapnode_t* node = pmm->alloc(sizeof(mapnode_t));
  node->pa        = paddr;
  node->va        = vaddr;
  list_add(&node->list, &proc->pg_map);
  AddrSpace* as = &proc->as;
  info("AS %x map va:0x%06x%06x -> pa:0x%x", as->ptr, va >> 24,
       va & ((1L << 24) - 1), paddr);
  // function map already has checks
  map(as, vaddr, paddr, prot);
}

/**
 * @brief unmap one virtual page
 *
 * @param as
 * @param vaddr
 */
void uporc_pgunmap(task_t* proc, void* vaddr) {
  uintptr_t va   = (uintptr_t)vaddr;
  mapnode_t* pos = NULL;
  int flag       = 0;
  list_for_each_entry(pos, &proc->pg_map, list) {
    if ((intptr_t)pos->va == (intptr_t)vaddr) {
      flag = 1;
      break;
    }
  }
  if (!flag) {
    panic("try to unmap a empty page frame");
  }
  AddrSpace* as = &proc->as;
  info("AS %x unmap va:0x%06x%06x", as->ptr, va >> 24, va & ((1L << 24) - 1));
  map(as, vaddr, 0, MMAP_NONE);
}

void inituvm(task_t* proc, unsigned char* init, int sz) {
  AddrSpace* as = &proc->as;
  assert_msg(sz <= SZ_PAGE, "initcode size greater than 4KB");
  char* mem;
  mem = pmm->pgalloc();
  mem_size[proc->pid] += SZ_PAGE;
  memset(mem, 0, sizeof(mem));
  memcpy(mem, init, sz);
  uproc_pgmap(proc, as->area.start, mem, MMAP_READ | MMAP_WRITE);
}

int allocuvm(task_t* proc, int newsz, int oldsz) {
  // AddrSpace* as = &proc->as;
  assert(newsz >= oldsz);

  uintptr_t a = ROUNDUP(oldsz, SZ_PAGE);
  char* mem;
  for (; a < newsz; a += SZ_PAGE) {
    mem = pmm->pgalloc();
    mem_size[proc->pid] += SZ_PAGE;
    if (mem == 0) {
      panic("memory overflowed");
    }
    memset(mem, 0, SZ_PAGE);
    uproc_pgmap(proc, (void*)a, mem, MMAP_READ | MMAP_WRITE);
  }
  return newsz;
}

void copyuvm(task_t* proc, task_t* src, int sz) {
  // intptr_t i;
  char* a;
  mapnode_t* pos = NULL;
  list_for_each_entry(pos, &src->pg_map, list) {
    a = pmm->pgalloc();
    mem_size[proc->pid] += SZ_PAGE;
    memcpy(a, (char*)pos->pa, SZ_PAGE);
    uproc_pgmap(proc, pos->va, a,
                MMAP_READ | MMAP_WRITE);  // only read & write
  }
}

int deallocuvm(task_t* proc, int newsz, int oldsz) {
  assert(newsz <= oldsz);
  intptr_t a;

  a = ROUNDUP(newsz, SZ_PAGE);
  for (; a < oldsz; a += SZ_PAGE) {
    mapnode_t* pos = NULL;
    list_for_each_entry(pos, &proc->pg_map, list) {
      if ((intptr_t)pos->va == (intptr_t)a) {
        uporc_pgunmap(proc, (void*)a);
        pmm->free(pos->pa);
        mem_size[proc->pid] -= SZ_PAGE;
        break;
      }
    }
  }
  return newsz;
}