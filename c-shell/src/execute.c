#include <stdio.h>
#include <string.h>
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

void exec_cmd(Token *tokens) {
    if (!tokens) return;

    if (strcmp(tokens->value, "hop") == 0) {
        run_hop(tokens);
        return;
    }
    else if (strcmp(tokens->value, "reveal") == 0) {
        run_reveal(tokens);
        return;
    }
    else if (strcmp(tokens->value, "locate") == 0) {
        run_locate(tokens);
        return;
    }
    else if (strcmp(tokens->value, "peek") == 0) {
        run_peek(tokens);
        return;
    }
    else if (strcmp(tokens->value, "activities") == 0) {
        run_activities(tokens);
        return;
    }
    else if (strcmp(tokens->value, "resume") == 0) {
        run_resume(tokens);
        return;
    }
    else if (strcmp(tokens->value, "ping") == 0) {
        run_ping(tokens);
        return;
    }
    else if (strcmp(tokens->value, "spy") == 0) {
        run_spy(tokens);
        return;
    }
    else if (strcmp(tokens->value, "snoop") == 0) {
        run_snoop(tokens);
        return;
    }
    execute_pipeline(tokens);
}
