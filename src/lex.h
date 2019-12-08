#ifndef LEX_H
#define LEX_H

#include <inttypes.h>
#include <stdbool.h>

typedef enum LexTokenType {
    TOKEN_UNDEFINED = 0,
    TOKEN_IDENTIFIER,
    TOKEN_OPERATOR,
    TOKEN_INTEGER,
    TOKEN_FLOAT,
    TOKEN_STRING_LITERAL,
    TOKEN_CHARACTER_LITERAL,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_DO,
    TOKEN_RETURN
} LexTokenType_T;

typedef struct LexToken {
    LexTokenType_T type;
    unsigned lineno;
    bool sval_ownership;
    union {
        int64_t ival;
        double fval;
        char *sval;
    };

    struct LexToken *next;
} LexToken_T;

typedef struct LexState {
    char *filename;
    char *contents;
    size_t index;
    size_t flen;
    size_t lineno;
    LexToken_T *tokens;
    LexToken_T *backtoken;
} LexState_T;

LexState_T *lex_file(const char *);
void lex_cleanup(LexState_T **);

#endif
