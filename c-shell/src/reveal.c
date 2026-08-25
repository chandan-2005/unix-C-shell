#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include "reveal.h"
#include "prompt.h"
#include "hop.h"

static int cmp_names(const void *a, const void *b) {
    const char *sa = *(const char * const *)a;
    const char *sb = *(const char * const *)b;
    return strcmp(sa, sb);
}

static char** read_sorted_entries(const char *path, int flag_a, int *out_count) {
    DIR *dir = opendir(path);
    if (!dir) {
        *out_count = -1;
        return NULL;
    }

    char **names = NULL;
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (!flag_a && entry->d_name[0] == '.') continue;
        names = realloc(names, sizeof(char*) * (count + 1));
        names[count++] = strdup(entry->d_name);
    }
    closedir(dir);

    if (count > 1) qsort(names, count, sizeof(char*), cmp_names);
    *out_count = count;
    return names;
}

static void free_entries(char **names, int count) {
    for (int i = 0; i < count; i++) free(names[i]);
    free(names);
}

static void reveal_dir(const char *abs_path, const char *rel_prefix, int flag_a, int flag_t) {
    int count;
    char **names = read_sorted_entries(abs_path, flag_a, &count);
    if (count < 0) {
        fprintf(stderr, "reveal: no such directory\n");
        return;
    }

    for (int i = 0; i < count; i++) {
        char rel[PATH_MAX];
        if (rel_prefix[0] == '\0') snprintf(rel, sizeof(rel), "%s", names[i]);
        else snprintf(rel, sizeof(rel), "%s/%s", rel_prefix, names[i]);

        char child_abs[PATH_MAX];
        snprintf(child_abs, sizeof(child_abs), "%s/%s", abs_path, names[i]);

        struct stat st;
        int is_dir = (stat(child_abs, &st) == 0 && S_ISDIR(st.st_mode));

        if (is_dir && flag_t) printf("%s/\n", rel);
        else printf("%s\n", rel);

        if (is_dir && flag_t) {
            reveal_dir(child_abs, rel, flag_a, flag_t);
        }
    }
    free_entries(names, count);
}

void run_reveal(Token *tokens) {
    int flag_a = 0, flag_t = 0;
    const char *arg = NULL;
    int arg_count = 0;

    Token *curr = tokens->next;
    while (curr != NULL) {
        if (curr->value[0] == '-' && strlen(curr->value) > 1) {
            for (int i = 1; curr->value[i] != '\0'; i++) {
                if (curr->value[i] == 'a') flag_a = 1;
                else if (curr->value[i] == 't') flag_t = 1;
                else {
                    fprintf(stderr, "reveal: invalid syntax\n");
                    return;
                }
            }
        } else {
            arg = curr->value;
            arg_count++;
        }
        curr = curr->next;
    }

    if (arg_count > 1) {
        fprintf(stderr, "reveal: invalid syntax\n");
        return;
    }

    const char *path;
    if (arg == NULL) {
        path = ".";
    } else if (strcmp(arg, "~") == 0) {
        path = get_shell_home();
    } else if (strcmp(arg, ".") == 0) {
        path = ".";
    } else if (strcmp(arg, "..") == 0) {
        path = "..";
    } else if (strcmp(arg, "-") == 0) {
        const char *prev = get_hop_prev_dir();
        if (prev[0] == '\0') {
            fprintf(stderr, "reveal: no such directory\n");
            return;
        }
        path = prev;
    } else {
        path = arg;
    }

    struct stat st;
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "reveal: no such directory\n");
        return;
    }

    reveal_dir(path, "", flag_a, flag_t);
}