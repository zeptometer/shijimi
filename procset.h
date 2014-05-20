#include <unistd.h>
#include <stdbool.h>

#ifndef PROCSET_H
#define PROCSET_H

typedef struct {
  int size;
  int capacity;
  pid_t *pgids;
} procset;

procset *make_proc_set (int capacity);
bool     in_proc       (procset *ps, pid_t pid);
void     push_proc     (procset *ps, pid_t pid);
void     rem_proc      (procset *ps, pid_t pid);
pid_t    pop_proc      (procset *ps);

#endif
