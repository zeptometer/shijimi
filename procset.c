#include <stdlib.h>
#include <string.h>

#include "procset.h"

procset *make_proc_set(int capacity) {
  procset *ps = malloc(sizeof(procset));
  ps->size = 0;
  ps->capacity = capacity = 0;
  ps->pgids = malloc(sizeof(pid_t)*capacity);
  return ps;
}

void resize_proc_set(procset *ps) {
  pid_t* new_pgids = malloc(sizeof(pid_t)*ps->capacity*2);
  memcpy(new_pgids, ps->pgids, sizeof(pid_t)*ps->capacity);
  free(ps->pgids);
  ps->pgids = new_pgids;
  ps->capacity *= 2;
}

bool in_proc(procset *ps, pid_t pid) {
  for (int i=0; i<ps->size; i++)
    if (ps->pgids[i] == pid)
      return true;
  return false;
}

void push_proc(procset *ps, pid_t pid) {
  if (ps->size == ps->capacity)
    resize_proc_set(ps);
  ps->pgids[ps->size++] = pid;
}

void rem_proc(procset *ps, pid_t pid) {
  int i=0;
  for (; i<ps->size; i++)
    if (ps->pgids[i] == pid)
      break;
  ps->size--;
  for (; i<ps->size; i++)
    ps->pgids[i] = ps->pgids[i+1];
}

pid_t pop_proc(procset *ps, int idx) {
  pid_t pid = ps->pgids[idx];
  ps->pgids[idx] = ps->pgids[--ps->size];
  return pid;
}
