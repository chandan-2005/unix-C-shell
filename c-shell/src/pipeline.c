#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#include "pipeline.h"
#include "redirection.h"
#include "resolver.h"
#include "jobs.h"
#include "term.h"

/* Rebuilds a human-readable command line (for job-control messages like
   "[1] + Stopped    sleep 100") out of the token range [start, end_excl).
   This is a display string only - it is never re-parsed. */
static void build_cmdline(Token *start, Token *end_excl, char *out, size_t outsize) {
    out[0] = '\0';
    size_t used = 0;
    for (Token *t = start; t != NULL && t != end_excl; t = t->next) {
        size_t len = strlen(t->value);
        int need_space = (used > 0);
        if (used + len + (size_t)need_space + 1 >= outsize) break;
        if (need_space) { out[used++] = ' '; }
        memcpy(out + used, t->value, len);
        used += len;
        out[used] = '\0';
    }
}

/* Launches a single command-group (one or more stages separated by '|')
 * found in the token range [start, end_excl). Handles Part C redirection
 * and pipes exactly as before, plus process-group/job-control wiring for
 * Parts D/E: every child is placed in a fresh process group (pgid = pid
 * of the group's first process, per spec), and the group is either
 * waited on in the foreground (with the terminal handed to it) or
 * registered as a background job and left running.
 *
 * Returns 1 if this was a *lone* (non-piped) command that failed to
 * resolve to an executable - the one situation in which, per D1, the
 * caller must stop processing the rest of a ';'-separated sequence.
 */
static int launch_group(Token *start, Token *end_excl, int background) {
    char cmdline[1024];
    build_cmdline(start, end_excl, cmdline, sizeof(cmdline));

    Token *cmd_starts[128];
    int num_cmds = 0;
    cmd_starts[num_cmds++] = start;
    for (Token *p = start; p != NULL && p != end_excl; p = p->next) {
        if (p->type == TOKEN_OP_PIPE) cmd_starts[num_cmds++] = p->next;
    }

    int pipes[128][2];
    for (int i = 0; i < num_cmds - 1; i++) pipe(pipes[i]);

    pid_t pids[128];
    char *names[128];
    int alive[128];
    int tmp_in_fds[128];
    int lone_not_found = 0;
    pid_t pgid = 0;

    for (int i = 0; i < num_cmds; i++) {
        char *args[128]; int argc = 0;
        char *in_files[128]; int in_count = 0;
        char *out_files[128]; int out_modes[128]; int out_count = 0;

        Token *curr = cmd_starts[i];
        while (curr != NULL && curr != end_excl && curr->type != TOKEN_OP_PIPE && argc < 127) {
            if (curr->type == TOKEN_OP_LT && curr->next) {
                in_files[in_count++] = curr->next->value;
                curr = curr->next->next; continue;
            } else if ((curr->type == TOKEN_OP_GT || curr->type == TOKEN_OP_GTGT) && curr->next) {
                out_modes[out_count] = (curr->type == TOKEN_OP_GTGT);
                out_files[out_count++] = curr->next->value;
                curr = curr->next->next; continue;
            } else {
                args[argc++] = curr->value;
            }
            curr = curr->next;
        }
        args[argc] = NULL;

        pids[i] = -1;
        alive[i] = 0;
        tmp_in_fds[i] = -1;

        if (argc == 0) continue;

        tmp_in_fds[i] = setup_input(in_files, in_count, i);
        if (tmp_in_fds[i] == -2) continue;

        if (outreg_validate(out_files, out_modes, out_count) < 0) {
            if (tmp_in_fds[i] >= 0) {
                close(tmp_in_fds[i]);
                char name[64]; snprintf(name, sizeof(name), ".cshell_tmp_in_%d", i); unlink(name);
            }
            continue;
        }

        char *exec_path = resolve_command_path(args[0]);
        if (!exec_path) {
            const char *display_name = (args[0][0] == '%') ? args[0] + 1 : args[0];
            fprintf(stderr, "cshell: command not found (%s)\n", display_name);
            if (tmp_in_fds[i] >= 0) {
                close(tmp_in_fds[i]);
                char name[64]; snprintf(name, sizeof(name), ".cshell_tmp_in_%d", i); unlink(name);
            }
            if (num_cmds == 1) lone_not_found = 1;
            continue;
        }

        pid_t child = fork();
        if (child == 0) {
            /* Children never keep the shell's own job-control dispositions. */
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);

            pid_t mypgid = (i == 0) ? getpid() : pgid;
            setpgid(0, mypgid);

            if (tmp_in_fds[i] >= 0) dup2(tmp_in_fds[i], STDIN_FILENO);
            else if (i > 0) dup2(pipes[i - 1][0], STDIN_FILENO);

            if (out_count > 0) {
                char tmp_out_name[64];
                snprintf(tmp_out_name, sizeof(tmp_out_name), ".cshell_tmp_out_%d", (int)getpid());
                int tmp_out_fd = open(tmp_out_name, O_RDWR | O_CREAT | O_TRUNC, 0600);
                dup2(tmp_out_fd, STDOUT_FILENO);
                close(tmp_out_fd);
            } else if (i < num_cmds - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            for (int p = 0; p < num_cmds - 1; p++) { close(pipes[p][0]); close(pipes[p][1]); }
            if (tmp_in_fds[i] >= 0) close(tmp_in_fds[i]);

            execv(exec_path, args);
            perror("execv failed");
            _exit(EXIT_FAILURE);
        }

        /* Parent */
        if (i == 0) pgid = child;
        setpgid(child, pgid); /* also done by parent, per spec; races with the child's own call are harmless */

        if (tmp_in_fds[i] >= 0) {
            close(tmp_in_fds[i]);
            char name[64]; snprintf(name, sizeof(name), ".cshell_tmp_in_%d", i); unlink(name);
        }
        if (out_count > 0) outreg_register(child, out_files, out_modes, out_count);

        pids[i] = child;
        names[i] = strdup(args[0]);
        alive[i] = 1;
        free(exec_path);
    }

    for (int p = 0; p < num_cmds - 1; p++) { close(pipes[p][0]); close(pipes[p][1]); }

    pid_t live_pids[128];
    char *live_names[128];
    int live_n = 0;
    for (int i = 0; i < num_cmds; i++) {
        if (alive[i]) { live_pids[live_n] = pids[i]; live_names[live_n] = names[i]; live_n++; }
    }

    if (live_n == 0) {
        return lone_not_found;
    }

    if (background) {
        printf("[%d] %d\n", jobs_register(pgid, live_pids, live_names, live_n, cmdline, 1, JOB_RUNNING),
               (int)live_pids[0]);
        fflush(stdout);
    } else {
        term_give_to(pgid);
        int stopped = 0;
        jobs_wait_foreground(live_pids, live_n, &stopped, NULL);
        term_reclaim();

        if (stopped) {
            int num = jobs_register(pgid, live_pids, live_names, live_n, cmdline, 0, JOB_STOPPED);
            printf("[%d] + Stopped    %s\n", num, cmdline);
            fflush(stdout);
        } else {
            for (int i = 0; i < live_n; i++) outreg_finalize(live_pids[i]);
        }
        jobs_flush_notifications();
    }

    for (int i = 0; i < live_n; i++) free(live_names[i]);
    return 0;
}

void execute_pipeline(Token *tokens) {
    Token *group_start = tokens;

    while (group_start != NULL) {
        Token *sep = NULL;
        for (Token *p = group_start; p != NULL; p = p->next) {
            if (p->type == TOKEN_OP_SEMI || p->type == TOKEN_OP_AMP) { sep = p; break; }
        }

        int background = (sep != NULL && sep->type == TOKEN_OP_AMP);
        int lone_not_found = launch_group(group_start, sep, background);

        if (!background && lone_not_found) break; /* D1: stop the ; sequence */
        if (sep == NULL) break;
        group_start = sep->next; /* NULL for a trailing '&' with nothing after (BG -> eps) */
    }
}
