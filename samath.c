#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "checkAnswer.h"
#include "getQuestion.h"
#include "input.h"
#include "mistakes.h"
#include "options.h"
#include "report.h"

#define welcomeText "👋 Welcome to SaMath System!"
#define noArgs                                                      \
  "😅 Please, provide me a number of questions that I should output! See " \
  "samath --help. :3\n"
#define badArgs "😅 That has to be a whole number of questions, 1 or more! :3\n"
#define apiUnreachable \
  "😔 I could not reach SaMathAPI. Is it running? See SAMATH_API_URL.\n"
#define apiRefused                                                  \
  "😔 SaMathAPI answered with an error instead of questions. See "   \
  "SAMATH_API_URL.\n"
#define apiNotQuestions                                             \
  "😔 SAMATH_API_URL answered, but not with questions. It has to " \
  "ask for them as text, as http://localhost:3000/gen?c=true does.\n"
#define fetching "⏳ Asking SaMathAPI for the questions..."
#define noCurl                                                          \
  "😔 I need curl to fetch the questions, and could not run it. Is it "  \
  "installed and on your PATH?\n"
#define apiTimedOut                                                     \
  "😔 SaMathAPI did not answer in time. It may be busy, or the network " \
  "slow. See SAMATH_API_URL.\n"
#define writeFailure "😔 I could not write the result out. Is the disk full?\n"
#define noMemory                                                          \
  "😔 I ran out of memory to keep the mistakes, so the run stopped there.\n"
#define noPractice                                                        \
  "😔 I ran out of memory for practice, so the mistakes were not asked "  \
  "again.\n"
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
  "  -l, --length N       how many numbers each question has (2 to 12)\n" \
  "  -b, --brackets       ask for questions with brackets\n"              \
  "  -m, --max N          no number further from zero than N (10-10000)\n" \
  "  -o, --operators LIST only these: add, subtract, multiply, divide,\n" \
  "                       separated by commas\n"                         \
  "  -n, --no-negatives   ask for questions with no number below zero\n" \
  "  -f, --feedback       say after each answer whether it was right\n"  \
  "  -h, --help           show this\n"                                    \
  "\n"                                                                    \
  "Short options can be joined: -pf, or -pt 30 with a time limit.\n"      \
  "Answer with A, B, C or D, no Enter needed. Q stops the run.\n"         \
  "SAMATH_API_URL says where the questions come from.\n"

/* What to say when the questions could not be fetched. */
static const char* fetchFailure(int code) {
  switch (code) {
    case QUIZ_REFUSED:
      return apiRefused;
    case QUIZ_NOT_QUESTIONS:
      return apiNotQuestions;
    case QUIZ_NO_CURL:
      return noCurl;
    case QUIZ_TIMED_OUT:
      return apiTimedOut;
    default:
      return apiUnreachable;
  }
}

enum { ASKED_WRONG = 0, ASKED_RIGHT = 1, ASKED_STOP = -1, ASKED_TIMEOUT = -2 };

/* Output to a terminal that can clear the screen and move the cursor. One
   that says it is dumb, as Emacs's M-x shell does, gets what a pipe gets. */
static int atTerminal;

/* With -f, a line about the answer last given, which the next question shows
   above itself; empty until there is one. */
static int giveFeedback;
static char feedback[QUESTION_EXPRESSION_MAX + 2 * QUESTION_NUMBER_MAX + 32];

static void rememberFeedback(const Question* q, int result, int choice) {
  if (result == ASKED_RIGHT)
    snprintf(feedback, sizeof feedback, "✅ Right: %s = %s", q->expression,
             q->answer);
  else if (result == ASKED_TIMEOUT)
    snprintf(feedback, sizeof feedback, "❌ Wrong: %s = %s (time ran out)",
             q->expression, q->answer);
  else
    snprintf(feedback, sizeof feedback, "❌ Wrong: %s = %s (you said %s)",
             q->expression, q->answer, q->options[choice]);
}

/* An escape sequence, not a `clear` process for every question, and only for
   a person at a terminal: a pipe or a file gets a blank line instead. */
static void clearScreen(void) {
  if (atTerminal)
    fputs("\033[H\033[2J", stdout);
  else
    putchar('\n');
}

/* The line the answer is typed on. With a time limit the seconds left come
   last, and at a terminal the cursor position before them is saved, so that
   tick() rewrites only them: going back to the start of the line would land
   on its second half in a terminal narrow enough to wrap it. */
static void showPrompt(int limit) {
  if (limit > 0)
    printf("\tAnswer (A-D, Q to stop) - %s%d s left: ",
           atTerminal ? "\0337" : "", limit);
  else
    fputs("\tAnswer (A-D, Q to stop): ", stdout);
  fflush(stdout);
}

/* Counts the time limit down in place, at a terminal only. */
static void tick(int left) {
  if (!atTerminal) return;
  printf("\0338%d s left: \033[K", left);
  fflush(stdout);
}

/* Shows question `number` of `total`, or only its number when `total` is 0,
   and reads the answer, within `limit` seconds if that is not 0. Returns ASKED_RIGHT, ASKED_WRONG, ASKED_TIMEOUT, or ASKED_STOP when
   the person stopped or the input ended. The time from showing the question
   to the answer, or to the end of the limit, is added to `*spent`; waiting for
   the server is not the person's time. A `note` is put on a line of its own
   above the question, under what -f has to say about the last answer. Unless
   the person stopped, `*chosen` is set to the option picked or to
   CHOICE_TIMEOUT; `chosen` may be NULL. */
static int askQuestion(long number, long total, const Question* q, int limit,
                       double* spent, const char* note, int* chosen) {
  inputDiscard();
  clearScreen();
  if (*feedback) printf("%s\n\n", feedback);
  if (note) printf("%s\n\n", note);
  if (total)
    printf("%ld/%ld\t🤔 Solve this expression: %s\n\n", number, total,
           q->expression);
  else
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
  if (chosen) *chosen = choice;

  int result = ASKED_TIMEOUT;
  if (choice != CHOICE_TIMEOUT) {
    char letter[2] = {(char)('A' + choice), '\0'};
    result = checkAnswer(letter, q->answer, q->options[0], q->options[1],
                         q->options[2], q->options[3]);
  }
  if (giveFeedback) rememberFeedback(q, result, choice);
  return result;
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
      int result = askQuestion(pending[i].number, 0, &pending[i].question,
                               limit, &notCounted, note, NULL);
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
  Options options;
  const char* culprit;
  switch (parseArgs(argc, argv, &options, &culprit)) {
    case ARGS_OK:
      break;
    case ARGS_NONE:
      return fputs(noArgs, stderr), 1;
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
    case ARGS_BAD_LENGTH:
      return fprintf(stderr,
                     "😅 A question has %d to %d numbers, not \"%s\"! :3\n",
                     LENGTH_MIN, LENGTH_MAX, culprit),
             1;
    case ARGS_BAD_MAX:
      return fprintf(stderr,
                     "😅 The largest number is a whole number from %d to %d, "
                     "not \"%s\"! :3\n",
                     MAX_MIN, MAX_MAX, culprit),
             1;
    case ARGS_BAD_OPERATORS:
      return fprintf(stderr,
                     "😅 Operators are some of add, subtract, multiply and "
                     "divide, each once and separated by commas, not "
                     "\"%s\"! :3\n",
                     culprit),
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

  puts(welcomeText);
  const char* term = getenv("TERM");
  atTerminal = isatty(STDOUT_FILENO) && !(term && !strcmp(term, "dumb"));
  giveFeedback = options.feedback;
  Quiz quiz;
  quizOpen(&quiz, &options);
  inputStart();

  Mistakes mistakes = {NULL, 0, 0};
  long answered = 0;
  double thinking = 0;
  /* The question that took longest to answer, and how long. */
  double slowest = 0;
  long slowestNumber = 0;
  Question slowestQuestion;
  /* A server lost after the first question ends the run there, and what was
     answered until then still gets its result. Running out of memory for a
     mistake ends it the same way, before the question that could not be
     kept is counted. */
  int lost = 0, outOfMemory = 0;
  for (long i = 0; i < options.questions; i++) {
    Question question;
    /* Fetching the first batch can take a few seconds when the server is slow
       or not there, and nothing else is on the screen to say so. */
    int waiting = i == 0 && atTerminal;
    if (waiting) {
      fputs(fetching, stdout);
      fflush(stdout);
    }
    int fetched = quizNext(&quiz, &question);
    if (waiting) {
      fputs("\r\033[K", stdout);
      fflush(stdout);
    }
    if (fetched < 0) {
      if (i == 0) {
        fputs(fetchFailure(fetched), stderr);
        quizClose(&quiz);
        return 1;
      }
      lost = fetched;
      break;
    }
    int chosen;
    double before = thinking;
    int result = askQuestion(i + 1, options.questions, &question,
                             options.seconds, &thinking, NULL, &chosen);
    if (result == ASKED_STOP) break;
    if (result != ASKED_RIGHT) {
      Mistake wrong = {i + 1, question, chosen};
      if (!mistakesAdd(&mistakes, &wrong)) {
        thinking = before;
        outOfMemory = 1;
        break;
      }
    }
    answered++;
    if (thinking - before > slowest) {
      slowest = thinking - before;
      slowestNumber = i + 1;
      slowestQuestion = question;
    }
  }
  quizClose(&quiz);

  /* A run that was stopped is over; one that was finished goes on to the
     mistakes if that was asked for. */
  long stillWrong = 0, rounds = 0;
  int practised = options.practice && mistakes.count &&
                  answered == options.questions;
  int practiceLost = 0;
  if (practised) {
    stillWrong =
        practise(mistakes.items, mistakes.count, options.seconds, &rounds);
    if (stillWrong < 0) {
      practised = 0;
      practiceLost = 1;
    }
  }

  clearScreen();
  if (lost)
    printf("😔 SaMathAPI stopped answering after %ld of %ld questions.\n\n",
           answered, options.questions);
  else if (outOfMemory)
    printf("😔 I ran out of memory after %ld of %ld %s.\n\n", answered,
           options.questions,
           options.questions == 1 ? "question" : "questions");
  else if (answered < options.questions)
    printf("✋ You stopped after %ld of %ld %s.\n\n", answered,
           options.questions,
           options.questions == 1 ? "question" : "questions");
  if (mistakes.count) {
    printf("😔 You have %ld %s!\n\n", mistakes.count,
           mistakes.count == 1 ? "mistake" : "mistakes");
    for (long i = 0; i < mistakes.count; i++) {
      const Mistake* m = &mistakes.items[i];
      printf("Task %ld. %s\t= %s\t", m->number, m->question.expression,
             m->question.answer);
      if (m->chosen == CHOICE_TIMEOUT)
        puts("(time ran out)");
      else
        printf("(you said %s)\n", m->question.options[m->chosen]);
    }
  } else if (answered && !outOfMemory)
    puts(noErrors);
  if (answered) {
    char stats[128];
    formatStats(answered - mistakes.count, answered, thinking, stats,
                sizeof stats);
    printf("\n📊 %s\n", stats);
    char slowestLine[QUESTION_EXPRESSION_MAX + 64];
    if (formatSlowest(slowestNumber, slowestQuestion.expression, slowest,
                      answered, slowestLine, sizeof slowestLine))
      printf("🐢 Slowest: %s\n", slowestLine);
  }
  if (practised && !stillWrong)
    printf("\n🔁 Practice: every mistake put right, in %ld %s.\n", rounds,
           rounds == 1 ? "round" : "rounds");
  else if (practised)
    printf("\n🔁 Practice stopped with %ld of %ld still to put right.\n",
           stillWrong, mistakes.count);
  puts(bye);
  mistakesFree(&mistakes);
  /* Output that did not get out, as to a full disk, is not a finished run,
     whatever the score. */
  int written = fflush(stdout) == 0 && !ferror(stdout);
  if (!written) fputs(writeFailure, stderr);
  if (lost) fputs(fetchFailure(lost), stderr);
  if (outOfMemory) fputs(noMemory, stderr);
  if (practiceLost) fputs(noPractice, stderr);
  return lost != 0 || outOfMemory || practiceLost || !written;
}
