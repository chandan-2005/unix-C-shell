#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "external.h"

static int is_exe(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode) && (access(path, X_OK) == 0));
}

static char* resolve_command_path(const char *cmd) {
    char full_path[8192];
    if (strchr(cmd, '/') != NULL) {
        if (is_exe(cmd)) return strdup(cmd);
        return NULL;
    }
    int skip_cwd = 0;
    const char *search_cmd = cmd;
    if (cmd[0] == '%') {
        skip_cwd = 1;
        search_cmd = cmd + 1; 
    }
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

void run_external(Token *tokens) {
    char *args[128]; 
    int argc = 0;
    char *in_files[128];
    int in_count = 0;

    Token *curr = tokens;
    while (curr != NULL && argc < 127) {
        if (curr->type == TOKEN_OP_LT) {
            if (curr->next && curr->next->type == TOKEN_WORD) {
                in_files[in_count++] = curr->next->value;
                curr = curr->next->next; 
                continue;
            }
        } 
        else if (curr->type == TOKEN_WORD) {
            args[argc++] = curr->value;
        } 
        else {
            break;
        }
        curr = curr->next;
    }
    args[argc] = NULL; 

    if (argc == 0) return;

    int tmp_fd = -1;
    if (in_count > 0) {
        tmp_fd = open(".cshell_tmp_in", O_RDWR | O_CREAT | O_TRUNC, 0600);
        if (tmp_fd < 0) { 
            perror("cshell: tmpfile failed"); 
            return; 
        }

        for (int i = 0; i < in_count; i++) {
            int fd = open(in_files[i], O_RDONLY);
            if (fd < 0) {
                fprintf(stderr, "cshell: no such file or directory\n");
                close(tmp_fd);
                unlink(".cshell_tmp_in"); 
                return;
            }
            char buf[4096];
            ssize_t bytes;
            while ((bytes = read(fd, buf, sizeof(buf))) > 0) {
                write(tmp_fd, buf, bytes);
            }
            close(fd);
        }
        lseek(tmp_fd, 0, SEEK_SET);
    }

    char *exec_path = resolve_command_path(args[0]);
    if (exec_path == NULL) {
        fprintf(stderr, "cshell: command not found (%s)\n", args[0]);
        if (tmp_fd != -1) { close(tmp_fd); unlink(".cshell_tmp_in"); }
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
    } else if (pid == 0) {
        if (tmp_fd != -1) {
            dup2(tmp_fd, STDIN_FILENO);
            close(tmp_fd);
        }
        execv(exec_path, args);
        perror("execv failed");
        exit(EXIT_FAILURE);
    } else {
        int status;
        waitpid(pid, &status, 0);
        
        if (tmp_fd != -1) {
            close(tmp_fd);
            unlink(".cshell_tmp_in");
        }
    }

    free(exec_path);
}