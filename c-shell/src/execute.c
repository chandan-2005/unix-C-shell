#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

#include "execute.h"
#include "hop.h"
#include "reveal.h"
#include "locate.h"
#include "peek.h"
#include "pipeline.h"
#include "activities.h"
#include "resume.h"
#include "ping.h"
#include "spy.h"
#include "snoop.h"
#include "redirection.h"

/* Per Q22/Q39 of the doubt doc, builtins (Part B intrinsics, plus the
 * Part E/F ones) are just another kind of CMD in the grammar and must
 * support the same redirection as any external command. Builtins run
 * directly in the shell's own process though (no fork - that's what
 * lets `hop` actually change the shell's cwd), so redirection here means
 * temporarily pointing the shell's own stdin/stdout at the right places,
 * running the builtin, then putting them back.
 *
 * strip_redirections() walks the token list once, pulls every
 * `< file` / `> file` / `>> file` pair out of it (freeing those nodes),
 * and hands the caller plain filename+mode arrays - so the builtin
 * itself only ever sees its real arguments, exactly as if redirection
 * had never been typed.
 */
static void strip_redirections(Token *tokens,
                                char *in_files[], int *in_count,
                                char *out_files[], int out_modes[], int *out_count) {
    Token *prev = tokens;
    Token *curr = tokens->next;

    while (curr != NULL) {
        int is_redir = (curr->type == TOKEN_OP_LT ||
                         curr->type == TOKEN_OP_GT ||
                         curr->type == TOKEN_OP_GTGT);

        if (is_redir && curr->next != NULL && curr->next->type == TOKEN_WORD) {
            Token *op = curr;
            Token *word = curr->next;
            char *fname = strdup(word->value);

            if (op->type == TOKEN_OP_LT) {
                in_files[*in_count] = fname;
                (*in_count)++;
            } else {
                out_modes[*out_count] = (op->type == TOKEN_OP_GTGT);
                out_files[*out_count] = fname;
                (*out_count)++;
            }

            Token *after = word->next;
            prev->next = after;
            free(op->value);
            free(op);
            free(word->value);
            free(word);
            curr = after;
        } else {
            prev = curr;
            curr = curr->next;
        }
    }
}

static void run_builtin(Token *tokens) {
    if (strcmp(tokens->value, "hop") == 0) run_hop(tokens);
    else if (strcmp(tokens->value, "reveal") == 0) run_reveal(tokens);
    else if (strcmp(tokens->value, "locate") == 0) run_locate(tokens);
    else if (strcmp(tokens->value, "peek") == 0) run_peek(tokens);
    else if (strcmp(tokens->value, "activities") == 0) run_activities(tokens);
    else if (strcmp(tokens->value, "resume") == 0) run_resume(tokens);
    else if (strcmp(tokens->value, "ping") == 0) run_ping(tokens);
    else if (strcmp(tokens->value, "spy") == 0) run_spy(tokens);
    else if (strcmp(tokens->value, "snoop") == 0) run_snoop(tokens);
}

static int is_builtin_name(const char *name) {
    return strcmp(name, "hop") == 0 || strcmp(name, "reveal") == 0 ||
           strcmp(name, "locate") == 0 || strcmp(name, "peek") == 0 ||
           strcmp(name, "activities") == 0 || strcmp(name, "resume") == 0 ||
           strcmp(name, "ping") == 0 || strcmp(name, "spy") == 0 ||
           strcmp(name, "snoop") == 0;
}

void exec_cmd(Token *tokens) {
    if (!tokens) return;

    if (!is_builtin_name(tokens->value)) {
        execute_pipeline(tokens);
        return;
    }

    char *in_files[16];
    int in_count = 0;
    char *out_files[16];
    int out_modes[16];
    int out_count = 0;

    strip_redirections(tokens, in_files, &in_count, out_files, out_modes, &out_count);

    int saved_stdin = -1;
    int saved_stdout = -1;
    int tmp_out_fd = -1;
    int ok = 1;

    if (in_count > 0) {
        int tmp_in = setup_input(in_files, in_count, 9001);
        if (tmp_in < 0) {
            ok = 0; /* setup_input already printed the right error */
        } else {
            saved_stdin = dup(STDIN_FILENO);
            dup2(tmp_in, STDIN_FILENO);
            close(tmp_in);
            unlink(".cshell_tmp_in_9001");
        }
    }

    char tmp_out_name[64];
    if (ok && out_count > 0) {
        if (outreg_validate(out_files, out_modes, out_count) < 0) {
            ok = 0;
        } else {
            snprintf(tmp_out_name, sizeof(tmp_out_name), ".cshell_tmp_out_builtin_%d", (int)getpid());
            tmp_out_fd = open(tmp_out_name, O_RDWR | O_CREAT | O_TRUNC, 0600);
            saved_stdout = dup(STDOUT_FILENO);
            dup2(tmp_out_fd, STDOUT_FILENO);
        }
    }

    if (ok) {
        run_builtin(tokens);
        fflush(stdout);
    }

    if (saved_stdout >= 0) {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
        close(tmp_out_fd);

        int out_fds[16];
        for (int j = 0; j < out_count; j++) {
            int flags = O_WRONLY | O_CREAT | O_APPEND; /* truncation already
                                                            happened inside outreg_validate() */
            out_fds[j] = open(out_files[j], flags, 0644);
        }
        int rfd = open(tmp_out_name, O_RDONLY);
        if (rfd >= 0) {
            char buf[4096];
            ssize_t n;
            while ((n = read(rfd, buf, sizeof(buf))) > 0) {
                for (int j = 0; j < out_count; j++) {
                    if (out_fds[j] >= 0) write(out_fds[j], buf, n);
                }
            }
            close(rfd);
        }
        unlink(tmp_out_name);
        for (int j = 0; j < out_count; j++) {
            if (out_fds[j] >= 0) close(out_fds[j]);
        }
    }
    if (saved_stdin >= 0) {
        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);
    }

    for (int i = 0; i < in_count; i++) free(in_files[i]);
    for (int i = 0; i < out_count; i++) free(out_files[i]);
}
