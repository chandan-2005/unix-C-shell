#ifndef PROMPT_H
#define PROMPT_H
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif

void init_homedir();
void display_prompt();
#endif