#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "resolver.h"
static int is_exe(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode) && (access(path, X_OK) == 0));
}
char* resolve_command_path(const char *cmd) {
    char full_path[8192];
    if (strchr(cmd, '/') != NULL) {
        if (is_exe(cmd)) return strdup(cmd);
        return NULL;
    }
    
    int skip_cwd = (cmd[0] == '%');
    const char *search_cmd = skip_cwd ? cmd + 1 : cmd;
    
    if (!skip_cwd) {
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            snprintf(full_path, sizeof(full_path), "%s/%s", cwd, search_cmd);
            if (is_exe(full_path)) return strdup(full_path);
        }
    }
    char *path_env = getenv("PATH");
    if (path_env) {
        char *path_copy = strdup(path_env);
        char *dir = strtok(path_copy, ":");
        while (dir != NULL) {
            snprintf(full_path, sizeof(full_path), "%s/%s", dir, search_cmd);
            if (is_exe(full_path)) {
                free(path_copy);
                return strdup(full_path);
            }
            dir = strtok(NULL, ":");
        }
        free(path_copy);
    }
    return NULL;
}