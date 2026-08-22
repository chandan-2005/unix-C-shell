#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "locate.h"

static int is_executable(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode) && (access(path, X_OK) == 0)) {
        return 1;
    }
    return 0;
}

void run_locate(Token *tokens) {
    Token *curr = tokens->next; 

    if (curr == NULL) {
        fprintf(stderr, "locate: invalid syntax\n");
        return;
    }
    while (curr != NULL) {
        int found = 0;
        char *cmd = curr->value;
        char full_path[4096];
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            snprintf(full_path, sizeof(full_path), "%s/%s", cwd, cmd);
            if (is_executable(full_path)) {
                printf("%s\n", full_path);
                found = 1;
            }
        }
        char *path_env = getenv("PATH");
        if (path_env != NULL) {
            char *path_copy = strdup(path_env); 
            char *dir = strtok(path_copy, ":");
            while (dir != NULL) {
                snprintf(full_path, sizeof(full_path), "%s/%s", dir, cmd);  
                if (is_executable(full_path)) {
                    printf("%s\n", full_path);
                    found = 1;
                }
                dir = strtok(NULL, ":");
            }
            free(path_copy);
        }
        if (!found) {
            fprintf(stderr, "locate: command not found (%s)\n", cmd);
        }
        curr = curr->next;
    }
}