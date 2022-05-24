#include <kernel.h>
#include <syscall_defs.h>
#include <syscalls.h>
#include <thread.h>
#include <vm.h>
#include <common.h>
#include <list.h>

#include "initcode.inc"

void uproc_init();
int uproc_create(task_t *proc, const char *name);
Context *uproc_pagefault(Event ev, Context *context);
Context *uproc_syscall(Event ev, Context *context);
int growuproc(int n);

/**
 * @brief initialize uproc
 *
 */
void uproc_init() {
  // create first proc
  task_t *initproc = pmm->alloc(sizeof(task_t));
  uproc_create(initproc, "initcode");

  // init the user vm area
  inituvm(initproc, _init, _init_len);
  initproc->pmsize = SZ_PAGE;

  os->on_irq(0, EVENT_PAGEFAULT, uproc_pagefault);
}

/**
 * @brief create a user process.
 *        Notice: Do not copy _init
 *
 * @param proc PCB
 * @param name process name
 */
int uproc_create(task_t *proc, const char *name) {
  assert_msg(proc != NULL && name != NULL, "null arguments in uproc_create");
  proc->pid      = kmt_next_pid();
  proc->name     = name;
  proc->entry    = NULL;
  proc->arg      = NULL;
  proc->state    = ST_E;
  proc->owner    = -1;
  proc->count    = 0;
  proc->wait_sem = NULL;
  proc->killed   = 0;
  proc->next     = NULL;

  memset(proc->fenceA, FILL_FENCE, sizeof(proc->fenceA));
  memset(proc->stack, FILL_STACK, sizeof(proc->stack));
  memset(proc->fenceB, FILL_FENCE, sizeof(proc->fenceB));

  Area kstack = {(void *)proc->stack,
                 (void *)proc->stack + sizeof(proc->stack)};
  protect(&proc->as);
  AddrSpace *as = &proc->as;
  INIT_LIST_HEAD(&proc->pg_map);

  // ucontext entry: start addr of proc
  proc->context = ucontext(as, kstack, as->area.start);

  // // map stack
  // intptr_t i;
  // for (i = 0; i < STACK_SIZE; i += (as->pgsize)) {
  //   uproc_pgmap(as, as->area.end - STACK_SIZE + i,
  //               (void *)((intptr_t)kstack.start + i), MMAP_WRITE |
  //               MMAP_READ);
  // }

  // Notice: do not inituvm here, move to uproc_init
  proc->pmsize = 0;

  // add to task list
  assert_msg(!spin_holding(&task_list_lock), "already hold task_list_lock");
  kmt->spin_lock(&task_list_lock);
  task_t *tp = &root_task;
  while (tp->next)
    tp = tp->next;
  tp->next = proc;
  kmt->spin_unlock(&task_list_lock);

  return proc->pid;
}

/**
 * @brief EVENT_PAGEFAULT handler on trap
 *
 * @param ev
 * @param context
 * @return Context* NULL
 */
Context *uproc_pagefault(Event ev, Context *context) {
  // ev.ref: the failed virtual addr, 48bit unsigned long
  uintptr_t ref = (uintptr_t)ev.ref;
  info("pid=%d pagefault: %06x%06x", cpu_tasks[cpu_current()]->pid, ref >> 24,
       (uint32_t)ref & ((1L << 24) - 1));
  AddrSpace *as = &cpu_tasks[cpu_current()]->as;
  void *paddr   = pmm->pgalloc();
  mem_size[cpu_tasks[cpu_current()]->pid] += SZ_PAGE;
  // vaddr:  the start addr of that page
  uintptr_t vaddr = ref & ~(as->pgsize - 1L);
  success("get next page frame: 0x%x", (intptr_t)paddr);
  uproc_pgmap(cpu_tasks[cpu_current()], (void *)vaddr, paddr,
              MMAP_READ | MMAP_WRITE);
  return NULL;
}

int sys_kputc(task_t *proc, char ch) {
  putch(ch);
  return 0;
}

int sys_fork(task_t *proc) {
  info("pid=%d syscall fork", proc->pid);

  task_t *subproc = pmm->alloc(sizeof(task_t));
  // TODO: change subproc's name
  uproc_create(subproc, proc->name);

  // do not copy parent's rsp0, cr3
  uintptr_t rsp0 = subproc->context->rsp0;
  void *cr3      = subproc->context->cr3;
  // memcpy(subproc->stack, proc->stack, STACK_SIZE);
  memcpy(subproc->context, proc->context, sizeof(Context));
  subproc->context->rsp0 = rsp0;
  subproc->context->cr3  = cr3;
  subproc->context->rax  = 0;

  // copy pages
  copyuvm(subproc, proc, proc->pmsize);
  subproc->pmsize = proc->pmsize;
  subproc->parent = proc;

  return subproc->pid;
}

int sys_exit(task_t *proc, int status) {
  task_t *p;
  kmt->spin_lock(&task_list_lock);
  for (p = root_task.next; p != NULL; p = p->next) {
    if (proc->parent == p) {
      if (p->state == ST_S && p->wait_subproc_status != NULL) {
        p->state                  = ST_W;
        *(p->wait_subproc_status) = status;
      }
      break;
    }
  }
  kmt->spin_unlock(&task_list_lock);
  proc->killed = 1;
  return 1;
}

int sys_wait(task_t *proc, int *status) {
  task_t *p;
  int havekids, pid;
  havekids = 0;
  assert_msg(!spin_holding(&task_list_lock), "already hold task_list_lock");
  kmt->spin_lock(&task_list_lock);
  for (p = root_task.next; p != NULL; p = p->next) {
    if (p->parent != proc) continue;
    havekids = 1;
  }
  // No point waiting if we don't have any children.
  if (!havekids || proc->killed) {
    kmt->spin_unlock(&task_list_lock);
    return -1;
  }

  proc->wait_subproc_status = status;
  proc->state               = ST_S;
  kmt->spin_unlock(&task_list_lock);

  return 0;
}

int sys_kill(task_t *proc, int pid) {
  panic("not implemented");
  return 1;
}

int sys_getpid(task_t *proc) {
  return proc->pid;
}

void *sys_mmap(task_t *proc, void *addr, int length, int prot, int flags) {
  panic("not implemented");
}

int sys_sleep(task_t *proc, int seconds) {
#define TIMER_HZ 200
  proc->count = 0 - seconds * TIMER_HZ;
  proc->state = ST_S;
  return 0;
}

int64_t sys_uptime(task_t *proc) {
  return io_read(AM_TIMER_UPTIME).us / 1000;
}

int growuproc(int n) {
  int sz;
  task_t *task = cpu_tasks[cpu_current()];
  sz           = task->pmsize;
  if (n > 0) {
    sz = allocuvm(task, sz, sz + n);
    if (sz == 0) return -1;
  } else if (n < 0) {
    sz = deallocuvm(task, sz, sz + n);
    if (sz == 0) return -1;
  }
  task->pmsize = sz;
  return 0;
}

int sys_sbrk(int n) {
  int sz;
  sz = cpu_tasks[cpu_current()]->pmsize;
  if (growuproc(n) < 0) return -1;
  return sz;
}

MODULE_DEF(uproc) = {
    .init = uproc_init,
};