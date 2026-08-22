#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "execute.h"
#include "prompt.h"
#include "reveal.h"
#include "locate.h"
#include "peek.h"
#include "external.h"
static char prev_dir[PATH_MAX] = "";

void exec_cmd(Token *tokens) {
    if (!tokens) return;

    if (strcmp(tokens->value, "hop") == 0) {
        const char *target = NULL;
        if (!tokens->next || strcmp(tokens->next->value, "~") == 0) {
            target = get_shell_home();
        } 
        else if (strcmp(tokens->next->value, "-") == 0) {
            if (prev_dir[0] == '\0') {
                fprintf(stderr, "hop:no previous directory\n");
                return;
            }
            target = prev_dir;
        } 
        else {
            target = tokens->next->value;
        }
        
        char curr_dir[PATH_MAX];
        getcwd(curr_dir, sizeof(curr_dir));
        
        if (chdir(target) != 0) {
            perror("hop failed");
        } else {
            strncpy(prev_dir, curr_dir, PATH_MAX);
            
            char new_dir[PATH_MAX];
            getcwd(new_dir, sizeof(new_dir));
            printf("%s\n", new_dir);
        }
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
    run_external(tokens);
    printf("Command '%s' recognized\n", tokens->value);
}