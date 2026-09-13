#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "ping.h"
#include "jobs.h"

void run_ping(Token *tokens) {
    Token *t1 = tokens->next;
    if (!t1) { fprintf(stderr, "ping: invalid syntax\n"); return; }
    Token *t2 = t1->next;
    if (!t2 || t2->next != NULL) { fprintf(stderr, "ping: invalid syntax\n"); return; }

    const char *target_str = t1->value;
    const char *sig_str = t2->value;

    if (sig_str[0] == '\0') { fprintf(stderr, "ping: invalid syntax\n"); return; }
    char *endp;
    long sig_val = strtol(sig_str, &endp, 10);
    if (*endp != '\0' || sig_val < 0) {
        fprintf(stderr, "ping: invalid syntax\n");
        return;
    }
    int actual_sig = (int)(sig_val % 64);

    if (target_str[0] == '%') {
        if (target_str[1] == '\0') { fprintf(stderr, "ping: no such process found\n"); return; }
        char *endp2;
        long jn = strtol(target_str + 1, &endp2, 10);
        if (*endp2 != '\0') { fprintf(stderr, "ping: no such process found\n"); return; }

        Job *j = jobs_find_by_number((int)jn);
        if (!j) { fprintf(stderr, "ping: no such process found\n"); return; }

        kill(-j->pgid, actual_sig);
        printf("Sent signal %s to %s\n", sig_str, target_str);
    } else {
        char *endp2;
        long pidval = strtol(target_str, &endp2, 10);
        if (*endp2 != '\0') { fprintf(stderr, "ping: no such process found\n"); return; }

        int idx;
        Job *j = jobs_find_by_pid((pid_t)pidval, &idx);
        if (!j) { fprintf(stderr, "ping: no such process found\n"); return; }

        kill((pid_t)pidval, actual_sig);
        printf("Sent signal %s to %s\n", sig_str, target_str);
    }
    fflush(stdout);
}
