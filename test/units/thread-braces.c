#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <spinlock.h>
#include <thread.h>
#include <logger.h>
#include <devices.h>

#define MAX_COUNT 100

task_t *task_consumer, *task_producer;

int cnt = 0, tot = 0;
spinlock_t cnt_lock, print_lock;

void consumer(void *arg) {
  while (1) {
    spin_lock(&cnt_lock);
    if (cnt) {
      cnt--;
      tot++;
      spin_lock(&print_lock);
      cprintf("tty1", ")");
      printf(")");
      spin_unlock(&print_lock);
      if (cnt == 0 && tot > MAX_COUNT) {
        spin_unlock(&cnt_lock);
        break;
      }
    }
    spin_unlock(&cnt_lock);
  }
  spin_lock(&print_lock);
  cprintf("tty1", "C");
  printf("C");
  spin_unlock(&print_lock);
  _log_mask = LOG_ERROR | LOG_WARN;
  while (1)
    ;
}

void producer(void *arg) {
  while (1) {
    spin_lock(&cnt_lock);
    if (tot > MAX_COUNT) {
      spin_unlock(&cnt_lock);
      break;
    }
    cnt++;
    tot++;
    spin_lock(&print_lock);
    cprintf("tty1", "(");
    printf("(");
    spin_unlock(&print_lock);
    spin_unlock(&cnt_lock);
  }
  spin_lock(&print_lock);
  cprintf("tty1", "P");
  printf("P");
  spin_unlock(&print_lock);
  while (1)
    ;
}

static void create_threads() {
  task_consumer = pmm->alloc(sizeof(task_t));
  task_producer = pmm->alloc(sizeof(task_t));
  kmt->create(task_consumer, "consumer", consumer, NULL);
  kmt->create(task_producer, "producer", producer, NULL);
}

int main() {
  ioe_init();

  _log_mask = LOG_ERROR | LOG_WARN | LOG_INFO | LOG_SUCCESS;

  cte_init(os->trap);
  os->init();

  spin_init(&print_lock, "print_lock");
  spin_init(&cnt_lock, "cnt_lock");

  create_threads();

  kmt_print_all_tasks();
  kmt_print_cpu_tasks();
  mpe_init(os->run);
  return 1;
}