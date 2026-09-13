#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "resume.h"
#include "jobs.h"
#include "term.h"
#include "redirection.h"

static volatile sig_atomic_t g_timed_out = 0;

static void alarm_handler(int sig) {
    (void)sig;
    g_timed_out = 1;
}

void run_resume(Token *tokens) {
    Token *t = tokens->next;

    if (!t || t->value[0] != '%' || t->value[1] == '\0') {
        fprintf(stderr, "resume: invalid syntax\n");
        return;
    }
    char *endp;
    long job_number = strtol(t->value + 1, &endp, 10);
    if (*endp != '\0') {
        fprintf(stderr, "resume: invalid syntax\n");
        return;
    }

    t = t->next;
    if (!t) { fprintf(stderr, "resume: invalid syntax\n"); return; }

    int is_fg;
    if (strcmp(t->value, "fg") == 0) is_fg = 1;
    else if (strcmp(t->value, "bg") == 0) is_fg = 0;
    else { fprintf(stderr, "resume: invalid syntax\n"); return; }

    long timeout = -1;
    t = t->next;
    if (t) {
        if (!is_fg || strcmp(t->value, "--timeout") != 0) {
            fprintf(stderr, "resume: invalid syntax\n");
            return;
        }
        t = t->next;
        if (!t) { fprintf(stderr, "resume: invalid syntax\n"); return; }
        char *endp2;
        timeout = strtol(t->value, &endp2, 10);
        if (*endp2 != '\0' || t->value[0] == '\0' || timeout < 0) {
            fprintf(stderr, "resume: invalid syntax\n");
            return;
        }
        t = t->next;
        if (t) { fprintf(stderr, "resume: invalid syntax\n"); return; }
    }

    Job *job = jobs_find_by_number((int)job_number);
    if (!job) {
        fprintf(stderr, "resume: no such job\n");
        return;
    }

    kill(-job->pgid, SIGCONT);
    job->state = JOB_RUNNING;

    if (!is_fg) {
        printf("[%d] + Running    %s\n", job->job_number, job->cmdline);
        fflush(stdout);
        return;
    }

    /* Foreground: print the command line, hand over the terminal, then
       wait exactly like a fresh foreground launch would. */
    printf("%s\n", job->cmdline);
    fflush(stdout);

    pid_t pids[JOB_MAX_PROCS];
    int n = job->num_procs;
    for (int i = 0; i < n; i++) pids[i] = job->pids[i];
    int number = job->job_number;
    pid_t pgid = job->pgid;
    char cmdline_copy[1024];
    strncpy(cmdline_copy, job->cmdline, sizeof(cmdline_copy) - 1);
    cmdline_copy[sizeof(cmdline_copy) - 1] = '\0';

    term_give_to(pgid);

    if (timeout >= 0) {
        g_timed_out = 0;
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = alarm_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGALRM, &sa, NULL);
        alarm((unsigned int)timeout);
    }

    int stopped = 0;
    int done = jobs_wait_foreground(pids, n, &stopped, timeout >= 0 ? &g_timed_out : NULL);

    if (timeout >= 0) {
        alarm(0);
        signal(SIGALRM, SIG_DFL);
    }

    term_reclaim();

    if (timeout >= 0 && g_timed_out && !stopped && done < n) {
        kill(-pgid, SIGTERM);
        printf("resume: job timed out\n");
        jobs_remove(number);
    } else if (stopped) {
        Job *j2 = jobs_find_by_number(number);
        if (j2) j2->state = JOB_STOPPED;
        printf("[%d] + Stopped    %s\n", number, cmdline_copy);
    } else {
        for (int i = 0; i < n; i++) outreg_finalize(pids[i]);
        jobs_remove(number);
    }
    fflush(stdout);
    jobs_flush_notifications();
}
