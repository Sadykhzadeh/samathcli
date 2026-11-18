#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "checkAnswer.h"

int checkAnswer(const char* userAnswer, const char* ans, const char* first,
                const char* second, const char* third, const char* fourth) {
  if (!userAnswer || !ans || !first || !second || !third || !fourth) return 0;
  const char* options[] = {first, second, third, fourth};
  /* "a" is as good an answer as "A". The letter is right when its option
     reads as the answer, so two options with the same value are both right. */
  int choice = toupper((unsigned char)userAnswer[0]) - 'A';
  return userAnswer[0] && !userAnswer[1] && choice >= 0 && choice < 4 &&
         !strcmp(options[choice], ans);
}
