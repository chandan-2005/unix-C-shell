#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include "prompt.h"
#include "lexer.h"
#include "parser.h"
#include "execute.h"
#include "reveal.h"
int main(void) {
    init_homedir();
    char *line = NULL;  
    size_t len = 0;     
    ssize_t read;       
    while (1) {
        display_prompt();
        read = getline(&line, &len, stdin);
        if (read == -1) {
            printf("\n"); 
            break;        
        }
        Token *tokens = lex_input(line);
        if (tokens != NULL) {
            if (valid_syntax(tokens)) {
                exec_cmd(tokens);
                }
            free_t(tokens);
        }
    }
    free(line);
    return 0;
}