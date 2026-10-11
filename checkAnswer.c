#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "checkAnswer.h"

int checkAnswer(const char* userAnswer, const char* ans, const char* first,
                const char* second, const char* third, const char* fourth) {
  if (!userAnswer || !ans || !first || !second || !third || !fourth) return 0;
  const char* actualAns =
      (!strcmp(ans, first)) ? "A" : (!strcmp(ans, second))
                                        ? "B"
                                        : (!strcmp(ans, third)) ? "C" : "D";
  /* "a" is as good an answer as "A". */
  return toupper((unsigned char)userAnswer[0]) == actualAns[0] &&
         userAnswer[1] == '\0';
}
