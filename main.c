#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "parse.h"
#include "procset.h"
#ifdef DEBUG
#include "print.h"
#endif

const int BUF_SIZE  = 200;
const int INIT_PROCSET_CAP = 30;

struct sigaction sigign, sigdefault;

pid_t shell_pgid;
char **envp;
procset *bg_procs, *cz_procs;

int get_write_option(write_option wo) {
  int op = O_CREAT | O_WRONLY;
  switch (wo) {
  case TRUNC:
    return op | O_TRUNC;
  case APPEND:
    return op | O_APPEND;
  default:
    return -1;
  }
}

int open_pipes(process *proc) {
  int pipefd[2];
  process *prev = NULL;

  for(; proc != NULL; prev = proc, proc = proc->next) {
    if (prev == NULL) {
      if (proc->input_redirection) {
	proc->in_fd = open(proc->input_redirection, O_RDONLY);
      } else {
	proc->in_fd = dup(0);
      }
    } else {
      proc->in_fd = pipefd[0];
    }

    if (proc->next == NULL) {
      if (proc->output_redirection) {
	int op = get_write_option(proc->output_option);
	proc->out_fd = open(proc->output_redirection, op, 0664);
      } else {
	proc->out_fd = dup(1);
      }
    } else {
      pipe(pipefd);
      proc->out_fd = pipefd[1];
    }
  }
  return 0;
}

void init_child(process *p) {
  dup2(p->in_fd, 0);
  dup2(p->out_fd, 1);
  close(p->in_fd);
  close(p->out_fd);

  sigaction(SIGINT,  &sigdefault, NULL);
  sigaction(SIGTTOU, &sigdefault, NULL);
  sigaction(SIGTSTP, &sigdefault, NULL);

  execve(p->program_name, p->argument_list, envp);
}

pid_t exec_process_first(process *p, job_mode mode) {
  pid_t pid = fork();
  if (pid == 0) {
    pid_t pid = getpid();
    setpgid(pid, pid);
    if (mode == FOREGROUND) tcsetpgrp(0, pid);
    init_child(p);
  } else {
    setpgid(pid, pid);
    close(p->in_fd);
    close(p->out_fd);
    return pid;
  }
  return -1;
}

void exec_process(process *p, pid_t pgid) {
  pid_t pid = fork();
  if (pid == 0) {
    setpgid(getpid(), pgid);
    init_child(p);
  } else {
    setpgid(pid, pgid);
    close(p->in_fd);
    close(p->out_fd);
  }
}

void wait_foregroud_process(pid_t pgid) {
  int status;
  while (1) {
    while (waitpid(-pgid, &status, WUNTRACED) != -1) {
      if (WIFSTOPPED(status) && !in_proc(cz_procs, pgid)) {
	push_proc(cz_procs, pgid);
	break;
      }
    }
    if (errno == ECHILD) break;
    if (errno == EINTR) continue;
    perror("error");
    abort();
  }
  tcsetpgrp(0, shell_pgid);
}

bool exec_builtin(job job) {
  process *proc = job.process_list;
  char* name = proc->program_name;
  char** args = proc->argument_list;

  if (strcmp(name, "exit") == 0) {
    exit(0);
  } else if (strcmp(name, "jobs") == 0) {
    printf("Background Jobs:\n");
    for (int i=0; i<bg_procs->size; i++)
      printf("%02d %d\n", i, bg_procs->pgids[i]);

    printf("\nSuspended Jobs:\n");
    for (int i=0; i<cz_procs->size; i++)
      printf("%02d %d\n", i, cz_procs->pgids[i]);

    return true;
  } else if (strcmp(name, "bg") == 0) {
    int idx = cz_procs->size-1;
    if (idx < 0)
      return true;

    pid_t pgid = pop_proc(cz_procs, idx);
    push_proc(bg_procs, pgid);
    kill(-pgid, SIGCONT);
    return true;
  } else if (strcmp (name, "fg") == 0) {
    int idx = cz_procs->size-1;
    if (idx < 0)
      return true;

    pid_t pgid = pop_proc(cz_procs, idx);
    tcsetpgrp(0, pgid);
    kill(-pgid, SIGCONT);
    wait_foregroud_process(pgid);
    return true;
  }
  return false;
}

void exec_job (job job) {
  if (exec_builtin(job))
    return;

  process *proc = job.process_list;

  open_pipes(proc);

  pid_t pgid = exec_process_first(proc, job.mode);
  for (process *p=proc->next; p!=NULL; p=p->next) {
    exec_process(p, pgid);
  }

  switch (job.mode) {
  case FOREGROUND:
    wait_foregroud_process(pgid);
    break;
  case BACKGROUND:
    push_proc(bg_procs, pgid);
    break;
  default:
    fputs("invalid mode", stderr);
    abort();
  }
}

void exec_job_list (job *job_list) {
  while (job_list != NULL) {
    exec_job(*job_list);
    job_list = job_list->next;
  }
}

void wait_bg_procs() {
  for (int i=0; i<bg_procs->size; i++) {
    int status;
    pid_t pgid = bg_procs->pgids[i], tmp;
    while ((tmp = waitpid(-pgid, &status, WNOHANG)) > 0) {
      if (WIFSTOPPED(status) && !in_proc(cz_procs, pgid)) {
	rem_proc(bg_procs, pgid);
	push_proc(cz_procs, pgid);
      }
    }
    if (tmp == -1) {
      rem_proc(bg_procs, pgid);
      printf("job %d terminate\n", pgid);
    }
  }
}

int main(int argc, char** argv, char** e) {
  envp = e;
  shell_pgid = getpgrp();
  bg_procs = make_proc_set(INIT_PROCSET_CAP);
  cz_procs = make_proc_set(INIT_PROCSET_CAP);

  sigign.sa_handler = SIG_IGN;
  sigdefault.sa_handler = SIG_DFL;
  sigaction(SIGINT, &sigign, NULL);
  sigaction(SIGTTOU, &sigign, NULL);
  sigaction(SIGTSTP, &sigign, NULL);

  while (1) {
    char buf[BUF_SIZE];
    job* job_list;
    if (get_line(buf, BUF_SIZE) == NULL)
      break ;
    job_list = parse_line(buf);
    if (job_list == NULL)
      continue;

    exec_job_list(job_list);
    wait_bg_procs();
  }
}
