#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
  "  -s, --seed NUMBER    ask the same questions every time\n"            \
  "                       (0 to 4294967295; the API has to take a seed)\n" \
  "  -p, --practice       ask the ones you got wrong again until they are\n" \
  "                       right\n"                                        \
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
   the server is not the person's time. A `note` is put on a line of its own
   above the question. */
static int askQuestion(long number, const Question* q, int limit,
                       double* spent, const char* note) {
  inputDiscard();
  clearScreen();
  if (note) printf("%s\n\n", note);
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

/* Asks the mistakes again, round after round, until each has been put right.
   Sets `*rounds` to how many rounds that took. Returns how many were still
   wrong when the person stopped, 0 when all were put right, or -1 when there
   was no memory for it. */
static long practise(const Mistake* mistakes, long count, int limit,
                     long* rounds) {
  Mistake* pending = malloc((size_t)count * sizeof *pending);
  if (!pending) return -1;
  memcpy(pending, mistakes, (size_t)count * sizeof *pending);

  long left = count;
  double notCounted = 0; /* practice is not part of the time on the result */
  *rounds = 0;
  while (left > 0) {
    (*rounds)++;
    long kept = 0;
    for (long i = 0; i < left; i++) {
      char note[96];
      snprintf(note, sizeof note, "🔁 Practice, round %ld: %ld to go", *rounds,
               left - i);
      int result = askQuestion(pending[i].number, &pending[i].question, limit,
                               &notCounted, note);
      if (result == ASKED_STOP) {
        free(pending);
        return kept + (left - i);
      }
      if (result != ASKED_RIGHT) pending[kept++] = pending[i];
    }
    left = kept;
  }
  free(pending);
  return 0;
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
    case ARGS_BAD_SEED:
      return fprintf(stderr,
                     "😅 A seed is a whole number from 0 to %lld, not "
                     "\"%s\"! :3\n",
                     SEED_MAX, culprit),
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
  quizOpen(&quiz, options.questions, options.seed);
  inputStart();

  Mistake* mistakes = NULL;
  long numberOfMistakes = 0, room = 0, answered = 0;
  double thinking = 0;
  /* A server lost after the first question ends the run there, and what was
     answered until then still gets its result. */
  int lost = 0;
  for (long i = 0; i < options.questions; i++) {
    Question question;
    if (quizNext(&quiz, &question) < 0) {
      if (i == 0) {
        fputs(apiUnreachable, stderr);
        return 1;
      }
      lost = 1;
      break;
    }
    int result =
        askQuestion(i + 1, &question, options.seconds, &thinking, NULL);
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

  /* A run that was stopped is over; one that was finished goes on to the
     mistakes if that was asked for. */
  long stillWrong = 0, rounds = 0;
  int practised = options.practice && numberOfMistakes &&
                  answered == options.questions;
  if (practised) {
    stillWrong = practise(mistakes, numberOfMistakes, options.seconds, &rounds);
    if (stillWrong < 0) {
      free(mistakes);
      return 1;
    }
  }

  clearScreen();
  if (lost)
    printf("😔 SaMathAPI stopped answering after %ld of %ld questions.\n\n",
           answered, options.questions);
  else if (answered < options.questions)
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
  if (practised && !stillWrong)
    printf("\n🔁 Practice: every mistake put right, in %ld %s.\n", rounds,
           rounds == 1 ? "round" : "rounds");
  else if (practised)
    printf("\n🔁 Practice stopped with %ld of %ld still to put right.\n",
           stillWrong, numberOfMistakes);
  puts(bye);
  free(mistakes);
  if (lost) fputs(apiUnreachable, stderr);
  return lost;
}
