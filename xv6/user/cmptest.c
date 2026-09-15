// cmptest: measures turnaround/waiting/response time for a fixed batch
// of CPU-bound processes, for the scheduler comparison in the report
// (section 2.3.3). Run the SAME binary under a RR-built kernel and an
// MLFQ-built kernel and compare the printed averages.
//
// Definitions used here (all in ticks):
//   response time  = first tick the child actually executed - arrival tick
//   turnaround time = tick the child finished          - arrival tick
//   waiting time    = turnaround time - actual CPU burst consumed
//
// All children "arrive" at (approximately) the same tick, are pure
// CPU-bound (never call pause()), and each does the same amount of work,
// so the three schedulers can be compared on identical, reproducible load.

#include "kernel/types.h"
#include "user/user.h"

#define NCHILD 5
#define BURST  30 // ticks of CPU work per child

static void
spin_ticks(int n)
{
  int start = uptime();
  while (uptime() - start < n) {
    for (volatile int i = 0; i < 200000; i++) {
      ;
    }
  }
}

int
main(void)
{
  int arrival = uptime();
  printf("CMPTEST ARRIVAL %d\n", arrival);

  for (int i = 0; i < NCHILD; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("cmptest: fork failed\n");
      exit(1);
    }
    if (pid == 0) {
      int start = uptime();
      printf("CMPTEST CHILD %d START %d\n", getpid(), start);
      spin_ticks(BURST);
      int end = uptime();
      printf("CMPTEST CHILD %d END %d\n", getpid(), end);
      exit(0);
    }
  }

  // A little while after the hogs are already running and have had a
  // chance to be demoted, one short "interactive-style" job arrives and
  // only needs 2 ticks of CPU. This is the scenario where MLFQ should
  // visibly beat plain round-robin: a fresh process always starts at
  // queue 0, so it should get the CPU quickly even while several
  // CPU-bound hogs are already saturating the system, whereas plain RR
  // treats it exactly the same as every hog already waiting in line.
  spin_ticks(6);
  int short_arrival = uptime();
  int short_pid = fork();
  if (short_pid == 0) {
    int start = uptime();
    printf("CMPTEST SHORT %d ARRIVAL %d START %d\n", getpid(), short_arrival, start);
    spin_ticks(2);
    int end = uptime();
    printf("CMPTEST SHORT %d END %d\n", getpid(), end);
    exit(0);
  }

  for (int i = 0; i < NCHILD + 1; i++) {
    wait(0);
  }

  printf("CMPTEST DONE %d\n", uptime());
  exit(0);
}
