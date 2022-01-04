#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "checkAnswer.h"

int checkAnswer(char* userAnswer, char* ans, char* first, char* second,
                char* third, char* fourth) {
  char* actualAns =
      (!strcmp(ans, first)) ? "A" : (!strcmp(ans, second))
                                        ? "B"
                                        : (!strcmp(ans, third)) ? "C" : "D";
  if (!strcmp(userAnswer, actualAns))
    return 1;
  else
    return 0;
}