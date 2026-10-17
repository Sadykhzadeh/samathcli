#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "options.h"

/* The letters of the options that stand on their own, and of those that take
   a value. -h is read before anything else, so it is no more than a letter
   to skip here. */
#define SWITCHES "pbnfh"
#define VALUED "tsl"

/* The whole of `text` as a whole number from `low` to `high`, or -1. */
static long long wholeNumber(const char* text, long long low, long long high) {
  char* end;
  errno = 0;
  long long value = strtoll(text, &end, 10);
  if (errno || end == text || *end || value < low || value > high) return -1;
  return value;
}

static const struct {
  const char* name;
  char letter;
} longOptions[] = {{"practice", 'p'}, {"brackets", 'b'}, {"no-negatives", 'n'},
                   {"feedback", 'f'}, {"time", 't'},     {"seed", 's'},
                   {"length", 'l'}};

/* The letter of the long option `--name` or `--name=value` in `arg`, or 0 when
   it is none. A value that came attached is left in `*attached`, otherwise
   NULL. */
static char longOption(const char* arg, const char** attached) {
  const char* name = arg + 2;
  const char* equals = strchr(name, '=');
  size_t length = equals ? (size_t)(equals - name) : strlen(name);
  *attached = equals ? equals + 1 : NULL;
  for (size_t i = 0; i < sizeof longOptions / sizeof *longOptions; i++) {
    char letter = longOptions[i].letter;
    if (strlen(longOptions[i].name) != length ||
        strncmp(name, longOptions[i].name, length))
      continue;
    /* A value belongs only to the options that take one. */
    return equals && !strchr(VALUED, letter) ? 0 : letter;
  }
  return 0;
}

/* Whether `arg` is --help, or short options with an h among the letters that
   come before one that takes a value, as in -h and -pfh. */
static int asksForHelp(const char* arg) {
  if (!strcmp(arg, "--help")) return 1;
  if (arg[0] != '-' || arg[1] == '-' || !arg[1]) return 0;
  for (const char* c = arg + 1; *c && !strchr(VALUED, *c); c++)
    if (*c == 'h') return 1;
  return 0;
}

static void setSwitch(char letter, Options* options) {
  switch (letter) {
    case 'p':
      options->practice = 1;
      break;
    case 'b':
      options->brackets = 1;
      break;
    case 'n':
      options->noNegatives = 1;
      break;
    case 'f':
      options->feedback = 1;
      break;
  }
}

/* Sets what -t, -s or -l (`letter`) asks for from `value`, which is NULL when
   the option was the last word. `word` is the argument that held the option. */
static ArgsResult setValue(char letter, const char* value, const char* word,
                           Options* options, const char** culprit) {
  long long number;
  ArgsResult failure;
  switch (letter) {
    case 't':
      number = value ? wholeNumber(value, 1, TIME_LIMIT_MAX) : -1;
      failure = ARGS_BAD_TIME;
      if (number >= 0) options->seconds = (int)number;
      break;
    case 's':
      number = value ? wholeNumber(value, 0, SEED_MAX) : -1;
      failure = ARGS_BAD_SEED;
      if (number >= 0) options->seed = number;
      break;
    default:
      number = value ? wholeNumber(value, LENGTH_MIN, LENGTH_MAX) : -1;
      failure = ARGS_BAD_LENGTH;
      if (number >= 0) options->length = (int)number;
      break;
  }
  if (number >= 0) return ARGS_OK;
  *culprit = value ? value : word;
  return failure;
}

ArgsResult parseArgs(int argc, char** argv, Options* out, const char** culprit) {
  Options options = {0, 0, -1, 0, 0, 0, 0, 0};
  int optionsEnded = 0;
  /* The option that is not one, when it is one letter among several. */
  static char unknown[3] = {'-', '\0', '\0'};
  *culprit = NULL;

  /* Asking for help wins over anything else that is wrong with the line. */
  for (int i = 1; i < argc && strcmp(argv[i], "--"); i++)
    if (asksForHelp(argv[i])) return ARGS_HELP;

  for (int i = 1; i < argc; i++) {
    const char* arg = argv[i];
    int negativeNumber = arg[0] == '-' && arg[1] >= '0' && arg[1] <= '9';

    if (optionsEnded || arg[0] != '-' || !arg[1] || negativeNumber) {
      if (options.questions) {
        *culprit = arg;
        return ARGS_EXTRA;
      }
      long long questions = wholeNumber(arg, 1, INT_MAX);
      if (questions < 0) {
        *culprit = arg;
        return ARGS_BAD_COUNT;
      }
      options.questions = (long)questions;
    } else if (!strcmp(arg, "--")) {
      optionsEnded = 1;
    } else if (arg[1] == '-') {
      const char* attached;
      char letter = longOption(arg, &attached);
      if (!letter) {
        *culprit = arg;
        return ARGS_BAD_OPTION;
      }
      if (strchr(VALUED, letter)) {
        const char* value =
            attached ? attached : i + 1 < argc ? argv[++i] : NULL;
        ArgsResult result = setValue(letter, value, arg, &options, culprit);
        if (result != ARGS_OK) return result;
      } else {
        setSwitch(letter, &options);
      }
    } else {
      /* One or several short options in a word: -p, -pf, -t10, -pt 10. The
         first that takes a value takes the rest of the word as it, or else
         the next word. */
      for (const char* c = arg + 1; *c; c++) {
        if (strchr(SWITCHES, *c)) {
          setSwitch(*c, &options);
          continue;
        }
        if (!strchr(VALUED, *c)) {
          unknown[1] = *c;
          *culprit = (unsigned char)*c < 0x80 ? unknown : arg;
          return ARGS_BAD_OPTION;
        }
        const char* value = c[1] ? c + 1 : i + 1 < argc ? argv[++i] : NULL;
        ArgsResult result = setValue(*c, value, arg, &options, culprit);
        if (result != ARGS_OK) return result;
        break;
      }
    }
  }

  if (!options.questions) return ARGS_NONE;
  *out = options;
  return ARGS_OK;
}
