#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include "getQuestion.h"

#define welcomeText "👋 Welcome to SaMath System!"
#define noArgs \
  "😅 Please, provide me a number of questions that I should output! :3"
#define badArgs "😅 That has to be a whole number of questions, 1 or more! :3"
#define apiUnreachable \
  "😔 I could not reach SaMathAPI. Is it running? See SAMATH_API_URL.\n"
#define noErrors "🥳 Fantastic, you haven't got any mistakes!"
#define bye "\n😉 Have a nice day!"

static void cleanTempFiles() {
  remove(".tmp.samath");
  remove(".tmp.errors");
}

/* Returns the requested question count, or -1 when the argument is not a
   whole number of 1 or more. */
static long parseQuestionCount(const char* text) {
  char* end;
  errno = 0;
  long count = strtol(text, &end, 10);
  if (errno || end == text || *end || count < 1 || count > INT_MAX) return -1;
  return count;
}

int main(int n, char** a) {
  puts(welcomeText);
  if (!(n - 1)) return puts(noArgs), 0;
  long numberOfQuestions = parseQuestionCount(a[1]);
  if (numberOfQuestions < 0) return puts(badArgs), 1;

  /* A fresh run must not count mistakes left behind by an earlier one. */
  cleanTempFiles();

  int numberOfCorrect = 0;
  for (long i = 0; i < numberOfQuestions; i++) {
    int answer = displayQuestion((int)(i + 1));
    if (answer < 0) {
      fputs(apiUnreachable, stderr);
      cleanTempFiles();
      return 1;
    }
    numberOfCorrect += answer;
  }

  long numberOfMistakes = numberOfQuestions - numberOfCorrect;
  if (numberOfMistakes) {
    printf("😔 You have %ld mistakes!\n\n", numberOfMistakes);
    FILE* errorsFile = fopen(".tmp.errors", "r");
    if (errorsFile) {
      char errorsArr[256];
      while (fgets(errorsArr, sizeof errorsArr, errorsFile))
        printf("%s", errorsArr);
      fclose(errorsFile);
    }
  } else
    puts(noErrors);
  puts(bye);
  cleanTempFiles();
  return 0;
}
