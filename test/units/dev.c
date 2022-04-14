#include <common.h>
#include <trap.h>
#include <devices.h>

/**
 * @brief
 *
 * @param arg
 */
static void tty_reader(void *arg) {
  device_t *tty = dev->lookup(arg);
  char cmd[128], resp[128], ps[16];
  snprintf(ps, 16, "(%s) $ ", arg);
  while (1) {
    tty->ops->write(tty, 0, ps, strlen(ps));
    int nread  = tty->ops->read(tty, 0, cmd, sizeof(cmd) - 1);
    cmd[nread] = '\0';
    sprintf(resp, "tty reader task: got %d character(s).\n", strlen(cmd));
    tty->ops->write(tty, 0, resp, strlen(resp));
  }
}

void run() {
  iset(true);
  while (1)
    ;
}

int main() {
  ioe_init();
  cte_init(os->trap);
  os->init();
  kmt->create(pmm->alloc(sizeof(task_t)), "tty_reader", tty_reader, "tty1");
  kmt->create(pmm->alloc(sizeof(task_t)), "tty_reader", tty_reader, "tty2");
  mpe_init(run);
  return 0;
}
