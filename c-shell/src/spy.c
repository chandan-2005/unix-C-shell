#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "spy.h"

static const char* type_of(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return "REG";
    if (S_ISDIR(st.st_mode)) return "DIR";
    if (S_ISCHR(st.st_mode)) return "CHR";
    if (S_ISBLK(st.st_mode)) return "BLK";
    if (S_ISFIFO(st.st_mode)) return "FIFO";
    if (S_ISSOCK(st.st_mode)) return "SOCK";
    return "REG";
}

static void print_row(pid_t pid, const char *fd, const char *type, const char *path) {
    printf("%d    %s    %s    %s\n", (int)pid, fd, type, path);
}

void run_spy(Token *tokens) {
    Token *t = tokens->next;
    if (t != NULL && t->next != NULL) {
        fprintf(stderr, "spy: invalid syntax\n");
        return;
    }

    pid_t pid;
    if (t == NULL) {
        pid = getpid();
    } else {
        char *endp;
        long v = strtol(t->value, &endp, 10);
        if (*endp != '\0' || t->value[0] == '\0' || v < 0) {
            fprintf(stderr, "spy: no such process\n");
            return;
        }
        pid = (pid_t)v;
    }

    char procdir[64];
    snprintf(procdir, sizeof(procdir), "/proc/%d", (int)pid);
    struct stat st;
    if (stat(procdir, &st) != 0) {
        fprintf(stderr, "spy: no such process\n");
        return;
    }

    printf("PID    FD    TYPE   PATH\n");

    char linkpath[300], target[512];
    ssize_t len;

    snprintf(linkpath, sizeof(linkpath), "/proc/%d/cwd", (int)pid);
    len = readlink(linkpath, target, sizeof(target) - 1);
    if (len > 0) {
        target[len] = '\0';
        print_row(pid, "cwd", type_of(target), target);
    }

    snprintf(linkpath, sizeof(linkpath), "/proc/%d/exe", (int)pid);
    len = readlink(linkpath, target, sizeof(target) - 1);
    if (len > 0) {
        target[len] = '\0';
        print_row(pid, "txt", type_of(target), target);
    }

    /* mem: every uniquely file-backed mapping in /proc/<pid>/maps */
    char mapspath[64];
    snprintf(mapspath, sizeof(mapspath), "/proc/%d/maps", (int)pid);
    FILE *fp = fopen(mapspath, "r");
    if (fp) {
        char *seen[256];
        int seen_count = 0;
        char line[1024];
        while (fgets(line, sizeof(line), fp)) {
            char *slash = strchr(line, '/');
            if (!slash) continue;
            char path[512];
            size_t plen = strcspn(slash, "\n");
            if (plen >= sizeof(path)) plen = sizeof(path) - 1;
            memcpy(path, slash, plen);
            path[plen] = '\0';

            int dup = 0;
            for (int i = 0; i < seen_count; i++) {
                if (strcmp(seen[i], path) == 0) { dup = 1; break; }
            }
            if (dup) continue;
            if (seen_count < 256) seen[seen_count++] = strdup(path);

            print_row(pid, "mem", type_of(path), path);
        }
        for (int i = 0; i < seen_count; i++) free(seen[i]);
        fclose(fp);
    }

    /* numeric fds, ascending */
    char fddir[64];
    snprintf(fddir, sizeof(fddir), "/proc/%d/fd", (int)pid);
    DIR *d = opendir(fddir);
    if (d) {
        int nums[1024];
        int ncount = 0;
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            char *endp;
            long v = strtol(ent->d_name, &endp, 10);
            if (*endp != '\0') continue;
            if (ncount < 1024) nums[ncount++] = (int)v;
        }
        closedir(d);

        for (int a = 0; a < ncount; a++) {
            for (int b = a + 1; b < ncount; b++) {
                if (nums[b] < nums[a]) { int tmp = nums[a]; nums[a] = nums[b]; nums[b] = tmp; }
            }
        }

        for (int i = 0; i < ncount; i++) {
            char fdlink[96];
            snprintf(fdlink, sizeof(fdlink), "%s/%d", fddir, nums[i]);
            len = readlink(fdlink, target, sizeof(target) - 1);
            if (len <= 0) continue;
            target[len] = '\0';
            char label[16];
            snprintf(label, sizeof(label), "%d", nums[i]);
            print_row(pid, label, type_of(target), target);
        }
    }
}
