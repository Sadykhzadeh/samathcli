#include <stdio.h>
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

  q.working[0] = '\0';
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

/* How deep the values of a reply may nest, counting the array of tasks as
   level 0: the tasks are at 1, their members at 2. */
#define JSON_DEPTH_MAX 4

static const char* jsonSpace(const char* text) {
  return text + strspn(text, " \t\r\n");
}

/* The string at `text`, which starts with its quote: `*begin` and `*length`
   are what lies between the quotes, escapes left as they were written. A
   backslash makes the character after it part of the string, so an escaped
   quote does not end it. Returns the text after the closing quote, or NULL. */
static const char* jsonString(const char* text, const char** begin,
                              size_t* length) {
  if (*text != '"') return NULL;
  const char* end = text + 1;
  while (*end && *end != '"') end += *end == '\\' && end[1] ? 2 : 1;
  if (*end != '"') return NULL;
  *begin = text + 1;
  *length = (size_t)(end - text - 1);
  return end + 1;
}

/* The whole number at `text`, an optional minus sign and digits, as the text
   of it and its length. Returns the text after it, or NULL when there is no
   such number or it has a fraction or an exponent. */
static const char* jsonNumber(const char* text, const char** begin,
                              size_t* length) {
  const char* digits = *text == '-' ? text + 1 : text;
  size_t size = strspn(digits, "0123456789");
  if (!size || (digits[size] && strchr(".eE", digits[size]))) return NULL;
  *begin = text;
  *length = (size_t)(digits - text) + size;
  return text + *length;
}

/* Whether the string `begin` of `length` bytes is `name`. */
static int isKey(const char* begin, size_t length, const char* name) {
  return strlen(name) == length && !strncmp(begin, name, length);
}

/* The value at `text`, of any kind, skipped. Returns the text after it, or
   NULL when it is not well-formed or nests deeper than JSON_DEPTH_MAX from
   `depth`. */
static const char* jsonSkip(const char* text, int depth) {
  text = jsonSpace(text);
  const char* begin;
  size_t length;
  if (*text == '"') return jsonString(text, &begin, &length);
  if (*text == '[' || *text == '{') {
    if (depth >= JSON_DEPTH_MAX) return NULL;
    int object = *text == '{';
    char close = object ? '}' : ']';
    text = jsonSpace(text + 1);
    if (*text == close) return text + 1;
    for (;;) {
      if (object) {
        text = jsonString(jsonSpace(text), &begin, &length);
        if (!text) return NULL;
        text = jsonSpace(text);
        if (*text != ':') return NULL;
        text++;
      }
      text = jsonSkip(text, depth + 1);
      if (!text) return NULL;
      text = jsonSpace(text);
      if (*text == close) return text + 1;
      if (*text != ',') return NULL;
      text++;
    }
  }
  size_t size = strspn(text, "-+.0123456789eE");
  if (size) return text + size;
  if (!strncmp(text, "true", 4) || !strncmp(text, "null", 4)) return text + 4;
  if (!strncmp(text, "false", 5)) return text + 5;
  return NULL;
}

/* The array of strings and numbers at `text`, their text put together with
   `separator` between them in `out`. A member that does not fit leaves
   `*fits` at 0 and is not kept, but the array is still read to its end.
   Returns the text after the array, or NULL when it is not well-formed. */
static const char* jsonList(const char* text, const char* separator, char* out,
                            size_t capacity, int* fits) {
  text = jsonSpace(text);
  if (*text != '[') return NULL;
  out[0] = '\0';
  size_t used = 0;
  text = jsonSpace(text + 1);
  if (*text == ']') return text + 1;
  for (;;) {
    const char* begin;
    size_t length;
    text = jsonSpace(text);
    if (*text == '"')
      text = jsonString(text, &begin, &length);
    else
      text = jsonNumber(text, &begin, &length);
    if (!text) return NULL;
    size_t gap = used ? strlen(separator) : 0;
    if (used + gap + length + 1 > capacity) {
      *fits = 0;
    } else {
      memcpy(out + used, separator, gap);
      used += gap;
      memcpy(out + used, begin, length);
      used += length;
      out[used] = '\0';
    }
    text = jsonSpace(text);
    if (*text == ']') return text + 1;
    if (*text != ',') return NULL;
    text++;
  }
}

/* Reads the task object at `text` into `out`, and sets `*valid` to 1 when it
   holds a question parseQuestion accepts, or 0 when it does not. A value that
   is not an object is skipped, and is not a task. Returns the text after the
   value, or NULL when it is not well-formed. */
static const char* readTask(const char* text, Question* out, int* valid) {
  char task[QUESTION_LINE_MAX], options[QUESTION_LINE_MAX];
  char working[QUESTION_WORKING_MAX];
  int taskFits = 1, optionsFit = 1, workingFits = 1;
  task[0] = options[0] = working[0] = '\0';
  const char* answer = "";
  size_t answerLength = 0;
  *valid = 0;

  text = jsonSpace(text);
  if (*text != '{') return jsonSkip(text, 1);
  text = jsonSpace(text + 1);
  if (*text == '}') return text + 1;
  for (;;) {
    const char* key;
    size_t keyLength;
    text = jsonString(jsonSpace(text), &key, &keyLength);
    if (!text) return NULL;
    text = jsonSpace(text);
    if (*text != ':') return NULL;
    text = jsonSpace(text + 1);
    /* A value of the wrong kind is skipped, and only the part of the task it
       was for is lost: a reply is only broken when its text is. */
    const char* next = NULL;
    if (isKey(key, keyLength, "task")) {
      next = jsonList(text, "", task, sizeof task, &taskFits);
      if (!next) taskFits = 0;
    } else if (isKey(key, keyLength, "quizOptions")) {
      next = jsonList(text, ",", options, sizeof options, &optionsFit);
      if (!next) optionsFit = 0;
    } else if (isKey(key, keyLength, "working")) {
      next = jsonList(text, " = ", working, sizeof working, &workingFits);
      if (!next) workingFits = 0;
    } else if (isKey(key, keyLength, "answer")) {
      next = jsonNumber(text, &answer, &answerLength);
      if (!next) {
        answer = "";
        answerLength = 0;
      }
    }
    if (!next) next = jsonSkip(text, 2);
    text = next;
    if (!text) return NULL;
    text = jsonSpace(text);
    if (*text == '}') break;
    if (*text != ',') return NULL;
    text++;
  }

  char line[QUESTION_LINE_MAX];
  if (!taskFits || !optionsFit) return text + 1;
  int written = snprintf(line, sizeof line, "%s %s %.*s", task, options,
                         (int)answerLength, answer);
  if (written < 0 || (size_t)written >= sizeof line) return text + 1;
  if (!parseQuestion(line, out)) return text + 1;
  if (workingFits && strspn(working, "0123456789+-*/()= ") == strlen(working))
    strcpy(out->working, working);
  *valid = 1;
  return text + 1;
}

int parseWorkedQuestions(const char* text, Question* out, int max) {
  int count = 0;
  text = jsonSpace(text);
  int listed = *text == '[';
  if (listed) text = jsonSpace(text + 1);
  if (listed && *text == ']') return 0;
  while (text && count < max) {
    int valid;
    text = readTask(text, &out[count], &valid);
    if (!text) break;
    if (valid) count++;
    if (!listed) break;
    text = jsonSpace(text);
    if (*text != ',') break;
    text++;
  }
  return count;
}
