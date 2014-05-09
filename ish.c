#include "parser/parse.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#define BUF_LEN 1000

int main(int argc, char** argv, char** envp) {
  char buf[BUF_LEN];
  job* j = parse_line(get_line(buf, BUF_LEN));
  process p = j->process_list[0];
  pid_t pid;
  int status;
  
  pid = fork();
  if (pid == 0) {
    /* char** poyo; */
    /* printf("hoge: %s:\n", p.program_name); */
    /* for(poyo = p.argument_list; *poyo != NULL; poyo++) */
    /*   printf("tako: %s:\n", *poyo); */
    execve(p.program_name, p.argument_list, envp);
  } else {
    waitpid(pid, &status, 0);
  }
  return status;
}
