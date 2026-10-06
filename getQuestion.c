#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "checkAnswer.h"
#include "getQuestion.h"

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
static void clearScreen() {
  if (system("clear")) return;
}

static int fetchQuestion() {
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
  char stringOfContent[512];
  char* line = fgets(stringOfContent, sizeof stringOfContent, content);
  fclose(content);
  if (!line) return -1;
  stringOfContent[strcspn(stringOfContent, "\r\n")] = '\0';

  char* expression = strtok(stringOfContent, " ,");
  char* first = strtok(NULL, " ,");
  char* second = strtok(NULL, " ,");
  char* third = strtok(NULL, " ,");
  char* fourth = strtok(NULL, " ,");
  char* ans = strtok(NULL, " ,");
  if (!expression || !first || !second || !third || !fourth || !ans) return -1;

  printf("%d\t🤔 Solve this expression: %s\n\n", number, expression);
  printf("\tA) %s", first);
  printf("\t\tB) %s\n", second);
  printf("\tC) %s", third);
  printf("\t\tD) %s\n\n\t", fourth);

  char userAnswer[8];
  printf("Answer: ");
  if (scanf("%7s", userAnswer) != 1) return -1;
  clearScreen();

  if (checkAnswer(userAnswer, ans, first, second, third, fourth)) return 1;

  FILE* errors = fopen(".tmp.errors", "a");
  if (errors) {
    fprintf(errors, "Task %d. %s\t= %s\n", number, expression, ans);
    fclose(errors);
  }
  return 0;
}
