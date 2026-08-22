#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "reveal.h"
#include "prompt.h"
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>

static void print_detailed(const char *filepath, const char *filename) {
    struct stat st;
    if (stat(filepath, &st) == -1) {
        perror("stat failed");
        return;
    }
    printf((S_ISDIR(st.st_mode)) ? "d" : "-");
    printf((st.st_mode & S_IRUSR) ? "r" : "-");
    printf((st.st_mode & S_IWUSR) ? "w" : "-");
    printf((st.st_mode & S_IXUSR) ? "x" : "-");
    printf((st.st_mode & S_IRGRP) ? "r" : "-");
    printf((st.st_mode & S_IWGRP) ? "w" : "-");
    printf((st.st_mode & S_IXGRP) ? "x" : "-");
    printf((st.st_mode & S_IROTH) ? "r" : "-");
    printf((st.st_mode & S_IWOTH) ? "w" : "-");
    printf((st.st_mode & S_IXOTH) ? "x" : "-");
    printf(" %lu ", st.st_nlink);
    struct passwd *pw = getpwuid(st.st_uid);
    struct group  *gr = getgrgid(st.st_gid);
    printf("%s %s ", pw ? pw->pw_name : "unknown", gr ? gr->gr_name : "unknown");
    printf("%8ld ", st.st_size);

    char timebuf[64];
    struct tm *tm_info = localtime(&st.st_mtime);
    strftime(timebuf, sizeof(timebuf), "%b %d %H:%M", tm_info);
    printf("%s ", timebuf);
    printf("%s\n", filename);
}


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
        char full_path[4096];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        if (flag_l) {
            print_detailed(full_path, entry->d_name);
        } else {
            printf("%s\n", entry->d_name);
        }
    }
    closedir(dir);
}