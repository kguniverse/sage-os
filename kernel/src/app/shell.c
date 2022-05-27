#include <common.h>
#include <shell.h>
#include <file.h>
#include <vfs.h>
#include <user.h>
#include <fs.h>
#include <syscalls.h>
#include <buddy.h>

const cmd_t cmd_list[] = {
    {"man", man},     {"echo", echo},   {"ls", ls},       {"pwd", pwd},
    {"cd", cd},       {"cat", cat},     {"write", write}, {"link", link},
    {"mkdir", mkdir}, {"rmdir", rmdir}, {"rm", rm},       {"ps", ps},
    {"stat", stat},   {"mem", mem},
};

const int NR_CMD = sizeof(cmd_list) / sizeof(cmd_t);

void shell_task(void *arg) {
  task_t *proc  = (task_t *)arg;
  char buf[512] = "";
  char pwd[512] = "";
  char cmd[512] = "";
  char ret[512] = "";

  int stdin  = 0;
  int stdout = 1;

  sprintf(buf, "Welcome to SOShell.\nType [man] for help.\n\n");
  sys_write(proc, stdout, buf, strlen(buf));

  sprintf(pwd, "/");
  while (true) {
    sprintf(buf, "(tty) %s $ ", pwd);
    sys_write(proc, stdout, buf, strlen(buf));

    size_t nread   = sys_read(proc, stdin, cmd, sizeof(cmd));
    cmd[nread - 1] = '\0';
    // jump leading space
    char *arg = cmd;
    while (*arg == ' ')
      ++arg;
    memset(ret, 0, sizeof(ret));

    bool succ = false;
    for (int i = 0; i < NR_CMD; ++i) {
      if (strncmp(arg, cmd_list[i].name, strlen(cmd_list[i].name)) == 0) {
        succ = true;
        arg += strlen(cmd_list[i].name);
        // skip interval spaces
        while (*arg == ' ')
          ++arg;
        // char *tmp = "cd";
        if (!strcmp("cd", cmd_list[i].name)) {
          cmd_list[i].func(proc, arg, pwd, ret);
        } else {
          // if (fork1(proc) == 0) {
          //   task_t *nowproc = current_task;
          //   runcmd(nowproc, arg, pwd, ret, i);
          // }
          // int stati;
          // sys_wait(proc, &stati);
          cmd_list[i].func(proc, arg, pwd, ret);
        }
      }
    }
    if (!succ) sprintf(ret, "Invalid command.\n");
    sys_write(proc, stdout, ret, strlen(ret));
    sys_write(proc, stdout, "\n", 1);
  }
  panic("shell cannot exit.");
}

void shell_init() {
  task_t *task = pmm->alloc(sizeof(task_t));
  kmt->create(task, "shell", shell_task, task);
}

// int fork1(task_t *proc) {
//   int pid;

//   pid = sys_fork(proc);
//   if (pid == -1) panic("fork");
//   return pid;
// }

// void runcmd(task_t *proc, char *arg, char *pwd, char *ret, int i) {
//   cmd_list[i].func(proc, arg, pwd, ret);
//   sys_exit(proc, 0);
// }

bool get_dir(char *arg, char *pwd, char *dir) {
  char buf[512] = "";
  if (arg[0] == '/') {
    sprintf(buf, "%s", arg);
  } else {
    if (pwd[strlen(pwd) - 1] == '/') {
      sprintf(buf, "%s%s", pwd, arg);
    } else {
      sprintf(buf, "%s/%s", pwd, arg);
    }
  }

  size_t pos = 0;
  size_t cur = 0;
  size_t len = strlen(buf);

  if (arg[0] == '.') {
    if (arg[1] == '\0') {
      dir = pwd;
      return true;
    }
    if (arg[1] == '.') {
      int flag = 2;
      while (flag) {
        if (buf[len] == '/') {
          flag--;
        }
        if (flag == 0) {
          break;
        }
        len--;
      }
    }
  }

  while (pos <= len) {
    if (buf[pos] == ' ') {
      break;
    } else {
      dir[cur] = buf[pos];
      ++pos, ++cur;
    }
  }
  dir[cur] = '\0';
  len      = strlen(dir);
  if (len > 1 && dir[len - 1] == '/') dir[len - 1] = '\0';
  return true;
}

FUNC(man) {
  sprintf(ret, "Available commands: \n");
  for (int i = 0; i < NR_CMD; ++i) {
    strcat(ret, " - ");
    strcat(ret, cmd_list[i].name);
    strcat(ret, "\n");
  }
  strcat(ret, "\n");
}

FUNC(echo) {
  sprintf(ret, "%s\n", arg);
}

FUNC(pwd) {
  sprintf(ret, "%s\n", pwd);
}

FUNC(ls) {
#define MAX_DIRENT 16
  inode_t *inode;
  if (*arg == ' ' || *arg == '\0') {
    inode = proc->cwd;
  } else {
    inode = namei(arg);
  }
  assert_msg(inode->type == DINODE_TYPE_D, "cwd inode not directory!");
  dirent_t entries[MAX_DIRENT] = {0};
  int nbytes = readi(inode, (void *)entries, 0, sizeof(entries));
  if (nbytes == -1) {
    sprintf(ret, "ls error: cannot open inode for %s", pwd);
    return;
  }
  for (int i = 0; i < MAX_DIRENT; i++) {
    if (entries[i].inum == 0) continue;
    cprintf("tty1", "%s ", entries[i].name);
  }
}

FUNC(cd) {
  char dir[512] = "";
  if (sys_chdir(proc, arg) != 0) {
    sprintf(ret, "Invalid directory address.\n");
  } else {
    get_dir(arg, pwd, dir);
    strcpy(pwd, dir);
    sprintf(ret, "Change directory to %s.\n", arg);
  }
}

FUNC(cat) {
  int fd = sys_open(proc, arg, O_RDONLY);
  if (fd < 0) {
    sprintf(ret, "VFS ERROR: open failed with status %d.\n", fd);
  } else {
    sys_read(proc, fd, ret, 512);
    sys_close(proc, fd);
  }
}

FUNC(write) {
  // char dir[512]  = "";
  char arg1[512] = "";
  char *arg2     = arg;
  for (size_t i = 0; *arg2 != '\0'; ++i, ++arg2) {
    arg1[i] = *arg2;
    if (*arg2 == ' ') {
      arg1[i] = '\0';
      ++arg2;
      break;
    }
  }

  int fd = sys_open(proc, arg1, O_WRONLY | O_CREAT);
  if (fd < 0) {
    sprintf(ret, "VFS ERROR: open failed with status %d.\n", fd);
  } else {
    size_t nwrite = sys_write(proc, fd, (void *)arg2, strlen(arg2));
    sys_write(proc, fd, "\n", 1);
    sys_close(proc, fd);
    sprintf(ret, "Writed %d bytes successfully.\n", nwrite);
  }
}

FUNC(link) {
  char arg1[512] = "";
  char *arg2     = arg;
  for (size_t i = 0; *arg2 != '\0'; ++i, ++arg2) {
    arg1[i] = *arg2;
    if (*arg2 == ' ') {
      arg1[i] = '\0';
      ++arg2;
      break;
    }
  }

  int status = sys_link(proc, arg1, arg2);
  if (!status) {
    sprintf(ret, "Linked %s -> %s successfully.\n", arg1, arg2);
  } else {
    sprintf(ret, "VFS ERROR: link failed with status %d.\n", status);
  }
}

FUNC(mkdir) {
  char dir[512] = "";

  int status = sys_mkdir(proc, arg);
  if (!status) {
    sprintf(ret, "Successfully created folder %s.\n", dir);
  } else {
    sprintf(ret, "VFS ERROR: mkdir failed with status %d.\n", status);
  }
}

FUNC(rmdir) {
  int status = sys_unlink(proc, arg);
  if (!status) {
    sprintf(ret, "Successfully removed %s.\n", arg);
  } else {
    sprintf(ret, "VFS ERROR: unlink failed with status %d.\n", status);
  }
}

FUNC(rm) {
  int status = sys_unlink(proc, arg);
  if (!status) {
    sprintf(ret, "Successfully removed %s.\n", arg);
  } else {
    sprintf(ret, "VFS ERROR: unlink failed with status %d.\n", status);
  }
}

/*
FUNC(run) {
  task_t *game = pmm->alloc(sizeof(game));
  kmt_init_task(game, "type game", app_type_game, NULL);
  game->parent = current_task;

  spin_lock(&task_list_lock);

  current_task->wait_subproc = true;
  current_task->state        = ST_S;

  task_t *tp = &root_task;
  while (tp->next)
    tp = tp->next;
  tp->next = game;

  spin_unlock(&task_list_lock);

  yield();
}
*/

FUNC(ps) {
  bool holding = spin_holding(&task_list_lock);
  if (!holding) spin_lock(&task_list_lock);
  cprintf("tty1", "[all tasks]\n");
  for (task_t *tp = &root_task; tp != NULL; tp = tp->next) {
    cprintf("tty1", "pid %d <%s>: cpu=%d, state=%s, wait_sem=%s\n", tp->pid,
            tp->name, tp->owner, task_states_str[tp->state],
            tp->wait_sem ? tp->wait_sem->name : "null");
  }
  if (!holding) spin_unlock(&task_list_lock);
}

FUNC(stat) {
  stat_t *targetFile = pmm->alloc(sizeof(stat_t));
  ;
  int fd     = sys_open(proc, arg, O_RDONLY);
  int status = sys_fstat(proc, fd, targetFile);
  if (!status) {
    char *typestring;
    if (targetFile->type == 1) {
      typestring = "file";
    } else if (targetFile->type == 2) {
      typestring = "directory";
    } else {
      typestring = "unused";
    }
    sprintf(ret, "file: %s: type=%s, link number=%d, size=%dbytes\n", arg,
            typestring, targetFile->links, targetFile->size);
  } else {
    sprintf(ret, "VFS ERROR: stat failed with status %d.\n", status);
  }
  sys_close(proc, fd);
}

FUNC(mem) {
  bool holding = spin_holding(&task_list_lock);
  if (!holding) spin_lock(&task_list_lock);
  cprintf("tty1", "total memory: %d\n", total_mem);
  for (task_t *tp = &root_task; tp != NULL; tp = tp->next) {
    cprintf("tty1", "pid %d <%s> pmsize: %d\n", tp->pid, tp->name, tp->pmsize);
  }
  if (!holding) spin_unlock(&task_list_lock);
}
