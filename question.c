#include <string.h>
#include "question.h"

static int isExpressionChar(char c) {
  return (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '*' ||
         c == '/' || c == '(' || c == ')';
}

static int isNumber(const char* text) {
  const char* c = text;
  if (*c == '-') c++;
  if (!*c) return 0;
  for (; *c; c++)
    if (*c < '0' || *c > '9') return 0;
  return 1;
}

/* Copies the text up to the next character of `stops` (or the end) into `out`
   and moves `*line` onto that character. Returns 0 when the field is empty or
   does not fit. */
static int takeField(const char** line, const char* stops, char* out,
                     size_t capacity) {
  size_t length = strcspn(*line, stops);
  if (length == 0 || length >= capacity) return 0;
  memcpy(out, *line, length);
  out[length] = '\0';
  *line += length;
  return 1;
}

/* Moves past one `separator`; 0 when the line has something else there. */
static int skip(const char** line, char separator) {
  if (**line != separator) return 0;
  (*line)++;
  return 1;
}

int parseQuestion(const char* line, Question* out) {
  Question q;
  const char* at = line;

  if (!takeField(&at, " ", q.expression, sizeof q.expression)) return 0;
  for (const char* c = q.expression; *c; c++)
    if (!isExpressionChar(*c)) return 0;
  if (!skip(&at, ' ')) return 0;

  for (int i = 0; i < QUESTION_OPTIONS; i++) {
    int last = i == QUESTION_OPTIONS - 1;
    if (!takeField(&at, last ? " " : ",", q.options[i], sizeof q.options[i]))
      return 0;
    if (!isNumber(q.options[i])) return 0;
    if (!skip(&at, last ? ' ' : ',')) return 0;
  }

  if (!takeField(&at, " \t\r\n", q.answer, sizeof q.answer)) return 0;
  if (!isNumber(q.answer)) return 0;
  at += strspn(at, " \t\r\n");
  if (*at) return 0;

  int found = 0;
  for (int i = 0; i < QUESTION_OPTIONS; i++)
    if (!strcmp(q.options[i], q.answer)) found = 1;
  if (!found) return 0;

  *out = q;
  return 1;
}

int parseQuestions(const char* text, Question* out, int max) {
  int count = 0;
  while (*text && count < max) {
    size_t length = strcspn(text, "\n");
    if (length < QUESTION_LINE_MAX) {
      char line[QUESTION_LINE_MAX];
      memcpy(line, text, length);
      line[length] = '\0';
      if (parseQuestion(line, &out[count])) count++;
    }
    text += length;
    if (*text == '\n') text++;
  }
  return count;
}
