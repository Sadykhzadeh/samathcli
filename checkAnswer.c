#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "checkAnswer.h"

int checkAnswer(char* userAnswer, char* ans, char* first, char* second,
                char* third, char* fourth) {
  if (!userAnswer || !ans || !first || !second || !third || !fourth) return 0;
  char* actualAns =
      (!strcmp(ans, first)) ? "A" : (!strcmp(ans, second))
                                        ? "B"
                                        : (!strcmp(ans, third)) ? "C" : "D";
  /* "a" is as good an answer as "A". */
  return toupper((unsigned char)userAnswer[0]) == actualAns[0] &&
         userAnswer[1] == '\0';
}
