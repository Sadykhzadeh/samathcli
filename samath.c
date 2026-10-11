#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "checkAnswer.h"
#include "getQuestion.h"
#include "input.h"

#define welcomeText "👋 Welcome to SaMath System!"
#define noArgs \
  "😅 Please, provide me a number of questions that I should output! :3"
#define badArgs "😅 That has to be a whole number of questions, 1 or more! :3"
#define apiUnreachable \
  "😔 I could not reach SaMathAPI. Is it running? See SAMATH_API_URL.\n"
#define noErrors "🥳 Fantastic, you haven't got any mistakes!"
#define bye "\n😉 Have a nice day!"

typedef struct {
  long number;
  Question question;
} Mistake;

/* Returns the requested question count, or -1 when the argument is not a
   whole number of 1 or more. */
static long parseQuestionCount(const char* text) {
  char* end;
  errno = 0;
  long count = strtol(text, &end, 10);
  if (errno || end == text || *end || count < 1 || count > INT_MAX) return -1;
  return count;
}

/* An escape sequence, not a `clear` process for every question, and only for
   a person at a terminal: a pipe or a file should not get it. */
static void clearScreen(void) {
  if (isatty(STDOUT_FILENO)) fputs("\033[H\033[2J", stdout);
}

/* Shows the question and reads the answer. Returns 1 when it was right, 0 when
   it was not, and -1 when the person stopped or the input ended. */
static int askQuestion(long number, const Question* q) {
  inputDiscard();
  clearScreen();
  printf("%ld\t🤔 Solve this expression: %s\n\n", number, q->expression);
  printf("\tA) %s", q->options[0]);
  printf("\t\tB) %s\n", q->options[1]);
  printf("\tC) %s", q->options[2]);
  printf("\t\tD) %s\n\n\t", q->options[3]);
  printf("Answer (A-D, Q to stop): ");
  fflush(stdout);

  int choice = readChoice();
  if (choice == CHOICE_QUIT) return -1;

  char letter[2] = {(char)('A' + choice), '\0'};
  return checkAnswer(letter, q->answer, q->options[0], q->options[1],
                     q->options[2], q->options[3]);
}

int main(int n, char** a) {
  puts(welcomeText);
  if (!(n - 1)) return puts(noArgs), 0;
  long numberOfQuestions = parseQuestionCount(a[1]);
  if (numberOfQuestions < 0) return puts(badArgs), 1;

  Quiz quiz;
  quizOpen(&quiz, numberOfQuestions);
  inputStart();

  Mistake* mistakes = NULL;
  long numberOfMistakes = 0, room = 0, answered = 0;
  for (long i = 0; i < numberOfQuestions; i++) {
    Question question;
    if (quizNext(&quiz, &question) < 0) {
      fputs(apiUnreachable, stderr);
      free(mistakes);
      return 1;
    }
    int right = askQuestion(i + 1, &question);
    if (right < 0) break;
    answered++;
    if (right) continue;

    if (numberOfMistakes == room) {
      room = room ? room * 2 : 8;
      Mistake* grown = realloc(mistakes, (size_t)room * sizeof *mistakes);
      if (!grown) {
        free(mistakes);
        return 1;
      }
      mistakes = grown;
    }
    mistakes[numberOfMistakes].number = i + 1;
    mistakes[numberOfMistakes].question = question;
    numberOfMistakes++;
  }

  clearScreen();
  if (answered < numberOfQuestions)
    printf("✋ You stopped after %ld of %ld questions.\n\n", answered,
           numberOfQuestions);
  if (numberOfMistakes) {
    printf("😔 You have %ld mistakes!\n\n", numberOfMistakes);
    for (long i = 0; i < numberOfMistakes; i++)
      printf("Task %ld. %s\t= %s\n", mistakes[i].number,
             mistakes[i].question.expression, mistakes[i].question.answer);
  } else if (answered)
    puts(noErrors);
  puts(bye);
  free(mistakes);
  return 0;
}
