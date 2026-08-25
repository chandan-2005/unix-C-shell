#include <stdio.h>
#include "parser.h"

int valid_syntax(Token *head) {
    if (head == NULL) {
        return 1; 
    }
    Token *curr = head;
    Token *prev = NULL;

    while (curr != NULL) {
        if (prev == NULL && curr->type != TOKEN_WORD) {
            fprintf(stderr, "cshell: invalid syntax\n"); // Replaced perror!
            return 0;
        }

        if (curr->type == TOKEN_OP_LT || curr->type == TOKEN_OP_GT || curr->type == TOKEN_OP_GTGT) {
            if (curr->next == NULL || curr->next->type != TOKEN_WORD) {
                fprintf(stderr, "cshell: invalid syntax\n");
                return 0;
            }
        }

        if (curr->type == TOKEN_OP_PIPE) {
            if (curr->next == NULL || 
                curr->next->type == TOKEN_OP_PIPE || 
                curr->next->type == TOKEN_OP_AMP || 
                curr->next->type == TOKEN_OP_SEMI) {
                fprintf(stderr, "cshell: invalid syntax\n");
                return 0;
            }
        }

        if (curr->type == TOKEN_OP_SEMI) {
            if (curr->next == NULL || 
                curr->next->type == TOKEN_OP_PIPE || 
                curr->next->type == TOKEN_OP_AMP || 
                curr->next->type == TOKEN_OP_SEMI) {
                fprintf(stderr, "cshell: invalid syntax\n");
                return 0;
            }
        }
        if (curr->type == TOKEN_OP_AMP) {
            if (curr->next != NULL && 
               (curr->next->type == TOKEN_OP_PIPE || 
                curr->next->type == TOKEN_OP_AMP || 
                curr->next->type == TOKEN_OP_SEMI)) {
                fprintf(stderr, "cshell: invalid syntax\n");
                return 0;
            }
        }
        
        prev = curr;
        curr = curr->next;
    }
    return 1; 
}