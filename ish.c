#include "config.h"
#include "parser/parse.h"
#ifdef DEBUG
#include "parser/print.h"
#endif

#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#define BUF_LEN 1000

const pid_t INVALID_PID = -1;
pid_t shell_pgid, current_pgid;
struct sigaction sigign, sigdefault;

int main(int argc, char** argv, char** envp) {
  char buf[BUF_LEN];
  job* j;
  process p;
  int status;
  int pipefd[2];

  sigign.sa_handler = SIG_IGN;
  sigdefault.sa_handler = SIG_DFL;

  sigaction(SIGINT, &sigign, NULL);
  sigaction(SIGTTOU, &sigign, NULL);

  current_pgid = shell_pgid = getpgrp();

  while (1) {
    pid_t pid;

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

      pid = fork();
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
      waitpid(-current_pgid, &status, WNOHANG);
      break;
    default:
      fputs("invalid mode", stderr);
      abort();
    }
    current_pgid = shell_pgid;
  }

  return 0;
}
