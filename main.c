#include "builtin.h"
#include "print.h"

#define BUF_SIZE 100

char **envp;

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

void exec_single_process (job job) {
  int in = 0, out = 1;
  process proc = *job->process_list;

  if (proc.input_redirection) {
    in = open(p.input_redirection, O_RDONLY);
  }
  if (proc.output_redicretion) {
    out = open(proc.output_redirection, get_write_option(proc.output_option));
    access(proc.output_redirection, R_OK|W_OK);
  }

  if (is_builtin(proc.program_name)) {
    exec_builtin(proc, in, out);
  } else {
    exec_program(proc, in, out)
  }
}

int job_len(job job_list) {
  job j = job_list;
  int i = 0;
  while (j != NULL) {
    i++;
    j = j->next;
  }
  return i;
}

pid_t child_pgid;

void exec_proc(process proc, int in, itn out, int[][2] pipefd, int len) {
  pid_t pid = fork();
  if (pid == 0) {
    dup2(in, 0);
    dup2(out, 1);
    for (int i=0; i<len; i++) {
      close(pipefd[i][0]);
      close(pipefd[i][1]);
    }
    if (child_pgid == 1) {
      child_pgid = getpgrp();
      if (j->mode == FOREGROUND)
	tcsetpgrp(0, child_pgid);
    }
    setpgid(child_pgid);
    execve(proc.program_name, proc.argument_list, envp);
  } else {
    
  }
}

void exec_job (job job) {
  int len = proc_len(job);
  int pipefd[len][2];

  for (int i=0; i<len-1; i++)
    pipe[pipefd[i]];

  process proc = *job->process;
  child_pgid = 1;

  int in = (proc->input_redirection)?open(p.input_redirection, O_RDONLY):0;
  int out = pipefd[0][1];
  for (int i=0; i<len-1; i++) {
    exec_proc(proc, in, out);
    in = pipefd[i][0];
    out = pipefd[i+1][0];
    job = job->next;
  }
  if (proc->output_redirection) {
    int op = get_write_option(proc.output_option)
    out = open(proc.output_redirection, op);
  } else {
    out = 1;
  }
  exec_proc(proc, in, out, pipefd, len);
}


void exec_job_list (job job_list) {
  while (job_list != NULL) {
    exec_job(job_list);
    job_list = job_list->next;
  }
}

int main(int argc, char** argv, char** e) {
  envp = e;

  while (1) {
    char buf[BUF_SIZE];
    job* job_list;
    if (get_line(buf, BUF_SIZE) == NULL)
      break;
    job_list = parse_line(buf);
    if (job_list == NULL)
      continue;

    exec_job_list(job_list);
  }
}
