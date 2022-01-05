#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "getQuestion.h"

#define welcomeText "👋 Welcome to SaMath System!"
#define noArgs \
  "😅 Please, provide me a number of questions that I should output! :3"
#define noErrors "🥳 Fantastic, you haven't got any mistakes!"
#define bye "\n😉 Have a nice day!"

int main(int n, char** a) {
  puts(welcomeText);
  if (!(n - 1)) return puts(noArgs), 0;
  int numberOfCorrect = 0, numberOfQuestions = atoi(a[1]);
  for (int i = 0; i < numberOfQuestions; i++)
    numberOfCorrect += displayQuestion(i + 1);
  if (numberOfCorrect != numberOfQuestions) {
    printf("😔 You have %d mistakes!\n\n", numberOfQuestions - numberOfCorrect);
    FILE* errorsFile = fopen(".tmp.errors", "r");
    char errorsArr[100];
    for (int j = 0; j < numberOfQuestions - numberOfCorrect; j++) {
      fgets(errorsArr, 99, errorsFile);
      printf("%s", errorsArr);
    }
    fclose(errorsFile);
  } else
    puts(noErrors);
  puts(bye);
  system("rm .tmp.*");
  return 0;
}