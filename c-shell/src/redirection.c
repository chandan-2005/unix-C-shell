#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "redirection.h"
int setup_input(char *in_files[], int in_count, int cmd_index) {
    if (in_count == 0) return -1;

    char name[64];
    snprintf(name, sizeof(name), ".cshell_tmp_in_%d", cmd_index);

    int tmp_fd = open(name, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (tmp_fd < 0) { perror("cshell: tmpfile failed"); return -1; }

    for (int i = 0; i < in_count; i++) {
        int fd = open(in_files[i], O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "cshell: no such file or directory\n");
            close(tmp_fd); 
            unlink(name);
            return -2;
        }
        char buf[4096]; 
        ssize_t bytes;
        while ((bytes = read(fd, buf, sizeof(buf))) > 0) write(tmp_fd, buf, bytes);
        close(fd);
    }
    lseek(tmp_fd, 0, SEEK_SET);
    return tmp_fd;
}

int setup_output(char *out_files[], int out_modes[], int out_count, int out_fds[]) {
    for (int i = 0; i < out_count; i++) {
        int flags = O_WRONLY | O_CREAT | (out_modes[i] ? O_APPEND : O_TRUNC);
        out_fds[i] = open(out_files[i], flags, 0644);
        
        if (out_fds[i] < 0) {
            fprintf(stderr, "cshell: unable to create file for writing\n");
            for (int j = 0; j < i; j++) close(out_fds[j]);
            return -1;
        }
    }
    return 0;
}

void distribute_output(int out_fds[], int out_count, int cmd_index) {
    char name[64];
    snprintf(name, sizeof(name), ".cshell_tmp_out_%d", cmd_index);
    
    int tmp_out_fd = open(name, O_RDONLY);
    if (tmp_out_fd >= 0) {
        char buf[4096];
        ssize_t bytes;
        while ((bytes = read(tmp_out_fd, buf, sizeof(buf))) > 0) {
            for (int i = 0; i < out_count; i++) write(out_fds[i], buf, bytes);
        }
        close(tmp_out_fd);
    }
    unlink(name); 
    for (int i = 0; i < out_count; i++) close(out_fds[i]);
}