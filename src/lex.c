#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include "lex.h"

/* on_number constants */
#define NOT_ON_NUMBER 0
#define ON_INTEGER    1
#define ON_FLOAT      2

/* maximum CHARACTER size of number literals */
#define MAX_INTEGER_LENGTH 64
#define MAX_FLOAT_LENGTH   64

/* definitions for read_operator */
static const struct {
  const char *operator;
  uint8_t    opcode;
} multi_operators[] = {
  {">=",  SPECO_GE},
  {"<=",  SPECO_LE},
  {"==",  SPECO_EQ},
  {"!=",  SPECO_NEQ},
  {">>=", SPECO_SHR_BY},
  {"<<=", SPECO_SHL_BY},
  {"+=",  SPECO_INC_BY},
  {"-=",  SPECO_DEC_BY},
  {"*=",  SPECO_MUL_BY},
  {"/=",  SPECO_DIV_BY},
  {"%=",  SPECO_MOD_BY},
  {"^=",  SPECO_XOR_BY},
  {"|=",  SPECO_OR_BY},
  {"&=",  SPECO_AND_BY},
  {"++",  SPECO_INC_ONE},
  {"--",  SPECO_DEC_ONE},
  {">>",  SPECO_SHR},
  {"<<",  SPECO_SHL},
  {"&&",  SPECO_LOG_AND},
  {"||",  SPECO_LOG_OR},
  {"->",  SPECO_ARROW},
};

static void lex_err(LexState_T *L, const char *fmt, ...) {

  va_list varargs;
  va_start(varargs, fmt);

  fprintf(stderr, "Spyre Lex error:\n");
  fprintf(stderr, "\tmessage: ");
  vfprintf(stderr, fmt, varargs);
  fprintf(stderr, "\n\tline:    %zu", L->lineno);
  fprintf(stderr, "\n\tfile:    %s\n", L->filename);

  va_end(varargs);

  exit(EXIT_FAILURE);
}

static inline bool ready(LexState_T *L) {
  return L->index < L->flen; 
}

static inline void advance(LexState_T *L, int n) {
  L->index += n;
}

static inline int get(LexState_T *L) {
  return ready(L) ? L->contents[L->index++] : EOF;
}

static inline int at(LexState_T *L) {
  return ready(L) ? L->contents[L->index] : EOF;
}

static inline int peek(LexState_T *L, int n) {
  if (L->index + n < L->flen) {
    return L->contents[L->index + n];
  }
  return EOF;
}

static bool is_on_literal(LexState_T *L, const char *literal) {
  int offset = 0;
  for (const char *c = literal; *c; c++) {
    if (peek(L, offset++) != *c) {
      return false;
    }
  }
  return true;
}

static char *token_tostring(LexToken_T *token) {
  char *buf;
  const char *word = NULL;

  if (token->type == TOKEN_INTEGER || token->type == TOKEN_CHARACTER_LITERAL) {
    buf = malloc(sizeof(char) * 16);
    assert(buf);
    snprintf(buf, 16, "%lld", token->ival);
  } else if (token->type == TOKEN_FLOAT) {
    buf = malloc(sizeof(char) * 32);
    assert(buf);
    snprintf(buf, 32, "%f", token->fval);
  } else if (token->type == TOKEN_OPERATOR) {
    for (size_t i = 0; i < sizeof(multi_operators)/sizeof(multi_operators[0]); i++) {
      if (token->oval == multi_operators[i].opcode) {
        word = multi_operators[i].operator;
        break;
      }
    }
    if (word) {
      buf = malloc(strlen(word) + 1);
      assert(buf);
      strcpy(buf, word);
    } else {
      buf = malloc(2);
      assert(buf);
      buf[0] = token->oval;
      buf[1] = 0;
    }
  } else {
    switch (token->type) {
      case TOKEN_UNDEFINED:
        word = "?";
        break;
      case TOKEN_IDENTIFIER:
      case TOKEN_STRING_LITERAL:
        word = token->sval;
        break;
      case TOKEN_IF:
        word = "if";
        break;
      case TOKEN_ELSE:
        word = "else";
        break;
      case TOKEN_WHILE:
        word = "while";
        break;
      case TOKEN_BREAK:
        word = "break";
        break;
      case TOKEN_CONTINUE:
        word = "continue";
        break;
      case TOKEN_DO:
        word = "do";
        break;
      case TOKEN_RETURN:
        word = "return";
        break;
      default:
        word = "?";
    }

    buf = malloc(strlen(word) + 1);
    strcpy(buf, word);
  }

  return buf;
}

static void print_token(LexToken_T *token) {

  printf("{TYPE: %d | WORD: %s | LINE: %d}\n", token->type, token->as_string, token->lineno); 

}

static void print_tokens(LexToken_T *head) {
  for (LexToken_T *t = head; t != NULL; t = t->next) {
    print_token(t);
  }
}

static LexToken_T *make_token(LexState_T *L, LexTokenType_T type, void *v) {

  LexToken_T *token = malloc(sizeof(LexToken_T));
  assert(token);

  token->lineno = L->lineno;
  token->type = type;
  token->sval_ownership = false;
  token->next = NULL;

  /* assign token value.  builtin keywords get no value */
  switch (token->type) {
    case TOKEN_INTEGER:
    case TOKEN_CHARACTER_LITERAL:
      token->ival = *(int64_t *)v;
      break;
    case TOKEN_FLOAT:
      token->fval = *(double *)v;
      break;
    case TOKEN_IDENTIFIER:
    case TOKEN_STRING_LITERAL:
      token->sval_ownership = true;
      token->sval = (char *)v;
      break;
    case TOKEN_OPERATOR:
      token->oval = *(uint8_t *)v;
      break;
    default:
      break;
  }
  token->as_string = token_tostring(token);

  /* insert token into main token list */
  if (L->tokens == NULL) {
    L->tokens = token;
    L->backtoken = token;
  } else {
    L->backtoken->next = token;
    L->backtoken = token;
  }

  return token;

}

static void free_token(LexToken_T **token) {

  assert(token && *token);

  if ((*token)->sval_ownership) {
    free((*token)->sval);
  }
  free((*token)->as_string);
  free(*token);
  *token = NULL;

}

/* returns 0 for not on a number, 1 for integer, 2 for float */
static int on_number(LexState_T *L) {

  if (!isdigit(at(L))) {
    return NOT_ON_NUMBER;
  }

  int p = 1;
  while (true) {
    int look = peek(L, p);
    if (look == EOF) {
      return ON_INTEGER;
    } else if (look == '.') {
      if (!isdigit(peek(L, p + 1))) {
        lex_err(L, "malformed floating point number");
      }
      return ON_FLOAT;
    } else if (!isdigit(look)) {
      return ON_INTEGER;
    }

    p++;
  };

}

static void read_float(LexState_T *L) {

  char fltbuf[MAX_FLOAT_LENGTH]; 
  bool has_seen_decimal = false;

  for (int i = 0; i < MAX_FLOAT_LENGTH - 1; i++) {
    if (i == MAX_FLOAT_LENGTH - 2) {
      lex_err(L, "float literal is too long");
    }
    if (at(L) == '.') {
      if (has_seen_decimal) {
        lex_err(L, "malformed floating point literal");
      }
      has_seen_decimal = true;
    } else if (!isdigit(at(L))) {
      fltbuf[i] = 0;
      break;
    }
    fltbuf[i] = (char)get(L);
  }

  double value = strtod(fltbuf, NULL);

  make_token(L, TOKEN_FLOAT, &value);
}

static void read_integer(LexState_T *L) {

  char intbuf[MAX_INTEGER_LENGTH]; 

  for (int i = 0; i < MAX_INTEGER_LENGTH - 1; i++) {
    if (i == MAX_INTEGER_LENGTH - 2) {
      lex_err(L, "integer literal is too long");
    }
    if (!isdigit(at(L))) {
      intbuf[i] = 0;
      break;
    }
    intbuf[i] = (char)get(L);
  }

  int64_t value = atoll(intbuf);

  make_token(L, TOKEN_INTEGER, &value);

}

static void read_identifier(LexState_T *L) {

  size_t ident_len = 1;

  /* first get the length of the identifier.  start on 2nd character */
  while (true) {
    int look = peek(L, ident_len);
    if (isspace(look) || (look != '_' && !isalnum(look))) {
      break;
    }
    ident_len++;
  }

  /* copy identifier into a buffer */
  char *word_buf = malloc(ident_len + 1);
  assert(word_buf);
  strncpy(word_buf, &L->contents[L->index], ident_len);
  word_buf[ident_len] = 0;
  advance(L, ident_len);

  make_token(L, TOKEN_IDENTIFIER, word_buf);

}

static void read_operator(LexState_T *L) {

  char *opbuf;
  uint8_t code;
  bool found_match = false;

  for (int i = 0; i < sizeof(multi_operators)/sizeof(multi_operators[0]); i++) {
    const char *operator = multi_operators[i].operator;
    if (!strncmp(&L->contents[L->index], operator, strlen(operator))) {
      found_match = true;
      opbuf = malloc(strlen(operator) + 1);
      code = multi_operators[i].opcode;
      assert(opbuf);
      strcpy(opbuf, operator);
      break;
    }
  }

  if (found_match) {
    advance(L, strlen(opbuf));
  } else {
    opbuf = malloc(2);
    assert(opbuf);
    opbuf[0] = get(L);
    opbuf[1] = 0;
    code = opbuf[0];
  }

  free(opbuf);

  make_token(L, TOKEN_OPERATOR, &code);

}

static void read_string_literal(LexState_T *L) {

  char *litbuf;
  size_t buflen = 0;

  /* jump over opening quote */
  advance(L, 1);

  /* save original index for copying later */
  size_t start_index = L->index;

  while (true) {
    int look = get(L);
    if (look == EOF) {
      lex_err(L, "unbounded string literal");
    } else if (look == '"') {
      break;
    };
    buflen++;
  }

  /* jump over closing quote */
  advance(L, 1);

  /* load buffer */
  litbuf = malloc(buflen + 1);
  assert(litbuf);
  strncpy(litbuf, &L->contents[start_index], buflen);

  make_token(L, TOKEN_STRING_LITERAL, litbuf);

}

static void read_character_literal(LexState_T *L) {

  /* jump over opening quote */
  advance(L, 1);

  int64_t value = get(L);

  /* we'd better be on a closing quote */
  if (get(L) != '\'') {
    lex_err(L, "malformed character literal");
  }

  make_token(L, TOKEN_CHARACTER_LITERAL, &value);

}

static LexState_T *init_lexstate(const char *filename) {

  LexState_T *L = malloc(sizeof(LexState_T));
  assert(L);

  L->filename = malloc(strlen(filename) + 1);
  assert(L->filename);
  strcpy(L->filename, filename);
  L->tokens = NULL;
  L->backtoken = NULL;
  L->index = 0;
  L->lineno = 1;

  FILE *infile = fopen(filename, "rb");
  if (!infile) {
    fprintf(stderr, "couldn't open file for reading\n");
    exit(EXIT_FAILURE);
  }

  /* read file into contents buffer */
  fseek(infile, 0, SEEK_END);
  L->flen = ftell(infile);
  fseek(infile, 0, SEEK_SET);
  L->contents = malloc(L->flen + 1);
  assert(L->contents);
  (void)fread(L->contents, 1, L->flen, infile);
  L->contents[L->flen] = 0;
  fclose(infile);

  return L;

}

void lex_cleanup(LexState_T **L) {

  assert(L && *L);

  LexToken_T *token = (*L)->tokens;
  LexToken_T *next;
  while (token) {
    next = token->next;
    free_token(&token);
    token = next;
  }

  free((*L)->filename);
  free((*L)->contents);
  free(*L);
  *L = NULL;

}

LexState_T *lex_file(const char *filename) {

  LexState_T *L = init_lexstate(filename);

  /* main lexing loop */
  while (true) {
    int c = at(L);
    if (c == EOF) {
      break;
    }

    /* check for whitespace */
    if (c == ' ' || c == '\t') {
      advance(L, 1);
      continue;
    }

    /* check for newline */
    if (c == '\n') {
      L->lineno++;
      advance(L, 1);
    }

    /* check if we're on some sort of number literal */
    int number_code = on_number(L);
    if (number_code == ON_INTEGER) {
      read_integer(L);
    } else if (number_code == ON_FLOAT) {
      read_float(L);
    }

    /* check for builtin keywords */
    else if (is_on_literal(L, "if")) {
      make_token(L, TOKEN_IF, NULL);
      advance(L, strlen("if"));
    } else if (is_on_literal(L, "else")) {
      make_token(L, TOKEN_ELSE, "else");
      advance(L, strlen("else"));
    } else if (is_on_literal(L, "while")) {
      make_token(L, TOKEN_WHILE, NULL);
      advance(L, strlen("while"));
    } else if (is_on_literal(L, "break")) {
      make_token(L, TOKEN_BREAK, NULL);
      advance(L, strlen("break"));
    } else if (is_on_literal(L, "continue")) {
      make_token(L, TOKEN_CONTINUE, NULL);
      advance(L, strlen("continue"));
    } else if (is_on_literal(L, "do")) {
      make_token(L, TOKEN_DO, NULL);
      advance(L, strlen("do"));
    } else if (is_on_literal(L, "return")) {
      make_token(L, TOKEN_RETURN, NULL);
      advance(L, strlen("return"));
    }

    /* check for string literal */
    else if (at(L) == '"') {
      read_string_literal(L);
    }

    /* check for character literal */
    else if (at(L) == '\'') {
      read_character_literal(L);
    }

    /* check for operator */
    else if (ispunct(at(L))) {
      read_operator(L);
    }

    /* check for identifier */
    else if (isalpha(at(L)) || at(L) == '_') {
      read_identifier(L);
    }

  }

  return L;
}
