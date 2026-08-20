#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include "prompt.h"
#include "lexer.h"

int main() {
    init_homedir();
    char *uline = NULL;
    size_t len = 0;
    ssize_t inp_size = 0;
    while (1) {
        display_prompt();
        inp_size = getline(&uline, &len, stdin);
        if (inp_size == -1) {
            printf("\n"); 
            break;        
        }
        Token *tokens = lex_input(uline);
        if (tokens == NULL) {
            continue;
        }
        Token *current = tokens;
        while (current != NULL) {
            printf("Token: type=%d, value=[%s]\n", current->type, current->value);
            current = current->next;
        }
        free_tokens(tokens);
    }
    free(uline);
    return 0;
}