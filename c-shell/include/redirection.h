#ifndef REDIRECTION_H
#define REDIRECTION_H

int setup_input(char *in_files[], int in_count, int cmd_index);
int setup_output(char *out_files[], int out_modes[], int out_count, int out_fds[]);
void distribute_output(int out_fds[], int out_count, int cmd_index);

#endif