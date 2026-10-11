#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "checkAnswer.h"
#include "getQuestion.h"
#include "input.h"
#include "options.h"
#include "report.h"

#define welcomeText "👋 Welcome to SaMath System!"
#define noArgs \
  "😅 Please, provide me a number of questions that I should output! :3"
#define badArgs "😅 That has to be a whole number of questions, 1 or more! :3\n"
#define apiUnreachable \
  "😔 I could not reach SaMathAPI. Is it running? See SAMATH_API_URL.\n"
#define noErrors "🥳 Fantastic, you haven't got any mistakes!"
#define bye "\n😉 Have a nice day!"

#define helpText                                                          \
  "usage: samath [options] QUESTIONS\n"                                   \
  "\n"                                                                    \
  "  QUESTIONS            how many questions to ask, 1 or more\n"         \
  "  -t, --time SECONDS   seconds to answer each question (1 to 3600);\n" \
  "                       one that runs out counts as a mistake\n"        \
  "  -h, --help           show this\n"                                    \
  "\n"                                                                    \
  "Answer with A, B, C or D, no Enter needed. Q stops the run.\n"         \
  "SAMATH_API_URL says where the questions come from.\n"

typedef struct {
  long number;
  Question question;
  int timedOut;
} Mistake;

enum { ASKED_WRONG = 0, ASKED_RIGHT = 1, ASKED_STOP = -1, ASKED_TIMEOUT = -2 };

static int atTerminal;

/* An escape sequence, not a `clear` process for every question, and only for
   a person at a terminal: a pipe or a file gets a blank line instead. */
static void clearScreen(void) {
  if (atTerminal)
    fputs("\033[H\033[2J", stdout);
  else
    putchar('\n');
}

/* The line the answer is typed on. With a time limit it counts down in place
   at a terminal; elsewhere it is written once. */
static void showPrompt(int left) {
  const char* home = atTerminal ? "\r" : "";
  const char* erase = atTerminal ? "\033[K" : "";
  if (left > 0)
    printf("%s\tAnswer (A-D, Q to stop) - %d s left: %s", home, left, erase);
  else
    printf("%s\tAnswer (A-D, Q to stop): %s", home, erase);
  fflush(stdout);
}

static void tick(int left) {
  if (atTerminal) showPrompt(left);
}

/* Shows the question and reads the answer, within `limit` seconds if that is
   not 0. Returns ASKED_RIGHT, ASKED_WRONG, ASKED_TIMEOUT, or ASKED_STOP when
   the person stopped or the input ended. The time from showing the question
   to the answer, or to the end of the limit, is added to `*spent`; waiting for
   the server is not the person's time. */
static int askQuestion(long number, const Question* q, int limit,
                       double* spent) {
  inputDiscard();
  clearScreen();
  printf("%ld\t🤔 Solve this expression: %s\n\n", number, q->expression);
  printf("\tA) %s", q->options[0]);
  printf("\t\tB) %s\n", q->options[1]);
  printf("\tC) %s", q->options[2]);
  printf("\t\tD) %s\n\n", q->options[3]);
  showPrompt(limit);

  double shown = monotonicSeconds();
  int choice = readChoice(limit, tick);
  if (choice == CHOICE_QUIT) return ASKED_STOP;
  *spent += monotonicSeconds() - shown;
  if (choice == CHOICE_TIMEOUT) return ASKED_TIMEOUT;

  char letter[2] = {(char)('A' + choice), '\0'};
  return checkAnswer(letter, q->answer, q->options[0], q->options[1],
                     q->options[2], q->options[3]);
}

int main(int argc, char** argv) {
  puts(welcomeText);
  Options options;
  const char* culprit;
  switch (parseArgs(argc, argv, &options, &culprit)) {
    case ARGS_OK:
      break;
    case ARGS_NONE:
      return puts(noArgs), 0;
    case ARGS_HELP:
      return fputs(helpText, stdout), 0;
    case ARGS_BAD_COUNT:
      return fputs(badArgs, stderr), 1;
    case ARGS_BAD_TIME:
      return fprintf(stderr,
                     "😅 A time limit is a whole number of seconds, 1 to %d, "
                     "not \"%s\"! :3\n",
                     TIME_LIMIT_MAX, culprit),
             1;
    case ARGS_BAD_OPTION:
      return fprintf(stderr,
                     "😅 I do not know the option \"%s\". See samath --help! "
                     ":3\n",
                     culprit),
             1;
    case ARGS_EXTRA:
      return fprintf(stderr,
                     "😅 One number of questions is enough, and \"%s\" is a "
                     "second one! :3\n",
                     culprit),
             1;
  }

  atTerminal = isatty(STDOUT_FILENO);
  Quiz quiz;
  quizOpen(&quiz, options.questions);
  inputStart();

  Mistake* mistakes = NULL;
  long numberOfMistakes = 0, room = 0, answered = 0;
  double thinking = 0;
  for (long i = 0; i < options.questions; i++) {
    Question question;
    if (quizNext(&quiz, &question) < 0) {
      fputs(apiUnreachable, stderr);
      free(mistakes);
      return 1;
    }
    int result = askQuestion(i + 1, &question, options.seconds, &thinking);
    if (result == ASKED_STOP) break;
    answered++;
    if (result == ASKED_RIGHT) continue;

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
    mistakes[numberOfMistakes].timedOut = result == ASKED_TIMEOUT;
    numberOfMistakes++;
  }

  clearScreen();
  if (answered < options.questions)
    printf("✋ You stopped after %ld of %ld questions.\n\n", answered,
           options.questions);
  if (numberOfMistakes) {
    printf("😔 You have %ld mistakes!\n\n", numberOfMistakes);
    for (long i = 0; i < numberOfMistakes; i++)
      printf("Task %ld. %s\t= %s%s\n", mistakes[i].number,
             mistakes[i].question.expression, mistakes[i].question.answer,
             mistakes[i].timedOut ? "\t(time ran out)" : "");
  } else if (answered)
    puts(noErrors);
  if (answered) {
    char stats[128];
    formatStats(answered - numberOfMistakes, answered, thinking, stats,
                sizeof stats);
    printf("\n📊 %s\n", stats);
  }
  puts(bye);
  free(mistakes);
  return 0;
}
