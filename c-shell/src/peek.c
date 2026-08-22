#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "peek.h"

#define CHUNK_SIZE 4096

static int is_non_empty(const char *line) {
    return (line[0] != '\n' && line[0] != '\r');
}

static void print_forward(FILE *fp, int flag_n) {
    char *line = NULL;
    size_t len = 0;
    int line_num = 1;
    
    while (getline(&line, &len, fp) != -1) {
        if (flag_n && is_non_empty(line)) {
            printf("%d ", line_num++);
        }
        printf("%s", line);
        if (line[strlen(line)-1] != '\n') printf("\n");
    }
    free(line);
}

static void print_reverse_buffered(FILE *fp, int flag_n) {
    char **lines = NULL;
    int count = 0;
    char *line = NULL;
    size_t len = 0;
    
    while (getline(&line, &len, fp) != -1) {
        lines = realloc(lines, sizeof(char*) * (count + 1));
        lines[count++] = strdup(line);
    }
    free(line);
    
    int total_lines = 0;
    for (int i = 0; i < count; i++) {
        if (is_non_empty(lines[i])) total_lines++;
    }

    for (int i = count - 1; i >= 0; i--) {
        if (flag_n && is_non_empty(lines[i])) {
            printf("%d ", total_lines--);
        }
        printf("%s", lines[i]);
        if (lines[i][strlen(lines[i])-1] != '\n') printf("\n");
        free(lines[i]);
    }
    free(lines);
}

static void print_reverse_chunked(int fd, int flag_n) {
    off_t filesize = lseek(fd, 0, SEEK_END);
    if (filesize <= 0) return;
    
    int total_lines = 0;
    if (flag_n) {
        lseek(fd, 0, SEEK_SET);
        FILE *fp = fdopen(dup(fd), "r"); 
        char *l = NULL; size_t sz = 0;
        while (getline(&l, &sz, fp) != -1) {
            if (is_non_empty(l)) total_lines++;
        }
        free(l);
        fclose(fp);
        lseek(fd, 0, SEEK_END); 
    }
    
    char buf[CHUNK_SIZE];
    off_t pos = filesize;
    off_t line_end = pos;
    
    while (pos > 0) {
        off_t read_size = (pos < CHUNK_SIZE) ? pos : CHUNK_SIZE;
        pos -= read_size; 
        lseek(fd, pos, SEEK_SET);
        read(fd, buf, read_size);
        for (int i = read_size - 1; i >= 0; i--) {
            if (buf[i] == '\n') {
                off_t line_start = pos + i + 1;
                size_t len = line_end - line_start;
                
                if (len > 0) {
                    char *line_buf = malloc(len + 1);
                    lseek(fd, line_start, SEEK_SET);
                    read(fd, line_buf, len);
                    line_buf[len] = '\0';
                    
                    if (flag_n && is_non_empty(line_buf)) {
                        printf("%d ", total_lines--);
                    }
                    printf("%s", line_buf);
                    if (line_buf[len-1] != '\n') printf("\n");
                    free(line_buf);
                } else {
                    printf("\n"); 
                }
                line_end = pos + i + 1;
            }
        }
    }
    
    if (line_end > 0) {
        size_t len = line_end;
        char *line_buf = malloc(len + 1);
        lseek(fd, 0, SEEK_SET);
        read(fd, line_buf, len);
        line_buf[len] = '\0';
        
        if (flag_n && is_non_empty(line_buf)) printf("%d ", total_lines--);
        printf("%s", line_buf);
        if (line_buf[len-1] != '\n') printf("\n");
        free(line_buf);
    }
}

void run_peek(Token *tokens) {
    int flag_n = 0, flag_r = 0;
    char *files[100];
    int file_count = 0;
    
    Token *curr = tokens->next;
    while (curr != NULL) {
        if (curr->value[0] == '-' && strlen(curr->value) > 1) {
            for (int i = 1; curr->value[i] != '\0'; i++) {
                if (curr->value[i] == 'n') flag_n = 1;
                else if (curr->value[i] == 'r') flag_r = 1;
                else {
                    fprintf(stderr, "peek: invalid flag -- '%c'\n", curr->value[i]);
                    return;
                }
            }
        } else {
            files[file_count++] = curr->value;
        }
        curr = curr->next;
    } 
    if (file_count == 0) files[file_count++] = "-"; 
    for (int i = 0; i < file_count; i++) {
        const char *filename = files[i];
        
        if (strcmp(filename, "-") == 0) {
            if (flag_r) print_reverse_buffered(stdin, flag_n);
            else print_forward(stdin, flag_n);
            continue;
        }
        struct stat st;
        if (stat(filename, &st) == -1) {
            fprintf(stderr, "peek: no such file or directory\n");
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            fprintf(stderr, "peek: is a directory\n");
            continue;
        } 
        if (flag_r) {
            int fd = open(filename, O_RDONLY);
            if (fd == -1) { perror("peek"); continue; }
            print_reverse_chunked(fd, flag_n);
            close(fd);
        } else {
            FILE *fp = fopen(filename, "r");
            if (!fp) { perror("peek"); continue; }
            print_forward(fp, flag_n);
            fclose(fp);
        }
    }
}