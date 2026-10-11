#ifndef SAMATH_OPTIONS_H
#define SAMATH_OPTIONS_H

#define TIME_LIMIT_MAX 3600
#define SEED_MAX 4294967295LL

typedef struct {
  long questions;  /* how many to ask: 1 or more */
  int seconds;     /* the time each question gets, or 0 for no limit */
  long long seed;  /* 0 to SEED_MAX, or -1 for questions that differ every run */
  int practice;    /* ask the mistakes again until each is right */
} Options;

typedef enum {
  ARGS_OK,
  ARGS_NONE,       /* nothing at all was given */
  ARGS_HELP,       /* -h or --help */
  ARGS_BAD_COUNT,  /* the number of questions is not a whole number of 1 or more */
  ARGS_BAD_TIME,   /* -t without a whole number of seconds, 1 to TIME_LIMIT_MAX */
  ARGS_BAD_SEED,   /* -s without a whole number from 0 to SEED_MAX */
  ARGS_BAD_OPTION, /* an option there is no such thing as */
  ARGS_EXTRA       /* more than one number of questions */
} ArgsResult;

/* Reads the command line. Anything but ARGS_OK, ARGS_NONE and ARGS_HELP
   leaves the offending argument in `*culprit`. Options may come before or
   after the number of questions; one that takes a value takes it as
   `-t 10`, `-t10`, `--time 10` or `--time=10`; `--` ends the options. */
ArgsResult parseArgs(int argc, char** argv, Options* out, const char** culprit);

#endif
