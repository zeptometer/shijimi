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

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#define BUF_LEN 1000

char nullstr[1] = "\0";

int main(int argc, char** argv, char** envp) {
  char buf[BUF_LEN];
  job* j;
  process p;
  pid_t pid;
  int status, n_process, i;
  int pipefd[2];

  while (1) {
    get_line(buf, BUF_LEN);
    if (buf[0] == '\0') {
      break;
    }
    j = parse_line(buf);
    if (j == NULL) {
      break;
    }
#ifdef DEBUG
    print_job_list(j);
#endif
    p = j->process_list[0];
    n_process = 0;

    if (strcmp(p.program_name, "exit") == 0) {
      break;
    }

    if (p.input_redirection) {
      pipefd[0] = open(p.input_redirection, O_RDONLY);
    } else {
      pipefd[0] = 0;
    }

    while (1) {
      int in = pipefd[0], out;
      if (p.next != NULL) {
	pipe(pipefd);
	out = pipefd[1];
      } else if (p.next == NULL && p.output_redirection) {
	switch (p.output_option) {
	case TRUNC:
	  out = open(p.output_redirection, O_CREAT | O_WRONLY);
	  break;
	case APPEND:
	  out = open(p.output_redirection, O_CREAT | O_WRONLY | O_APPEND);
	  break;
	default:
	  out = -1;
	  perror("unknown mode");
	  abort();
	}
	access(p.output_redirection, R_OK|W_OK);
      } else {
	out = 1;
      }

      pid = fork();
      if (pid == 0) {
	if (in != 0) {
	  dup2(in, 0);
	  close(in);
	}
	if (pipefd[0] != 0) {
	  close(pipefd[0]);
	}
	if (out != 1) {
	  dup2(out, 1);
	  close(out);
	}
	execve(p.program_name, p.argument_list, envp);
      } else {
	n_process++;
	if (in != 0)  {close(in);}
	if (out != 1) {close(out);}
	if (n_process >= PLEN && p.next != NULL) {
	  perror("process number overflow");
	  break;
	}
	if (p.next == NULL) {
	  break;
	}
	p = *p.next;
      }
    }
    for (i=0; i<n_process; i++) {
      /* waitpid(pids[i], &status, WEXITED);  */
      wait(&status);
    }
  }

  return 0;
}
