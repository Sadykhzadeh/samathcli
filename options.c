#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "options.h"

/* The whole of `text` as a whole number from `low` to `high`, or -1. */
static long long wholeNumber(const char* text, long long low, long long high) {
  char* end;
  errno = 0;
  long long value = strtoll(text, &end, 10);
  if (errno || end == text || *end || value < low || value > high) return -1;
  return value;
}

/* Whether `arg` is this option: `-t`, `-t10`, `--time` or `--time=10`. A value
   that came attached to the word is left in `*attached`, otherwise NULL. */
static int isOption(const char* arg, char shortName, const char* longName,
                    const char** attached) {
  *attached = NULL;
  if (arg[0] != '-') return 0;
  if (arg[1] == shortName) {
    if (arg[2]) *attached = arg + 2;
    return 1;
  }
  if (arg[1] == '-' && !strncmp(arg + 2, longName, strlen(longName))) {
    const char* after = arg + 2 + strlen(longName);
    if (!*after) return 1;
    if (*after == '=') {
      *attached = after + 1;
      return 1;
    }
  }
  return 0;
}

ArgsResult parseArgs(int argc, char** argv, Options* out, const char** culprit) {
  Options options = {0, 0, -1, 0, 0, 0, 0, 0};
  int optionsEnded = 0;
  *culprit = NULL;

  /* Asking for help wins over anything else that is wrong with the line. */
  for (int i = 1; i < argc && strcmp(argv[i], "--"); i++)
    if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) return ARGS_HELP;

  for (int i = 1; i < argc; i++) {
    const char* arg = argv[i];
    const char* attached;
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
    } else if (!strcmp(arg, "-p") || !strcmp(arg, "--practice")) {
      options.practice = 1;
    } else if (!strcmp(arg, "-b") || !strcmp(arg, "--brackets")) {
      options.brackets = 1;
    } else if (!strcmp(arg, "-n") || !strcmp(arg, "--no-negatives")) {
      options.noNegatives = 1;
    } else if (!strcmp(arg, "-f") || !strcmp(arg, "--feedback")) {
      options.feedback = 1;
    } else if (isOption(arg, 't', "time", &attached)) {
      const char* value = attached ? attached : i + 1 < argc ? argv[++i] : NULL;
      long long seconds = value ? wholeNumber(value, 1, TIME_LIMIT_MAX) : -1;
      if (seconds < 0) {
        *culprit = value ? value : arg;
        return ARGS_BAD_TIME;
      }
      options.seconds = (int)seconds;
    } else if (isOption(arg, 's', "seed", &attached)) {
      const char* value = attached ? attached : i + 1 < argc ? argv[++i] : NULL;
      options.seed = value ? wholeNumber(value, 0, SEED_MAX) : -1;
      if (options.seed < 0) {
        *culprit = value ? value : arg;
        return ARGS_BAD_SEED;
      }
    } else if (isOption(arg, 'l', "length", &attached)) {
      const char* value = attached ? attached : i + 1 < argc ? argv[++i] : NULL;
      long long length = value ? wholeNumber(value, LENGTH_MIN, LENGTH_MAX) : -1;
      if (length < 0) {
        *culprit = value ? value : arg;
        return ARGS_BAD_LENGTH;
      }
      options.length = (int)length;
    } else {
      *culprit = arg;
      return ARGS_BAD_OPTION;
    }
  }

  if (!options.questions) return ARGS_NONE;
  *out = options;
  return ARGS_OK;
}
