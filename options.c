#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "options.h"

/* The letters of the options that stand on their own, and of those that take
   a value. -h is read before anything else, so it is no more than a letter
   to skip here. */
#define SWITCHES "pbBnNfh"
#define VALUED "tslmor"

/* Whether `text` is some of the operators SaMathAPI names, each once and
   separated by commas, as in "add,subtract". */
static int operatorList(const char* text) {
  static const char* const names[] = {"add", "subtract", "multiply", "divide"};
  unsigned seen = 0;
  for (;;) {
    size_t length = strcspn(text, ",");
    int found = -1;
    for (int i = 0; i < 4; i++)
      if (strlen(names[i]) == length && !strncmp(text, names[i], length))
        found = i;
    if (found < 0 || seen >> found & 1) return 0;
    seen |= 1u << found;
    if (!text[length]) return 1;
    text += length + 1;
  }
}

/* The whole of `text` as a whole number from `low` to `high`, or -1. */
static long long wholeNumber(const char* text, long long low, long long high) {
  char* end;
  errno = 0;
  long long value = strtoll(text, &end, 10);
  if (errno || end == text || *end || value < low || value > high) return -1;
  return value;
}

/* Whether `text` is LOW-HIGH, whole numbers up to RANGE_MAX with at most
   RANGE_WIDTH between them; the two are left in `*low` and `*high`. */
static int answerRange(const char* text, int* low, int* high) {
  char* dash;
  if (*text < '0' || *text > '9') return 0;
  long long first = strtoll(text, &dash, 10);
  if (*dash != '-' || dash[1] < '0' || dash[1] > '9') return 0;
  long long second = wholeNumber(dash + 1, 0, RANGE_MAX);
  if (first > RANGE_MAX || second < first || second - first > RANGE_WIDTH)
    return 0;
  *low = (int)first;
  *high = (int)second;
  return 1;
}

static const struct {
  const char* name;
  char letter;
} longOptions[] = {{"practice", 'p'}, {"brackets", 'b'}, {"no-brackets", 'B'},
                   {"no-negatives", 'n'}, {"negatives", 'N'},
                   {"feedback", 'f'},
                   {"time", 't'},     {"seed", 's'},
                   {"length", 'l'},   {"max", 'm'},      {"operators", 'o'},
                   {"range", 'r'}};

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

/* No long option has a name this long, so a word longer than this is not
   compared with them. */
#define NAME_LONGEST 24

/* The number of single-letter changes it takes to turn `a` into `b`, both at
   most NAME_LONGEST long. */
static size_t editDistance(const char* a, const char* b) {
  size_t rows[2][NAME_LONGEST + 1];
  size_t lengthA = strlen(a), lengthB = strlen(b);
  for (size_t j = 0; j <= lengthB; j++) rows[0][j] = j;
  for (size_t i = 1; i <= lengthA; i++) {
    const size_t* previous = rows[(i - 1) % 2];
    size_t* row = rows[i % 2];
    row[0] = i;
    for (size_t j = 1; j <= lengthB; j++) {
      size_t best = previous[j] + 1;
      if (row[j - 1] + 1 < best) best = row[j - 1] + 1;
      size_t replace = previous[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1);
      row[j] = replace < best ? replace : best;
    }
  }
  return rows[lengthA % 2][lengthB];
}

const char* nearestOption(const char* arg) {
  if (strncmp(arg, "--", 2)) return NULL;
  const char* name = arg + 2;
  size_t length = strcspn(name, "=");
  if (length < 3 || length > NAME_LONGEST) return NULL;
  char lower[NAME_LONGEST + 1];
  for (size_t i = 0; i < length; i++)
    lower[i] = (char)tolower((unsigned char)name[i]);
  lower[length] = '\0';

  static char suggestion[NAME_LONGEST + 3];
  size_t limit = length < 5 ? 1 : 2;
  size_t nearest = limit + 1;
  /* --help and --version are read before the rest, so they are not in the
     table. */
  static const char* const alone[] = {"help", "version"};
  size_t tabled = sizeof longOptions / sizeof *longOptions;
  for (size_t i = 0; i < tabled + 2; i++) {
    const char* known = i < tabled ? longOptions[i].name : alone[i - tabled];
    size_t shorter = strlen(known) < length ? strlen(known) : length;
    /* One that runs on past a name, as --timeout does past --time, or stops
       short of one, as --no-negative does, is as good as a letter off. */
    size_t apart = shorter >= 3 && !strncmp(lower, known, shorter)
                       ? 1
                       : editDistance(lower, known);
    if (apart < nearest) {
      nearest = apart;
      snprintf(suggestion, sizeof suggestion, "--%s", known);
    }
  }
  return nearest <= limit ? suggestion : NULL;
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
      options->brackets = BRACKETS_ASK;
      break;
    case 'B':
      options->brackets = BRACKETS_NEVER;
      break;
    case 'n':
      options->noNegatives = 1;
      break;
    case 'N':
      options->noNegatives = NEGATIVES_ASK;
      break;
    case 'f':
      options->feedback = 1;
      break;
  }
}

/* Sets what -t, -s, -l, -m or -o (`letter`) asks for from `value`, which is NULL when
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
    case 'm':
      number = value ? wholeNumber(value, MAX_MIN, MAX_MAX) : -1;
      failure = ARGS_BAD_MAX;
      if (number >= 0) options->max = (int)number;
      break;
    case 'o':
      /* Whatever is longer than the longest list is not one. */
      number = value && strlen(value) < OPERATORS_SIZE && operatorList(value)
                   ? 0
                   : -1;
      failure = ARGS_BAD_OPERATORS;
      if (number >= 0) strcpy(options->operators, value);
      break;
    case 'r':
      number = value && answerRange(value, &options->rangeLow, &options->rangeHigh)
                   ? 0
                   : -1;
      failure = ARGS_BAD_RANGE;
      if (number >= 0) options->ranged = 1;
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
  Options options = {0, 0, -1, 0, 0, 0, 0, 0, 0, "", 0, 0, 0};
  int optionsEnded = 0;
  /* The option that is not one, when it is one letter among several. */
  static char unknown[3] = {'-', '\0', '\0'};
  *culprit = NULL;

  /* Asking for help wins over anything else that is wrong with the line. */
  for (int i = 1; i < argc && strcmp(argv[i], "--"); i++)
    if (asksForHelp(argv[i])) return ARGS_HELP;
  for (int i = 1; i < argc && strcmp(argv[i], "--"); i++)
    if (!strcmp(argv[i], "--version")) return ARGS_VERSION;

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
  /* No answer is above the largest number, so a range that starts there has
     no question. The API would refuse it, after a round trip. */
  if (options.ranged && options.max && options.rangeLow > options.max) {
    *out = options;
    return ARGS_RANGE_ABOVE_MAX;
  }
  *out = options;
  return ARGS_OK;
}
