#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>
#include <sys/stat.h>
#include "hop.h"
#include "prompt.h"

#define MAX_ENTRIES 256

typedef struct {
    char path[PATH_MAX];
    long visits;
    time_t last_access;
} FrecencyEntry;

static char prev_dir[PATH_MAX] = "";

const char* get_hop_prev_dir(void) {
    return prev_dir;
}

static void frecency_db_path(char out[PATH_MAX]) {
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(out, PATH_MAX, "%s/.cshell_frecency", home);
}//frequency database path (persistent storage) is ~/.cshell_frecency in the user's home directory. If HOME is not set, it defaults to /tmp/.cshell_frecency.

static int load_entries(FrecencyEntry entries[MAX_ENTRIES]) {
    char dbpath[PATH_MAX];
    frecency_db_path(dbpath);

    FILE *fp = fopen(dbpath, "r");
    if (!fp) return 0;

    int count = 0;
    char line[PATH_MAX + 64];
    while (count < MAX_ENTRIES && fgets(line, sizeof(line), fp)) {
        long visits, last;
        char path[PATH_MAX];
        if (sscanf(line, "%ld %ld %[^\n]", &visits, &last, path) == 3) {
            strncpy(entries[count].path, path, PATH_MAX - 1);
            entries[count].path[PATH_MAX - 1] = '\0';
            entries[count].visits = visits;
            entries[count].last_access = (time_t)last;
            count++;
        }
    }
    fclose(fp);
    return count;
}

static void save_entries(FrecencyEntry entries[MAX_ENTRIES], int count) {
    char dbpath[PATH_MAX];
    frecency_db_path(dbpath);

    FILE *fp = fopen(dbpath, "w");
    if (!fp) return;
    for (int i = 0; i < count; i++) {
        fprintf(fp, "%ld %ld %s\n", entries[i].visits, (long)entries[i].last_access, entries[i].path);
    }
    fclose(fp);
}

static void record_visit(const char *abspath) {
    static FrecencyEntry entries[MAX_ENTRIES];
    int count = load_entries(entries);
    time_t now = time(NULL);

    int found = -1;
    for (int i = 0; i < count; i++) {
        if (strcmp(entries[i].path, abspath) == 0) { found = i; break; }
    }

    if (found >= 0) {
        entries[found].visits += 1;
        entries[found].last_access = now;
    } else if (count < MAX_ENTRIES) {
        strncpy(entries[count].path, abspath, PATH_MAX - 1);
        entries[count].path[PATH_MAX - 1] = '\0';
        entries[count].visits = 1;
        entries[count].last_access = now;
        count++;
    }
    save_entries(entries, count);
}

static double score_of(long visits, time_t last_access, time_t now) {
    double age_secs = difftime(now, last_access);
    double multiplier;
    if (age_secs < 3600) multiplier = 4.0;        
    else if (age_secs < 86400) multiplier = 2.0;  
    else if (age_secs < 604800) multiplier = 0.5;
    else multiplier = 0.25;                       
    return (double)visits * multiplier;
}

static int frecency_lookup(const char *name, char out[PATH_MAX]) {
    static FrecencyEntry entries[MAX_ENTRIES];
    int count = load_entries(entries);
    time_t now = time(NULL);

    int best = -1;
    double best_score = -1.0;
    for (int i = 0; i < count; i++) {
        if (!strstr(entries[i].path, name)) continue;

        struct stat st;
        if (stat(entries[i].path, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        double s = score_of(entries[i].visits, entries[i].last_access, now);
        if (s > best_score) {
            best_score = s;
            best = i;
        }
    }
    if (best < 0) return 0;
    strncpy(out, entries[best].path, PATH_MAX - 1);
    out[PATH_MAX - 1] = '\0';
    return 1;
}

static int resolve_direct(const char *name, char out[PATH_MAX]) {
    struct stat st;
    if (stat(name, &st) != 0 || !S_ISDIR(st.st_mode)) return 0;
    if (realpath(name, out) == NULL) return 0;
    return 1;
}

static void do_chdir(const char *target) {
    char old_cwd[PATH_MAX];
    if (getcwd(old_cwd, sizeof(old_cwd)) == NULL) old_cwd[0] = '\0';

    if (chdir(target) != 0) {
        fprintf(stderr, "hop: no such directory\n");
        return;
    }

    char new_cwd[PATH_MAX];
    if (getcwd(new_cwd, sizeof(new_cwd)) != NULL) {
        record_visit(new_cwd);
    }
    if (old_cwd[0] != '\0') {
        strncpy(prev_dir, old_cwd, PATH_MAX - 1);
        prev_dir[PATH_MAX - 1] = '\0';
    }
}

static void hop_one(const char *arg) {
    if (strcmp(arg, ".") == 0) {
        return; 
    }
    if (strcmp(arg, "~") == 0) {
        do_chdir(get_shell_home());
        return;
    }
    if (strcmp(arg, "..") == 0) {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL) return;
        if (strcmp(cwd, "/") == 0) return; 
        do_chdir("..");//go up one directory level
        return;
    }
    if (strcmp(arg, "-") == 0) {
        if (prev_dir[0] == '\0') return; 
        do_chdir(prev_dir);
        return;
    }

    char resolved[PATH_MAX];
    if (resolve_direct(arg, resolved)) {
        do_chdir(resolved);
        return;
    }
    if (frecency_lookup(arg, resolved)) {
        do_chdir(resolved);
        return;
    }
    fprintf(stderr, "hop: no such directory\n");
}

void run_hop(Token *tokens) {
    Token *curr = tokens->next;
    if (curr == NULL) {
        hop_one("~");//no arg then hop ~
        return;
    }
    while (curr != NULL) {
        hop_one(curr->value);
        curr = curr->next;
    }
}