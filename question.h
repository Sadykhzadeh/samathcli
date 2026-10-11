#ifndef SAMATH_QUESTION_H
#define SAMATH_QUESTION_H

#define QUESTION_OPTIONS 4
#define QUESTION_EXPRESSION_MAX 256
#define QUESTION_NUMBER_MAX 24
/* The longest line parseQuestion can accept, with its line ending. */
#define QUESTION_LINE_MAX 512

/* One question as SaMathAPI sends it. Options and answer stay text, exactly
   as received, so what is shown is what the server wrote. */
typedef struct {
  char expression[QUESTION_EXPRESSION_MAX];
  char options[QUESTION_OPTIONS][QUESTION_NUMBER_MAX];
  char answer[QUESTION_NUMBER_MAX];
} Question;

/* Reads one line of the API's CLI format,
   "<expression> <a>,<b>,<c>,<d> <answer>", for example "3*4-1 11,9,12,10 11".
   Returns 1 and fills `out`, or 0 when the line is anything else: the
   expression may only hold digits, + - * / and brackets, the four options and
   the answer have to be whole numbers, and the answer has to be one of the
   options. Whatever the server sends, nothing else reaches the terminal. */
int parseQuestion(const char* line, Question* out);

/* Reads every line of `text` that parseQuestion accepts into `out`, in order,
   up to `max` of them, and returns how many there were. Blank lines and lines
   that are not questions are skipped. */
int parseQuestions(const char* text, Question* out, int max);

#endif
