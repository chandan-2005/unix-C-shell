#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <limits.h>
#include <pwd.h>
#include "prompt.h"

static char shell_home[PATH_MAX] = "";
void init_homedir() {
    if (getcwd(shell_home, sizeof(shell_home)) == NULL) {
        perror("getcwd failed during shell home initialization");
        exit(EXIT_FAILURE);//stdlib prepro macro
    }
}

void display_prompt() {
    uid_t uid = getuid();
    struct passwd *pw = getpwuid(uid);
    const char *username;
    if(pw!= NULL) {
        username = pw->pw_name;
    } else {
        username = "not_found";
    }

    char hostname[HOST_NAME_MAX];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(hostname, "not_found", sizeof(hostname)-1);
        hostname[sizeof(hostname)-1] = '\0';
    }

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd failed");
        return;
    }

    char path[PATH_MAX];
    size_t h_len = strlen(shell_home);
    if (strcmp(cwd, shell_home) == 0) {
        snprintf(path, sizeof(path), "~");
    } else if (strncmp(cwd, shell_home, h_len) == 0 && cwd[h_len] == '/') {
        snprintf(path, sizeof(path), "~%s", cwd + h_len);
    } else {
        snprintf(path, sizeof(path), "%s", cwd);
    }
    printf("<%s@%s:%s> ", username, hostname, path);
    fflush(stdout);//line buffered
}