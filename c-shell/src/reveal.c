#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "reveal.h"
#include "prompt.h"

void run_reveal(Token *tokens) {
    int flag_a = 0;
    int flag_l = 0;
    const char *path = "."; 
    Token *curr = tokens->next; 
    while (curr != NULL) {
        if (curr->value[0] == '-' && strlen(curr->value) > 1) {
            for (int i = 1; curr->value[i] != '\0'; i++) {
                if (curr->value[i] == 'a') flag_a = 1;
                else if (curr->value[i] == 'l') flag_l = 1;
                else {
                    fprintf(stderr, "reveal: invalid flag -- '%c'\n", curr->value[i]);
                    return;
                }
            }
        } else {
            if (strcmp(curr->value, "~") == 0) {
                path = get_shell_home();
            } else {
                path = curr->value;
            }
        }
        curr = curr->next;
    }
    DIR *dir = opendir(path);
    if (dir == NULL) {
        perror("reveal");
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (!flag_a && entry->d_name[0] == '.') {
            continue;
        }
        // b
        printf("%s\n", entry->d_name);
    }

    closedir(dir);
}