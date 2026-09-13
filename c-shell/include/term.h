#ifndef TERM_H
#define TERM_H

#include <sys/types.h>

/* Puts the shell in its own process group, takes control of the
   controlling terminal, and installs the SIGINT/SIGTSTP/SIGTTOU/SIGCHLD
   dispositions the shell needs to survive job control. Call once, near
   the very start of main(). */
void term_init(void);

pid_t term_shell_pgid(void);
int   term_fd(void);

/* Give/reclaim the controlling terminal to/from a process group. */
void term_give_to(pid_t pgid);
void term_reclaim(void);

#endif
