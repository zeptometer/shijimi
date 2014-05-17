#include "config.h"
#include "parser/parse.h"
#ifdef DEBUG
#include "parser/print.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#define BUF_LEN 1000
#define INIT_PS_CAP 40

/*
  proc_set is used to manage background and suspended processes
*/

struct proc_set_ {
  int size;
  int capacity;
  pid_t *pgids;
};

typedef struct proc_set_ *proc_set;

proc_set make_proc_set(int capacity) {
  proc_set ps = malloc(sizeof(struct proc_set_));
  ps->size = 0;
  ps->capacity = capacity = 0;
  ps->pgids = malloc(sizeof(pid_t)*capacity);
  return ps;
}

void resize_proc_set(proc_set ps) {
  pid_t* new_pgids = malloc(sizeof(pid_t)*ps->capacity*2);
  memcpy(new_pgids, ps->pgids, sizeof(pid_t)*ps->capacity);
  free(ps->pgids);
  ps->pgids = new_pgids;
  ps->capacity *= 2;
}

bool in_proc(proc_set ps, pid_t pid) {
  for (int i=0; i<ps->size; i++)
    if (ps->pgids[i] == pid)
      return true;
  return false;
}

void push_proc(proc_set ps, pid_t pid) {
  if (ps->size == ps->capacity)
    resize_proc_set(ps);
  ps->pgids[ps->size++] = pid;
}

void rem_proc(proc_set ps, pid_t pid) {
  int i=0;
  for (; i<ps->size; i++)
    if (ps->pgids[i] == pid)
      break;
  ps->size--;
  for (; i<ps->size; i++)
    ps->pgids[i] = ps->pgids[i+1];
}

pid_t pop_proc(proc_set ps) {
  return ps->pgids[--ps->size];
}

proc_set bg_procs, cz_procs;

pid_t shell_pgid, current_pgid;
struct sigaction sigign, sigdefault;

void monitor_bg_procs() {
  int status;
  pid_t child_pid, child_pgid;
  while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {
    child_pgid = getpgid(child_pid);
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
      pid_t tmp;
      int status;
      while ((tmp = waitpid(-child_pgid, &status, WNOHANG)) > 0)
	;
      if (tmp == -1)
	rem_proc(bg_procs, child_pgid);
    } else if (WIFSTOPPED(status)) {
      if (in_proc(bg_procs, child_pgid)) {
	rem_proc(bg_procs, child_pgid);
	push_proc(cz_procs, child_pgid);
      }
      if (child_pgid == current_pgid) {
	tcsetpgrp(0, shell_pgid);
	current_pgid = shell_pgid;
      }
    }
  }
}

int main(int argc, char** argv, char** envp) {
  char buf[BUF_LEN];
  job* j;
  process p;
  int status;
  int pipefd[2];

  bg_procs = make_proc_set(INIT_PS_CAP);
  cz_procs = make_proc_set(INIT_PS_CAP);

  sigign.sa_handler = SIG_IGN;
  sigdefault.sa_handler = SIG_DFL;

  sigaction(SIGINT, &sigign, NULL);
  sigaction(SIGTTOU, &sigign, NULL);

  current_pgid = shell_pgid = getpgrp();

  while (1) {
    monitor_bg_procs();

    if (get_line(buf, BUF_LEN) == NULL)
      break;
    j = parse_line(buf);
    if (j == NULL)
      continue;

#ifdef DEBUG
    print_job_list(j);
#endif

    p = j->process_list[0];
    if (strcmp(p.program_name, "exit") == 0)
      break;
    if (strcmp(p.program_name, "bg") == 0) {
      pid_t pgid = pop_proc(cz_procs);
      push_proc(bg_procs, pgid);
      kill(pgid, SIGCONT);
      continue;
    }

    if (p.input_redirection)
      pipefd[0] = open(p.input_redirection, O_RDONLY);
    else
      pipefd[0] = 0;

    while (1) {
      int in = pipefd[0], out;
      if (p.next != NULL) {
	pipe(pipefd);
	out = pipefd[1];
      } else if (p.next == NULL && p.output_redirection) {
	int op = 0;
	switch (p.output_option) {
	case APPEND:
	  op |= O_APPEND;
	case TRUNC:
	  op |= O_CREAT | O_WRONLY;
	  break;
	default:
	  fputs("invalid mode", stderr);
	  abort();
	}
	out = open(p.output_redirection, op);
	access(p.output_redirection, R_OK|W_OK);
      } else {
	out = 1;
      }

      pid_t pid = fork();
      if (pid == 0) {
	sigaction(SIGINT, &sigdefault, NULL);
	sigaction(SIGTTOU, &sigdefault, NULL);

	dup2(in, 0);
	dup2(out, 1);
	if (pipefd[0] != 0) close(pipefd[0]);
	if (in != 0)        close(in);
	if (out != 1)       close(out);
	execve(p.program_name, p.argument_list, envp);
      } else {
	if (current_pgid == shell_pgid) {
	  current_pgid = pid;
	  if (j->mode == FOREGROUND)
	    tcsetpgrp(current_pgid, current_pgid);
	}
	setpgid(pid, current_pgid);

	if (in != 0)  close(in);
	if (out != 1) close(out);
	if (p.next == NULL) break;
	p = *p.next;
      }
    }

    switch (j->mode) {
    case FOREGROUND:
      while (waitpid(-current_pgid, &status, 0) != -1)
	;
      tcsetpgrp(0, shell_pgid);
      current_pgid = shell_pgid;
      break;
    case BACKGROUND:
      push_proc(bg_procs, current_pgid);
      break;
    default:
      fputs("invalid mode", stderr);
      abort();
    }
    current_pgid = shell_pgid;
  }

  return 0;
}
