#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "checkAnswer.h"
#include "getQuestion.h"
#include "question.h"

#define apiUrlEnv "SAMATH_API_URL"
#define apiUrlDefault "http://localhost:3000/gen?c=true"

/* The URL ends up inside a shell command, so anything that could end the
   quoting or start a second command is turned away. */
static int isSafeUrl(const char* url) {
  static const char allowed[] =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
      "-._~:/?#[]@!$&()*+,;=%";
  for (const char* c = url; *c; c++)
    if (!strchr(allowed, *c)) return 0;
  return *url != '\0';
}

/* A terminal that refuses to clear is not worth failing a question over. */
static void clearScreen(void) {
  if (system("clear")) return;
}

static int fetchQuestion(void) {
  const char* url = getenv(apiUrlEnv);
  if (!url || !isSafeUrl(url)) url = apiUrlDefault;

  char command[512];
  int length = snprintf(command, sizeof command,
                        "curl -fsS --max-time 10 '%s' -o .tmp.samath", url);
  if (length < 0 || (size_t)length >= sizeof command) return 0;
  if (system(command)) return 0;

  clearScreen();
  return 1;
}

int displayQuestion(int number) {
  if (!fetchQuestion()) return -1;

  FILE* content = fopen(".tmp.samath", "r");
  if (!content) return -1;
  char line[512];
  char* got = fgets(line, sizeof line, content);
  fclose(content);
  if (!got) return -1;

  Question q;
  if (!parseQuestion(line, &q)) return -1;

  printf("%d\t🤔 Solve this expression: %s\n\n", number, q.expression);
  printf("\tA) %s", q.options[0]);
  printf("\t\tB) %s\n", q.options[1]);
  printf("\tC) %s", q.options[2]);
  printf("\t\tD) %s\n\n\t", q.options[3]);

  char userAnswer[8];
  printf("Answer: ");
  if (scanf("%7s", userAnswer) != 1) return -1;
  clearScreen();

  if (checkAnswer(userAnswer, q.answer, q.options[0], q.options[1],
                  q.options[2], q.options[3]))
    return 1;

  FILE* errors = fopen(".tmp.errors", "a");
  if (errors) {
    fprintf(errors, "Task %d. %s\t= %s\n", number, q.expression, q.answer);
    fclose(errors);
  }
  return 0;
}
