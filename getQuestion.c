#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "checkAnswer.h"
#include "getQuestion.h"

void fetchQuestion() {
  system("curl http://localhost:3000/gen?c=true -o .tmp.samath && clear");
}

int displayQuestion(int number) {
  fetchQuestion();

  FILE* content = fopen(".tmp.samath", "r");
  char stringOfContent[150];
  fgets(stringOfContent, 300, content);
  fclose(content);

  char* expression = strtok(stringOfContent, " ,");
  printf("%d\t🤔 Solve this expression: %s\n\n", number, expression);

  char* first = strtok(NULL, " ,");
  printf("\tA) %s", first);

  char* second = strtok(NULL, " ,");
  printf("\t\tB) %s\n", second);
  char* third = strtok(NULL, " ,");
  printf("\tC) %s", third);
  char* fourth = strtok(NULL, " ,");
  printf("\t\tD) %s\n\n\t", fourth);
  char* userAnswer;
  printf("Answer: ");
  scanf("%1s", userAnswer);
  char* ans = strtok(NULL, " ,");

  system("clear");
  FILE* errors = fopen(".tmp.errors", "a");
  fprintf(errors, "Task %d. %s\t= %s\n", number, expression, ans);
  fclose(errors);

  return (checkAnswer(userAnswer, ans, first, second, third, fourth) == 1) ? 1
                                                                           : 0;
}