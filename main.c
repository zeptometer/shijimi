#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>

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
	proc->in_fd = dup(0);
      } else {
	proc->in_fd = open(proc->input_redirection, O_RDONLY);
      }
    } else {
      proc->in_fd = pipefd[0];
    }

    if (proc->next == NULL) {
      if (proc->input_redirection) {
	proc->out_fd = dup(1);
      } else {
	proc->out_fd = open(proc->output_redirection, get_write_option(proc->output_option));
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
    close(p->in_fd);
    close(p->out_fd);
  }
}

void exec_job (job job) {
  process *proc = job.process_list;

  open_pipes(proc);

  pid_t pgid = exec_process_first(proc, job.mode);
  for (process *p=proc->next; p!=NULL; p=p->next) {
    exec_process(p, pgid);
  }

  int status;
  switch (job.mode) {
  case FOREGROUND:
    while (waitpid(-pgid, &status, 0) != -1)
      if (WIFSTOPPED(status) && in_proc(cz_procs, pgid))
	push_proc(cz_procs, pgid);
    tcsetpgrp(0, shell_pgid);
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
  int status;
  pid_t child_pid;
  while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {
    pid_t child_pgid = getpgid(child_pid);
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
      pid_t tmp;
      int status;
      while ((tmp = waitpid(-child_pgid, &status, WNOHANG)) > 0)
	;
      if (tmp == -1)
	rem_proc(bg_procs, child_pgid);
    } else if (WIFSTOPPED(status) && in_proc(bg_procs, child_pgid)) {
      rem_proc(bg_procs, child_pgid);
      push_proc(cz_procs, child_pgid);
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
