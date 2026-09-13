#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include "prompt.h"
#include "lexer.h"
#include "parser.h"
#include "execute.h"
#include "reveal.h"
#include "term.h"
#include "jobs.h"

int main(void) {
    init_homedir();
    term_init();
    jobs_init();

    char *line = NULL;
    size_t len = 0;
    ssize_t nread;
    int eof_warned = 0;

    while (1) {
        /* Reports any background job that finished while we were busy
           running a foreground command (spec: reported only after the
           foreground process terminates), and is also what lets a
           completion get printed right before we redraw the prompt if
           one happened while we were idle (see the EINTR path below). */
        jobs_flush_notifications();
        display_prompt();

        errno = 0;
        nread = getline(&line, &len, stdin);

        if (nread == -1) {
            if (errno == EINTR) {
                /* A signal (SIGINT/SIGTSTP/SIGCHLD) interrupted the read
                   while we were idle at the prompt - not a real EOF.
                   Convert any newly-completed background jobs into
                   queued notifications, then loop around: the top of
                   the loop flushes them and redraws the prompt. */
                jobs_process_pending();
                clearerr(stdin);
                continue;
            }

            /* Real EOF: Ctrl-D on an empty line. */
            if (jobs_has_stopped() && !eof_warned) {
                fprintf(stderr, "cshell: there are stopped jobs\n");
                eof_warned = 1;
                clearerr(stdin);
                continue;
            }

            printf("\n");
            break;
        }

        eof_warned = 0;

        Token *tokens = lex_input(line);
        if (tokens != NULL) {
            if (valid_syntax(tokens)) {
                exec_cmd(tokens);
            }
            free_t(tokens);
        }
    }

    /* Any background or stopped jobs are sent SIGHUP and abandoned
       (never waited on) as the shell exits. */
    jobs_sighup_all();

    free(line);
    return 0;
}
