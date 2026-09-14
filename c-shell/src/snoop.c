#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "snoop.h"
#include "resolver.h"

/* A (non-exhaustive, per spec's explicit allowance) x86_64 syscall-number
   to name lookup table. Anything not listed here is printed as
   "syscall_N", exactly as the spec requires. */
typedef struct { long num; const char *name; } SyscallName;

static const SyscallName syscall_names[] = {
    {0, "read"}, {1, "write"}, {2, "open"}, {3, "close"}, {4, "stat"},
    {5, "fstat"}, {6, "lstat"}, {7, "poll"}, {8, "lseek"}, {9, "mmap"},
    {10, "mprotect"}, {11, "munmap"}, {12, "brk"}, {13, "rt_sigaction"},
    {14, "rt_sigprocmask"}, {16, "ioctl"}, {17, "pread64"}, {18, "pwrite64"},
    {19, "readv"}, {20, "writev"}, {21, "access"}, {22, "pipe"},
    {23, "select"}, {24, "sched_yield"}, {25, "mremap"}, {28, "madvise"},
    {32, "dup"}, {33, "dup2"}, {34, "pause"}, {35, "nanosleep"},
    {39, "getpid"}, {41, "socket"}, {42, "connect"}, {43, "accept"},
    {44, "sendto"}, {45, "recvfrom"}, {56, "clone"}, {57, "fork"},
    {58, "vfork"}, {59, "execve"}, {60, "exit"}, {61, "wait4"},
    {62, "kill"}, {63, "uname"}, {72, "fcntl"}, {73, "flock"},
    {74, "fsync"}, {79, "getcwd"}, {80, "chdir"}, {82, "rename"},
    {83, "mkdir"}, {84, "rmdir"}, {87, "unlink"}, {89, "readlink"},
    {90, "chmod"}, {92, "chown"}, {95, "umask"}, {96, "gettimeofday"},
    {97, "getrlimit"}, {99, "sysinfo"}, {100, "times"}, {102, "getuid"},
    {104, "getgid"}, {107, "geteuid"}, {108, "getegid"}, {109, "setpgid"},
    {110, "getppid"}, {111, "getpgrp"}, {112, "setsid"}, {131, "sigaltstack"},
    {137, "statfs"}, {158, "arch_prctl"}, {186, "gettid"}, {201, "time"},
    {202, "futex"}, {217, "getdents64"}, {218, "set_tid_address"},
    {228, "clock_gettime"}, {229, "clock_getres"}, {230, "clock_nanosleep"},
    {231, "exit_group"}, {232, "epoll_wait"}, {257, "openat"},
    {262, "newfstatat"}, {273, "set_robust_list"}, {302, "prlimit64"},
    {318, "getrandom"}, {332, "statx"}, {334, "rseq"}, {435, "clone3"},
    {439, "faccessat2"},
};
#define NUM_SYSCALL_NAMES (int)(sizeof(syscall_names) / sizeof(syscall_names[0]))

static const char* lookup_syscall_name(long num, char fallback[32]) {
    for (int i = 0; i < NUM_SYSCALL_NAMES; i++) {
        if (syscall_names[i].num == num) return syscall_names[i].name;
    }
    snprintf(fallback, 32, "syscall_%ld", num);
    return fallback;
}

typedef struct {
    long sysno;
    long long calls;
    double total_time;
    long first_seq;
} SyscallStat;

#define STATS_MAX 512
static SyscallStat stats[STATS_MAX];
static int stats_count = 0;
static long seq_counter = 0;

static void record_call(long sysno, double dt) {
    for (int i = 0; i < stats_count; i++) {
        if (stats[i].sysno == sysno) {
            stats[i].calls++;
            stats[i].total_time += dt;
            return;
        }
    }
    if (stats_count < STATS_MAX) {
        stats[stats_count].sysno = sysno;
        stats[stats_count].calls = 1;
        stats[stats_count].total_time = dt;
        stats[stats_count].first_seq = seq_counter++;
        stats_count++;
    }
}

static double time_diff(struct timespec a, struct timespec b) {
    return (b.tv_sec - a.tv_sec) + (b.tv_nsec - a.tv_nsec) / 1e9;
}

static void print_summary(void) {
    for (int a = 0; a < stats_count; a++) {
        for (int b = a + 1; b < stats_count; b++) {
            int swap = 0;
            if (stats[b].calls > stats[a].calls) swap = 1;
            else if (stats[b].calls == stats[a].calls && stats[b].first_seq < stats[a].first_seq) swap = 1;
            if (swap) { SyscallStat tmp = stats[a]; stats[a] = stats[b]; stats[b] = tmp; }
        }
    }

    printf("%-13s %-7s %s\n", "syscall", "calls", "time");
    for (int i = 0; i < stats_count; i++) {
        char fallback[32];
        const char *name = lookup_syscall_name(stats[i].sysno, fallback);
        printf("%-13s %-7lld %.3fs\n", name, stats[i].calls, stats[i].total_time);
    }
}

static void trace_loop(pid_t child) {
    ptrace(PTRACE_SETOPTIONS, child, 0, (void*)(long)PTRACE_O_TRACESYSGOOD);

    int in_syscall = 0;
    long current_sysno = -1;
    struct timespec entry_time;
    int sig_to_deliver = 0;
    int status;

    while (1) {
        ptrace(PTRACE_SYSCALL, child, NULL, (void*)(long)sig_to_deliver);
        sig_to_deliver = 0;

        if (waitpid(child, &status, 0) < 0) break;
        if (WIFEXITED(status) || WIFSIGNALED(status)) break;

        if (WIFSTOPPED(status)) {
            int sig = WSTOPSIG(status);
            if (sig == (SIGTRAP | 0x80)) {
                struct user_regs_struct regs;
                if (ptrace(PTRACE_GETREGS, child, NULL, &regs) == -1) continue;

                if (!in_syscall) {
                    current_sysno = (long)regs.orig_rax;
                    clock_gettime(CLOCK_MONOTONIC, &entry_time);
                    in_syscall = 1;
                } else {
                    struct timespec exit_time;
                    clock_gettime(CLOCK_MONOTONIC, &exit_time);
                    record_call(current_sysno, time_diff(entry_time, exit_time));
                    in_syscall = 0;
                }
            } else if (sig == SIGTRAP) {
                /* plain PTRACE_SYSCALL-related trap without the syscall
                   marker bit (can happen around exec) - ignore. */
            } else {
                sig_to_deliver = sig;
            }
        }
    }
}

void run_snoop(Token *tokens) {
    Token *t = tokens->next;
    if (!t) { fprintf(stderr, "snoop: command not found\n"); return; }

    stats_count = 0;
    seq_counter = 0;

    if (strcmp(t->value, "-p") == 0) {
        Token *pid_tok = t->next;
        if (!pid_tok) { fprintf(stderr, "snoop: no such process\n"); return; }

        char *endp;
        long pidval = strtol(pid_tok->value, &endp, 10);
        if (*endp != '\0' || pid_tok->value[0] == '\0') {
            fprintf(stderr, "snoop: no such process\n");
            return;
        }
        pid_t target = (pid_t)pidval;

        if (ptrace(PTRACE_ATTACH, target, NULL, NULL) == -1) {
            fprintf(stderr, "snoop: no such process\n");
            return;
        }
        int status;
        waitpid(target, &status, 0);

        trace_loop(target);
        print_summary();
        return;
    }

    char *args[128];
    int argc = 0;
    for (Token *p = t; p != NULL && argc < 127; p = p->next) args[argc++] = p->value;
    args[argc] = NULL;

    char *exec_path = resolve_command_path(args[0]);
    if (!exec_path) {
        fprintf(stderr, "snoop: command not found\n");
        return;
    }

    pid_t child = fork();
    if (child == 0) {
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        execv(exec_path, args);
        _exit(127);
    }
    free(exec_path);

    int status;
    waitpid(child, &status, 0); /* initial SIGTRAP right after execve */
    if (WIFEXITED(status)) return;

    trace_loop(child);
    print_summary();
}
