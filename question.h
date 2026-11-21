#ifndef SAMATH_QUESTION_H
#define SAMATH_QUESTION_H

#define QUESTION_OPTIONS 4
#define QUESTION_EXPRESSION_MAX 256
#define QUESTION_NUMBER_MAX 24
/* The longest line parseQuestion can accept, with its line ending. */
#define QUESTION_LINE_MAX 512
/* The longest working there is room for. A task has at most 12 numbers, so
   its working has at most 12 steps, the task and the answer included, and
   the step with n numbers left has n of them. A number takes at most 8 bytes
   with its brackets, as "(-10000)", its operator 1 and a pair of brackets
   added for the order of working 2, so the 78 numbers over the 12 steps take
   at most 11 * 78 = 858 bytes, and the " = " between steps 33 more. */
#define QUESTION_WORKING_MAX 1024

/* One question as SaMathAPI sends it. Options and answer stay text, exactly
   as received, so what is shown is what the server wrote. */
typedef struct {
  char expression[QUESTION_EXPRESSION_MAX];
  char options[QUESTION_OPTIONS][QUESTION_NUMBER_MAX];
  char answer[QUESTION_NUMBER_MAX];
  /* The steps the answer is worked out in, from the expression to the answer,
     joined by " = ", or "" when the server did not send them. */
  char working[QUESTION_WORKING_MAX];
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

/* Reads the tasks of a SaMathAPI answer to `working=true`, one object or an
   array of them, as in
   {"task":[3,"*",4],"answer":12,"quizOptions":[10,12,11,9],"working":["3*4","12"]}.
   Each task is read as parseQuestion reads its line, and keeps its working
   when that is made of expression characters, spaces and equals signs and
   fits. A task that does not read as a question is skipped; text that is not
   JSON ends the reading. Returns how many were read, up to `max`. */
int parseWorkedQuestions(const char* text, Question* out, int max);

#endif
