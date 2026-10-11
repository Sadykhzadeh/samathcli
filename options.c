#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "options.h"

/* The whole of `text` as a whole number from `low` to `high`, or -1. */
static long wholeNumber(const char* text, long low, long high) {
  char* end;
  errno = 0;
  long value = strtol(text, &end, 10);
  if (errno || end == text || *end || value < low || value > high) return -1;
  return value;
}

ArgsResult parseArgs(int argc, char** argv, Options* out, const char** culprit) {
  Options options = {0, 0};
  int optionsEnded = 0;
  *culprit = NULL;

  /* Asking for help wins over anything else that is wrong with the line. */
  for (int i = 1; i < argc && strcmp(argv[i], "--"); i++)
    if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) return ARGS_HELP;

  for (int i = 1; i < argc; i++) {
    const char* arg = argv[i];
    int negativeNumber = arg[0] == '-' && arg[1] >= '0' && arg[1] <= '9';

    if (optionsEnded || arg[0] != '-' || !arg[1] || negativeNumber) {
      if (options.questions) {
        *culprit = arg;
        return ARGS_EXTRA;
      }
      options.questions = wholeNumber(arg, 1, INT_MAX);
      if (options.questions < 0) {
        options.questions = 0;
        *culprit = arg;
        return ARGS_BAD_COUNT;
      }
    } else if (!strcmp(arg, "--")) {
      optionsEnded = 1;
    } else if (!strncmp(arg, "-t", 2) || !strncmp(arg, "--time", 6)) {
      const char* value;
      if (arg[1] == 't')
        value = arg[2] ? arg + 2 : (i + 1 < argc ? argv[++i] : NULL);
      else if (arg[6] == '=')
        value = arg + 7;
      else if (arg[6] == '\0')
        value = i + 1 < argc ? argv[++i] : NULL;
      else {
        *culprit = arg;
        return ARGS_BAD_OPTION;
      }
      long seconds = value ? wholeNumber(value, 1, TIME_LIMIT_MAX) : -1;
      if (seconds < 0) {
        *culprit = value ? value : arg;
        return ARGS_BAD_TIME;
      }
      options.seconds = (int)seconds;
    } else {
      *culprit = arg;
      return ARGS_BAD_OPTION;
    }
  }

  if (!options.questions) return ARGS_NONE;
  *out = options;
  return ARGS_OK;
}
