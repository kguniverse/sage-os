#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <sem.h>
#include <thread.h>
#include <logger.h>

#define MAX_COUNT 100

task_t *task_consumer, *task_producer;

int cnt = 0, tot = 0;
sem_t cnt_lock;

void consumer(void *arg) {
  while (1) {
    sem_wait(&cnt_lock);
    if (cnt) {
      cnt--;
      tot++;
      printf(")");
      if (cnt == 0 && tot > MAX_COUNT) {
        sem_signal(&cnt_lock);
        break;
      }
    }
    sem_signal(&cnt_lock);
  }
  info("\nconsumer finish\n");
  while (1)
    ;
}

void producer(void *arg) {
  while (1) {
    sem_wait(&cnt_lock);
    if (tot > MAX_COUNT) {
      sem_signal(&cnt_lock);
      break;
    }
    cnt++;
    tot++;
    printf("(");
    sem_signal(&cnt_lock);
  }
  info("\nproducer finish\n");
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
  cte_init(os->trap);
  os->init();
  create_threads();
  kmt_print_all_tasks();
  kmt_print_cpu_tasks();
  mpe_init(os->run);
  return 1;
}