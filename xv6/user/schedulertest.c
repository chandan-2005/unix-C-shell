// schedulertest: a small user-space workload for exercising and
// visualizing the MLFQ scheduler (mini-project Part 2, section 2.3.2).
//
// It forks a handful of children with different CPU-burst patterns:
//   - two purely CPU-bound children that never voluntarily give up the
//     CPU (so they should get demoted through the queues and only come
//     back to queue 0 via the periodic priority boost)
//   - two I/O-bound children that do a tiny bit of work and then call
//     pause() (a voluntary blocking "sleep"), so per the MLFQ spec they
//     should keep returning to the same queue instead of being demoted
//   - each child prints "<tick> <pid> <priority>" once per iteration via
//     the getmlfq() syscall, so the whole run's output is a ready-made
//     CSV (space separated) for the timeline scatter plot required by
//     the report - see plot_mlfq.py, which expects exactly this format.
//
// Usage: schedulertest [total_ticks]  (default 200)

#include "kernel/types.h"
#include "user/user.h"

static void
spin_ticks(int n)
{
  int start = uptime();
  while (uptime() - start < n) {
    for (volatile int i = 0; i < 200000; i++) {
      ; // burn CPU without ever blocking
    }
  }
}

static void
cpu_bound_child(int total_ticks)
{
  int pid = getpid();
  int start = uptime();
  while (uptime() - start < total_ticks) {
    printf("%d %d %d\n", uptime(), pid, getmlfq(pid));
    spin_ticks(2);
  }
  exit(0);
}

static void
io_bound_child(int total_ticks)
{
  int pid = getpid();
  int start = uptime();
  while (uptime() - start < total_ticks) {
    printf("%d %d %d\n", uptime(), pid, getmlfq(pid));
    spin_ticks(1); // a small burst of real work
    pause(3);      // then voluntarily block, like waiting on I/O
  }
  exit(0);
}

int
main(int argc, char *argv[])
{
  int total_ticks = 200;
  if (argc > 1) {
    total_ticks = atoi(argv[1]);
  }

  printf("schedulertest: start at tick %d, running for %d ticks\n",
         uptime(), total_ticks);

  int pid;

  // Two CPU-bound workers.
  for (int i = 0; i < 2; i++) {
    pid = fork();
    if (pid < 0) {
      printf("schedulertest: fork failed\n");
      exit(1);
    }
    if (pid == 0) {
      cpu_bound_child(total_ticks);
    }
  }

  // Two I/O-bound workers.
  for (int i = 0; i < 2; i++) {
    pid = fork();
    if (pid < 0) {
      printf("schedulertest: fork failed\n");
      exit(1);
    }
    if (pid == 0) {
      io_bound_child(total_ticks);
    }
  }

  for (int i = 0; i < 4; i++) {
    wait(0);
  }

  printf("schedulertest: done at tick %d\n", uptime());
  exit(0);
}
