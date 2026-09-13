#ifndef REDIRECTION_H
#define REDIRECTION_H

#include <sys/types.h>

int setup_input(char *in_files[], int in_count, int cmd_index);

/* Validates every output target can be opened with the right mode and
   applies the truncate/append side effect immediately (matching real
   shell behavior). Prints the spec's error and returns -1 on failure.
   Must be called BEFORE fork()ing the command that owns these targets. */
int outreg_validate(char *out_files[], int out_modes[], int out_count);

/* After a successful fork, remember (keyed by the child's own pid) which
   real files that process's captured stdout must eventually be copied
   into. A no-op if out_count is 0. */
void outreg_register(pid_t pid, char *out_files[], int out_modes[], int out_count);

/* Call once `pid` is known to have exited (foreground or background).
   Copies .cshell_tmp_out_<pid> into every registered real target and
   cleans up. No-op if pid was never registered. */
void outreg_finalize(pid_t pid);

#endif
