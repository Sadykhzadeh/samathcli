#ifndef SAMATH_OPTIONS_H
#define SAMATH_OPTIONS_H

#define TIME_LIMIT_MAX 3600
#define SEED_MAX 4294967295LL
/* How many numbers a question may have: the bounds SaMathAPI takes. */
#define LENGTH_MIN 2
#define LENGTH_MAX 12
/* How far from zero a number may be: the bounds SaMathAPI takes for `max`. */
#define MAX_MIN 10
#define MAX_MAX 10000
/* "add,subtract,multiply,divide", the longest list of operators there is. */
#define OPERATORS_SIZE 32
/* What `brackets` can be besides 0, which leaves it to the server. */
/* The bounds SaMathAPI takes for `answerMin` and `answerMax`, and how far
   apart they may be. */
#define RANGE_MAX 10000
#define RANGE_WIDTH 100
/* What `noNegatives` can be besides 0 and 1: negative numbers asked for, for a
   server that leaves them out unless told otherwise. */
#define NEGATIVES_ASK 2
#define BRACKETS_ASK 1
#define BRACKETS_NEVER 2

typedef struct {
  long questions;  /* how many to ask: 1 or more */
  int seconds;     /* the time each question gets, or 0 for no limit */
  long long seed;  /* 0 to SEED_MAX, or -1 for questions that differ every run */
  int practice;    /* ask the mistakes again until each is right */
  int length;      /* numbers in each question, or 0 to leave it to the server */
  int brackets;    /* BRACKETS_ASK for questions with brackets, BRACKETS_NEVER for none */
  int noNegatives; /* 1 for questions with no number below zero, NEGATIVES_ASK for ones that may have */
  int feedback;    /* say after each answer whether it was right */
  int max;         /* no number further from zero than this, or 0 to leave it to the server */
  char operators[OPERATORS_SIZE]; /* "add,subtract" and the like, or "" for all four */
  int ranged;      /* the answers come from rangeLow to rangeHigh */
  int rangeLow;
  int rangeHigh;
} Options;

typedef enum {
  ARGS_OK,
  ARGS_NONE,       /* nothing at all was given */
  ARGS_HELP,       /* -h or --help */
  ARGS_BAD_COUNT,  /* the number of questions is not a whole number of 1 or more */
  ARGS_BAD_TIME,   /* -t without a whole number of seconds, 1 to TIME_LIMIT_MAX */
  ARGS_BAD_SEED,   /* -s without a whole number from 0 to SEED_MAX */
  ARGS_BAD_LENGTH, /* -l without a whole number, LENGTH_MIN to LENGTH_MAX */
  ARGS_BAD_MAX,    /* -m without a whole number, MAX_MIN to MAX_MAX */
  ARGS_BAD_OPERATORS, /* -o without some of add, subtract, multiply and divide */
  ARGS_BAD_RANGE,  /* -r without LOW-HIGH, RANGE_MAX at most and RANGE_WIDTH apart */
  ARGS_BAD_OPTION, /* an option there is no such thing as */
  ARGS_EXTRA       /* more than one number of questions */
} ArgsResult;

/* Reads the command line. Anything but ARGS_OK, ARGS_NONE and ARGS_HELP
   leaves the offending argument in `*culprit`. Options may come before or
   after the number of questions; one that takes a value takes it as
   `-t 10`, `-t10`, `--time 10` or `--time=10`; `--` ends the options. */
ArgsResult parseArgs(int argc, char** argv, Options* out, const char** culprit);

/* The long option that a misspelt `arg` most likely meant, as "--practice" for
   "--practise" and "--time" for "--timeout=5", or NULL when none is near or
   `arg` is not a long option. It stays valid until the next call. */
const char* nearestOption(const char* arg);

#endif
