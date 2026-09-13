#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>
#include <signal.h>

#define JOBS_MAX 64
#define JOB_MAX_PROCS 32

typedef enum { JOB_RUNNING, JOB_STOPPED } JobState;

typedef struct {
    int used;
    int job_number;
    pid_t pgid;
    pid_t pids[JOB_MAX_PROCS];
    char *names[JOB_MAX_PROCS];
    int done[JOB_MAX_PROCS];
    int num_procs;
    char *cmdline;
    JobState state;
    int background;
} Job;

/* Lifecycle */
void jobs_init(void);
int  jobs_register(pid_t pgid, pid_t pids[], char *names[], int num_procs,
                    const char *cmdline, int background, JobState state);
void jobs_remove(int job_number);

/* Lookup */
Job *jobs_find_by_number(int job_number);
Job *jobs_find_by_pgid(pid_t pgid);
Job *jobs_find_by_pid(pid_t pid, int *proc_index_out);
int  jobs_has_stopped(void);

/* Display */
void jobs_print_activities(void);

/* Signal machinery */
void jobs_sigchld_handler(int sig);
/* Drains any pending SIGCHLD-reported events and applies them to the job
   table (updating state, queuing completion notifications, removing
   finished background jobs). Call this whenever the shell is idle (not
   inside jobs_wait_foreground), e.g. right after an interrupted getline. */
void jobs_process_pending(void);
/* abort_flag may be NULL. If non-NULL and *abort_flag becomes nonzero
   (e.g. set by a SIGALRM handler) while waiting, the wait returns early
   with whatever partial progress had been made. */
int  jobs_wait_foreground(pid_t pids[], int n, int *stopped_out, volatile sig_atomic_t *abort_flag);
void jobs_flush_notifications(void);

/* Shutdown */
void jobs_sighup_all(void);

#endif
