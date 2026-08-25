#include <stdio.h>
#include <string.h>
#include "execute.h"
#include "hop.h"
#include "reveal.h"
#include "locate.h"
#include "peek.h"
#include "pipeline.h"

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
    execute_pipeline(tokens);
}