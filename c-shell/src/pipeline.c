#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "pipeline.h"
#include "redirection.h"
#include "resolver.h"

void execute_pipeline(Token *tokens) {
    Token *list_end = NULL;
    for (Token *t = tokens; t != NULL; t = t->next) {
        if (strcmp(t->value, ";") == 0 || strcmp(t->value, "&") == 0) {
            list_end = t;
            break;
        }
    }

    Token *cmd_starts[128];
    int num_cmds = 0;
    cmd_starts[num_cmds++] = tokens;
    Token *curr = tokens;
    while (curr != NULL && curr != list_end) {
        if (strcmp(curr->value, "|") == 0) cmd_starts[num_cmds++] = curr->next;
        curr = curr->next;
    }

    int pipes[128][2];
    for (int i = 0; i < num_cmds - 1; i++) pipe(pipes[i]);

    pid_t pids[128];
    int tmp_in_fds[128];
    int out_fds_list[128][128];
    int out_counts[128];
    
    for (int i = 0; i < num_cmds; i++) {
        char *args[128]; int argc = 0;
        char *in_files[128]; int in_count = 0;
        char *out_files[128]; int out_modes[128]; out_counts[i] = 0;

        curr = cmd_starts[i];
        while (curr != NULL && curr != list_end && strcmp(curr->value, "|") != 0 && argc < 127) {
            if (strcmp(curr->value, "<") == 0 && curr->next) {
                in_files[in_count++] = curr->next->value;
                curr = curr->next->next; continue;
            } else if ((strcmp(curr->value, ">") == 0 || strcmp(curr->value, ">>") == 0) && curr->next) {
                out_modes[out_counts[i]] = (strcmp(curr->value, ">>") == 0);
                out_files[out_counts[i]++] = curr->next->value;
                curr = curr->next->next; continue;
            } else {
                args[argc++] = curr->value;
            }
            curr = curr->next;
        }
        args[argc] = NULL;
        
        if (argc == 0) { pids[i] = -1; continue; }

        tmp_in_fds[i] = setup_input(in_files, in_count, i);
        if (tmp_in_fds[i] == -2) { pids[i] = -1; continue; }

        if (setup_output(out_files, out_modes, out_counts[i], out_fds_list[i]) < 0) {
            if (tmp_in_fds[i] >= 0) {
                close(tmp_in_fds[i]);
                char name[64]; snprintf(name, sizeof(name), ".cshell_tmp_in_%d", i); unlink(name);
            }
            pids[i] = -1; continue;
        }

        char *exec_path = resolve_command_path(args[0]);
        if (!exec_path) {
            const char *display_name = (args[0][0] == '%') ? args[0] + 1 : args[0];
            fprintf(stderr, "cshell: command not found (%s)\n", display_name);
            pids[i] = -1; continue;
        }

        pids[i] = fork();
        if (pids[i] == 0) {
            /* Child connects inputs and outputs */
            if (tmp_in_fds[i] >= 0) dup2(tmp_in_fds[i], STDIN_FILENO);
            else if (i > 0) dup2(pipes[i-1][0], STDIN_FILENO); 

            if (out_counts[i] > 0) {
                char tmp_out_name[64];
                snprintf(tmp_out_name, sizeof(tmp_out_name), ".cshell_tmp_out_%d", i);
                int tmp_out_fd = open(tmp_out_name, O_RDWR | O_CREAT | O_TRUNC, 0600);
                dup2(tmp_out_fd, STDOUT_FILENO);
                close(tmp_out_fd);
            } else if (i < num_cmds - 1) {
                dup2(pipes[i][1], STDOUT_FILENO); 
            }

            for (int p = 0; p < num_cmds - 1; p++) { close(pipes[p][0]); close(pipes[p][1]); }
            
            if (tmp_in_fds[i] >= 0) close(tmp_in_fds[i]);
            for (int j = 0; j < out_counts[i]; j++) close(out_fds_list[i][j]);

            execv(exec_path, args);
            perror("execv failed");
            exit(EXIT_FAILURE);
        }
        free(exec_path);
    }
    for (int p = 0; p < num_cmds - 1; p++) { close(pipes[p][0]); close(pipes[p][1]); }

    for (int i = 0; i < num_cmds; i++) {
        if (pids[i] > 0) waitpid(pids[i], NULL, 0); 
        
        if (tmp_in_fds[i] >= 0) {
            close(tmp_in_fds[i]);
            char name[64]; snprintf(name, sizeof(name), ".cshell_tmp_in_%d", i); unlink(name);
        }
        
        if (out_counts[i] > 0 && pids[i] > 0) distribute_output(out_fds_list[i], out_counts[i], i);
        else if (out_counts[i] > 0) for (int j = 0; j < out_counts[i]; j++) close(out_fds_list[i][j]);
    }
}