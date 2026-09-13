#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "jobs.h"
#include "redirection.h"

static Job jobs[JOBS_MAX];
static int next_job_number = 1;

/* --- Async-signal-safe pending event queue, filled only by the SIGCHLD
   handler. Draining/interpreting it happens outside signal context. --- */
#define PENDING_MAX 256
static volatile sig_atomic_t pending_count = 0;
static pid_t pending_pid[PENDING_MAX];
static int   pending_status[PENDING_MAX];

/* --- Deferred text notifications (e.g. "sleep with pid 123 exited
   normally"). Printed only from a safe point in the main loop, never
   from inside the signal handler itself. --- */
#define NOTIF_MAX 64
static char notif_queue[NOTIF_MAX][256];
static int notif_count = 0;

static void queue_notification(const char *msg) {
    if (notif_count < NOTIF_MAX) {
        strncpy(notif_queue[notif_count], msg, sizeof(notif_queue[0]) - 1);
        notif_queue[notif_count][sizeof(notif_queue[0]) - 1] = '\0';
        notif_count++;
    }
}

void jobs_flush_notifications(void) {
    for (int i = 0; i < notif_count; i++) {
        printf("%s\n", notif_queue[i]);
    }
    notif_count = 0;
    fflush(stdout);
}

void jobs_init(void) {
    memset(jobs, 0, sizeof(jobs));
    next_job_number = 1;
    pending_count = 0;
    notif_count = 0;
}

int jobs_register(pid_t pgid, pid_t pids[], char *names[], int num_procs,
                   const char *cmdline, int background, JobState state) {
    int slot = -1;
    for (int i = 0; i < JOBS_MAX; i++) {
        if (!jobs[i].used) { slot = i; break; }
    }
    if (slot < 0) return -1;

    Job *j = &jobs[slot];
    memset(j, 0, sizeof(*j));
    j->used = 1;
    j->job_number = next_job_number++;
    j->pgid = pgid;
    j->num_procs = (num_procs > JOB_MAX_PROCS) ? JOB_MAX_PROCS : num_procs;
    for (int i = 0; i < j->num_procs; i++) {
        j->pids[i] = pids[i];
        j->names[i] = strdup(names[i] ? names[i] : "?");
        j->done[i] = 0;
    }
    j->cmdline = strdup(cmdline ? cmdline : "");
    j->state = state;
    j->background = background;
    return j->job_number;
}

void jobs_remove(int job_number) {
    for (int i = 0; i < JOBS_MAX; i++) {
        if (jobs[i].used && jobs[i].job_number == job_number) {
            for (int p = 0; p < jobs[i].num_procs; p++) free(jobs[i].names[p]);
            free(jobs[i].cmdline);
            memset(&jobs[i], 0, sizeof(jobs[i]));
            return;
        }
    }
}

Job *jobs_find_by_number(int job_number) {
    for (int i = 0; i < JOBS_MAX; i++) {
        if (jobs[i].used && jobs[i].job_number == job_number) return &jobs[i];
    }
    return NULL;
}

Job *jobs_find_by_pgid(pid_t pgid) {
    for (int i = 0; i < JOBS_MAX; i++) {
        if (jobs[i].used && jobs[i].pgid == pgid) return &jobs[i];
    }
    return NULL;
}

Job *jobs_find_by_pid(pid_t pid, int *proc_index_out) {
    for (int i = 0; i < JOBS_MAX; i++) {
        if (!jobs[i].used) continue;
        for (int p = 0; p < jobs[i].num_procs; p++) {
            if (jobs[i].pids[p] == pid) {
                if (proc_index_out) *proc_index_out = p;
                return &jobs[i];
            }
        }
    }
    return NULL;
}

int jobs_has_stopped(void) {
    for (int i = 0; i < JOBS_MAX; i++) {
        if (jobs[i].used && jobs[i].state == JOB_STOPPED) return 1;
    }
    return 0;
}

/* Oldest-launched-first: job_number increases monotonically, so a plain
   ascending scan over job_number already yields launch order. */
void jobs_print_activities(void) {
    int numbers[JOBS_MAX];
    int count = 0;
    for (int i = 0; i < JOBS_MAX; i++) {
        if (jobs[i].used) numbers[count++] = jobs[i].job_number;
    }
    for (int a = 0; a < count; a++) {
        for (int b = a + 1; b < count; b++) {
            if (numbers[b] < numbers[a]) { int t = numbers[a]; numbers[a] = numbers[b]; numbers[b] = t; }
        }
    }
    for (int k = 0; k < count; k++) {
        Job *j = jobs_find_by_number(numbers[k]);
        if (!j) continue;
        printf("[%d] pgid %d\n", j->job_number, (int)j->pgid);
        for (int p = 0; p < j->num_procs; p++) {
            if (j->done[p]) continue;
            const char *state = (j->state == JOB_STOPPED) ? "Stopped" : "Running";
            printf("  %d %s %s\n", (int)j->pids[p], j->names[p], state);
        }
    }
    fflush(stdout);
}

void jobs_sigchld_handler(int sig) {
    (void)sig;
    int saved_errno = errno;
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        if (pending_count < PENDING_MAX) {
            pending_pid[pending_count] = pid;
            pending_status[pending_count] = status;
            pending_count++;
        }
    }
    errno = saved_errno;
}

static int take_pending(pid_t *pid_out, int *status_out) {
    if (pending_count == 0) return 0;
    *pid_out = pending_pid[0];
    *status_out = pending_status[0];
    for (int i = 1; i < pending_count; i++) {
        pending_pid[i - 1] = pending_pid[i];
        pending_status[i - 1] = pending_status[i];
    }
    pending_count--;
    return 1;
}

/* Handle a wait-status change for a pid that belongs to some tracked job
   (not the job currently being foreground-waited on by the caller, if
   any). Updates the job table and, once every process in a job has
   truly exited, queues its completion message and (for backgrounded
   jobs only, per spec) removes the job. Stopped jobs stay in the table
   so `activities`/`resume` can still see them. */
static void handle_tracked_event(pid_t pid, int status) {
    outreg_finalize(pid);

    int idx;
    Job *j = jobs_find_by_pid(pid, &idx);
    if (!j) return;

    if (WIFSTOPPED(status)) {
        j->state = JOB_STOPPED;
        return;
    }

    if (WIFCONTINUED(status)) {
        j->state = JOB_RUNNING;
        return;
    }

    /* Exited or signaled: this one process is done. */
    j->done[idx] = 1;

    int all_done = 1;
    for (int p = 0; p < j->num_procs; p++) {
        if (!j->done[p]) { all_done = 0; break; }
    }

    if (all_done && j->background) {
        char msg[256];
        if (WIFSIGNALED(status)) {
            snprintf(msg, sizeof(msg), "%s with pid %d exited abnormally",
                     j->names[0], (int)j->pids[0]);
        } else {
            snprintf(msg, sizeof(msg), "%s with pid %d exited normally",
                     j->names[0], (int)j->pids[0]);
        }
        queue_notification(msg);
        jobs_remove(j->job_number);
    } else if (all_done) {
        /* A foreground job that was previously tracked (e.g. it had been
           stopped, resumed, and now finally finished) - just drop it. */
        jobs_remove(j->job_number);
    }
}

void jobs_process_pending(void) {
    sigset_t block_set, old_set;
    sigemptyset(&block_set);
    sigaddset(&block_set, SIGCHLD);
    sigprocmask(SIG_BLOCK, &block_set, &old_set);

    pid_t pid;
    int status;
    while (take_pending(&pid, &status)) {
        handle_tracked_event(pid, status);
    }

    sigprocmask(SIG_SETMASK, &old_set, NULL);
}

int jobs_wait_foreground(pid_t pids[], int n, int *stopped_out, volatile sig_atomic_t *abort_flag) {
    sigset_t block_set, old_set;
    sigemptyset(&block_set);
    sigaddset(&block_set, SIGCHLD);
    sigprocmask(SIG_BLOCK, &block_set, &old_set);

    int done_local[JOB_MAX_PROCS] = {0};
    int done_count = 0;
    int stopped = 0;

    while (done_count < n && !stopped) {
        if (abort_flag && *abort_flag) break;

        pid_t pid;
        int status;
        if (!take_pending(&pid, &status)) {
            sigsuspend(&old_set);
            continue;
        }

        int idx = -1;
        for (int i = 0; i < n; i++) {
            if (pids[i] == pid && !done_local[i]) { idx = i; break; }
        }

        if (idx >= 0) {
            if (WIFSTOPPED(status)) {
                stopped = 1;
            } else if (WIFCONTINUED(status)) {
                /* shouldn't normally happen mid-foreground-wait */
                continue;
            } else {
                outreg_finalize(pid);
                done_local[idx] = 1;
                done_count++;
            }
        } else {
            handle_tracked_event(pid, status);
        }
    }

    *stopped_out = stopped;
    sigprocmask(SIG_SETMASK, &old_set, NULL);
    return done_count;
}

void jobs_sighup_all(void) {
    for (int i = 0; i < JOBS_MAX; i++) {
        if (jobs[i].used) {
            kill(-jobs[i].pgid, SIGHUP);
        }
    }
}
