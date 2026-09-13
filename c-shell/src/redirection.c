#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include "redirection.h"

int setup_input(char *in_files[], int in_count, int cmd_index) {
    if (in_count == 0) return -1;

    char name[64];
    snprintf(name, sizeof(name), ".cshell_tmp_in_%d", cmd_index);

    int tmp_fd = open(name, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (tmp_fd < 0) { perror("cshell: tmpfile failed"); return -1; }

    for (int i = 0; i < in_count; i++) {
        int fd = open(in_files[i], O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "cshell: no such file or directory\n");
            close(tmp_fd);
            unlink(name);
            return -2;
        }
        char buf[4096];
        ssize_t bytes;
        while ((bytes = read(fd, buf, sizeof(buf))) > 0) write(tmp_fd, buf, bytes);
        close(fd);
    }
    lseek(tmp_fd, 0, SEEK_SET);
    return tmp_fd;
}

/* --- Output registry ---
 * Each redirected process writes its own stdout into a temp file named
 * after its own pid (.cshell_tmp_out_<pid>) rather than directly into the
 * real target(s). That's necessary because a single process can only
 * have one thing living at fd 1 at a time, but the spec allows a command
 * to fan its output out to several real files (`cmd > a > b`).
 *
 * outreg_validate() runs before fork(): it opens (and, per spec,
 * truncates/creates) every real target once up front so a bad target
 * aborts the command before it ever runs. outreg_register() then just
 * remembers the filenames+modes against the child's pid. Once that pid
 * is known to have exited (checked by both the foreground waiter and the
 * SIGCHLD path, so this works for background jobs too), outreg_finalize()
 * copies the temp file into every real target (using O_APPEND, since any
 * needed truncation already happened at validate time) and cleans up.
 */
#define OUTREG_MAX 128
#define OUTREG_FILES_MAX 16

typedef struct {
    int used;
    pid_t pid;
    int out_count;
    char *out_files[OUTREG_FILES_MAX];
} OutSpec;

static OutSpec registry[OUTREG_MAX];

int outreg_validate(char *out_files[], int out_modes[], int out_count) {
    int fds[OUTREG_FILES_MAX];
    for (int i = 0; i < out_count; i++) {
        int flags = O_WRONLY | O_CREAT | (out_modes[i] ? O_APPEND : O_TRUNC);
        fds[i] = open(out_files[i], flags, 0644);
        if (fds[i] < 0) {
            fprintf(stderr, "cshell: unable to create file for writing\n");
            for (int j = 0; j < i; j++) close(fds[j]);
            return -1;
        }
    }
    for (int i = 0; i < out_count; i++) close(fds[i]);
    return 0;
}

void outreg_register(pid_t pid, char *out_files[], int out_modes[], int out_count) {
    (void)out_modes;
    if (out_count <= 0) return;
    int slot = -1;
    for (int i = 0; i < OUTREG_MAX; i++) {
        if (!registry[i].used) { slot = i; break; }
    }
    if (slot < 0) return;

    OutSpec *s = &registry[slot];
    s->used = 1;
    s->pid = pid;
    s->out_count = (out_count > OUTREG_FILES_MAX) ? OUTREG_FILES_MAX : out_count;
    for (int i = 0; i < s->out_count; i++) s->out_files[i] = strdup(out_files[i]);
}

void outreg_finalize(pid_t pid) {
    for (int i = 0; i < OUTREG_MAX; i++) {
        if (!registry[i].used || registry[i].pid != pid) continue;

        OutSpec *s = &registry[i];
        char tmpname[64];
        snprintf(tmpname, sizeof(tmpname), ".cshell_tmp_out_%d", (int)pid);

        int out_fds[OUTREG_FILES_MAX];
        for (int j = 0; j < s->out_count; j++) {
            out_fds[j] = open(s->out_files[j], O_WRONLY | O_CREAT | O_APPEND, 0644);
        }

        int tmp_fd = open(tmpname, O_RDONLY);
        if (tmp_fd >= 0) {
            char buf[4096];
            ssize_t bytes;
            while ((bytes = read(tmp_fd, buf, sizeof(buf))) > 0) {
                for (int j = 0; j < s->out_count; j++) {
                    if (out_fds[j] >= 0) write(out_fds[j], buf, bytes);
                }
            }
            close(tmp_fd);
        }
        unlink(tmpname);

        for (int j = 0; j < s->out_count; j++) {
            if (out_fds[j] >= 0) close(out_fds[j]);
            free(s->out_files[j]);
        }
        s->used = 0;
        return;
    }
}
