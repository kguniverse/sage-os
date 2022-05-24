#ifndef __THREAD_H__
#define __THREAD_H__

#include <common.h>
#include <list.h>
#include <spinlock.h>

#define FILL_STACK 0xfd
#define FILL_FENCE 0xcd

#define STACK_SIZE       8192
#define STACK_FENCE_SIZE 32

#define MAX_TASK_STATES 8

#define MAX_MAP_NUM 64

int mem_size[20];

enum task_states {
  ST_U,  // Unused
  ST_E,  // Embryo
  ST_T,  // To sleep
  ST_S,  // Sleeping
  ST_W,  // Waken up
  ST_R,  // Running
  ST_Z,  // Zombie
  ST_X   // Special
};

struct mapnode {
  void *va, *pa;

  struct list_head list;
};

typedef struct mapnode mapnode_t;

struct task {
  uint32_t pid;                   // process id
  const char* name;               // process name for debug
  void (*entry)(void*);           // kernel thread entry
  void* arg;                      // args of entry func
  enum task_states state;         // process state
  sem_t* wait_sem;                // semaphore that the thread waiting for
  bool killed;                    // whether process is killed
  int32_t owner;                  // which cpu running this process now
  int32_t count;                  // a counter to avoid deadlock
  char fenceA[STACK_FENCE_SIZE];  // 32 bytes fence
  char stack[STACK_SIZE];         // user stack
  char fenceB[STACK_FENCE_SIZE];  // 32 bytes fence
  Context* context;               // process user context
  struct task* next;
  struct task* parent;
  int* wait_subproc_status;

  AddrSpace as;
  int pmsize;  // proc memory size
  struct list_head pg_map;
};

const char* task_states_str[MAX_TASK_STATES];

task_t root_task;
task_t* cpu_tasks[MAX_CPU];
spinlock_t task_list_lock;

void kmt_print_all_tasks(int mask);
void kmt_print_cpu_tasks(int mask);
uint32_t kmt_next_pid();

#endif