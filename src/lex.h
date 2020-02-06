#ifndef LEX_H
#define LEX_H

#include <inttypes.h>
#include <stdbool.h>

/* special operator cases.. for non-special, code is regular ascii */
#define SPECO_NULL          0
#define SPECO_EQ				    1  /* == */
#define SPECO_NEQ			      2  /* != */
#define SPECO_UNARY_MINUS 	3
#define SPECO_UNARY_PLUS		4
#define SPECO_INC_ONE		    5  /* ++ */
#define SPECO_DEC_ONE		    6  /* -- */
#define SPECO_INC_BY			  7  /* += */
#define SPECO_DEC_BY			  8  /* -= */
#define SPECO_MUL_BY			  9  /* *= */
#define SPECO_DIV_BY			  10 /* /= */
#define SPECO_MOD_BY			  11 /* %= */
#define SPECO_SHL_BY			  12 /* <<= */
#define SPECO_SHR_BY			  13 /* >>= */
#define SPECO_AND_BY			  14 /* &= */
#define SPECO_OR_BY			    15 /* |= */
#define SPECO_XOR_BY			  16 /* ^= */
#define SPECO_SHL			      17 /* << */
#define SPECO_SHR			      18 /* >> */
#define SPECO_LOG_AND		    19 /* && */
#define SPECO_LOG_OR			  20 /* || */
#define SPECO_GE				    21 /* >= */
#define SPECO_LE				    22 /* <= */
#define SPECO_ARROW			    23 /* -> */
#define SPECO_TYPENAME		  24 /* typename */
#define SPECO_SIZEOF			  25 /* sizeof */
#define SPECO_CALL			    26
#define SPECO_DOTS			    27 /* ... */
#define SPECO_CAST			    28
#define SPECO_INDEX			    29

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
    uint8_t oval;
    double fval;
    char *sval;
  };
  char *as_string;

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
