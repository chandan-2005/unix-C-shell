#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include "term.h"
#include "jobs.h"

static pid_t shell_pgid;
static int shell_terminal = STDIN_FILENO;

/* SIGINT/SIGTSTP reaching the shell itself (i.e. it currently owns the
   terminal because no foreground job is running) must not kill or stop
   it. These handlers do nothing but exist purely so the disposition
   isn't SIG_DFL - and, just as usefully, having no SA_RESTART means the
   blocking getline() in main() is interrupted (EINTR) so the shell can
   redraw its prompt right away instead of appearing to hang. */
static void noop_handler(int sig) {
    (void)sig;
}

void term_init(void) {
    shell_pgid = getpid();
    /* Put the shell in its own process group (harmless if it already is
       one, e.g. because it's already a session/group leader). */
    setpgid(shell_pgid, shell_pgid);
    tcsetpgrp(shell_terminal, shell_pgid);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = noop_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; /* deliberately no SA_RESTART */
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);

    /* The shell may not be the foreground process group at the instant it
       calls tcsetpgrp() to reclaim the terminal; ignoring SIGTTOU stops
       that call (and any stray terminal write) from stopping the shell. */
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);

    struct sigaction sc;
    memset(&sc, 0, sizeof(sc));
    sc.sa_handler = jobs_sigchld_handler;
    sigemptyset(&sc.sa_mask);
    sc.sa_flags = 0; /* no SA_RESTART: lets an idle getline() wake up */
    sigaction(SIGCHLD, &sc, NULL);
}

pid_t term_shell_pgid(void) { return shell_pgid; }
int term_fd(void) { return shell_terminal; }

void term_give_to(pid_t pgid) {
    tcsetpgrp(shell_terminal, pgid);
}

void term_reclaim(void) {
    tcsetpgrp(shell_terminal, shell_pgid);
}
