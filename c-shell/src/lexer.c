#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"

static void add_token(Token **head, Token **tail, TokenType type, const char *value) {
    Token *new_node = malloc(sizeof(Token));
    if (!new_node) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    
    new_node->type = type;
    new_node->value = strdup(value); 
    new_node->next = NULL;

    if (*head == NULL) {
        *head = new_node;
        *tail = new_node;
    } else {
        (*tail)->next = new_node;
        *tail = new_node;
    }
}

Token* lex_input(const char *input) {
    Token *head = NULL;
    Token *tail = NULL;
    int i = 0;

    while (input[i] != '\0') {
        if (input[i] == ' ' || input[i] == '\t' || input[i] == '\n' || input[i] == '\r') {
            i++;
            continue;
        }

        if (input[i] == '>') {
            if (input[i+1] == '>') {
                add_token(&head, &tail, TOKEN_OP_GTGT, ">>");
                i += 2; 
                continue;
            } else {
                add_token(&head, &tail, TOKEN_OP_GT, ">");
                i += 1; 
                continue;
            }
        } 
        else if (input[i] == '<') {
            add_token(&head, &tail, TOKEN_OP_LT, "<");
            i++; continue;
        } 
        else if (input[i] == '|') {
            add_token(&head, &tail, TOKEN_OP_PIPE, "|");
            i++; continue;
        } 
        else if (input[i] == '&') {
            add_token(&head, &tail, TOKEN_OP_AMP, "&");
            i++; continue;
        } 
        else if (input[i] == ';') {
            add_token(&head, &tail, TOKEN_OP_SEMI, ";");
            i++; continue;
        }
        
        else {
            char buffer[4096]; 
            int b_idx = 0;
            int in_sq = 0; 
            int in_dq = 0; 

            while (input[i] != '\0') {
                if (!in_sq && !in_dq) {
                    if (input[i] == ' ' || input[i] == '\t' || input[i] == '\n' || input[i] == '\r') break;
                    if (input[i] == '|' || input[i] == '&' || input[i] == ';' || 
                        input[i] == '<' || input[i] == '>') break;
                }

                if (input[i] == '\'' && !in_dq) {
                    in_sq = !in_sq; 
                    i++;
                    continue;
                }

                if (input[i] == '"' && !in_sq) {
                    in_dq = !in_dq; 
                    i++;
                    continue;
                }

                if (input[i] == '\\') {
                    if (in_sq) {
                        buffer[b_idx++] = input[i++];
                    } else if (in_dq) {
                        if (input[i+1] == '"' || input[i+1] == '\\') {
                            buffer[b_idx++] = input[i+1];
                            i += 2;
                        } else {
                            buffer[b_idx++] = input[i++];
                        }
                    } else {
                        if (input[i+1] == '\0' || input[i+1] == '\n') {
                            fprintf(stderr, "cshell: invalid syntax\n");
                            free_tokens(head);
                            return NULL;
                        }
                        buffer[b_idx++] = input[i+1];
                        i += 2;
                    }
                    continue;
                }

                buffer[b_idx++] = input[i++];
            }
            if (in_sq || in_dq) {
                fprintf(stderr, "cshell: invalid syntax\n");
                free_tokens(head);
                return NULL;
            }
            buffer[b_idx] = '\0';
            if (b_idx > 0) {
                add_token(&head, &tail, TOKEN_WORD, buffer);
            }
        }
    }
    return head;
}

void free_tokens(Token *head) {
    Token *current = head;
    while (current != NULL) {
        Token *next = current->next;
        free(current->value);
        free(current);
        current = next;
    }
}