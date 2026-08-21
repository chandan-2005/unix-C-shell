#ifndef LEXER_H
#define LEXER_H 
typedef enum TokenType {
    TOKEN_OP_PIPE,
    TOKEN_OP_AMP,
    TOKEN_OP_SEMI,
    TOKEN_OP_LT,
    TOKEN_OP_GT,
    TOKEN_OP_GTGT,
    TOKEN_WORD
} TokenType;

typedef struct Token {
    TokenType type;
    char *value;
    struct Token *next;
} Token;

Token* lex_input(const char *input);
void free_t(Token *tokens);

#endif 