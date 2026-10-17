#ifndef SAMATH_GET_QUESTION_H
#define SAMATH_GET_QUESTION_H

#include <stddef.h>
#include "options.h"
#include "question.h"

/* The most questions asked for in one request: SaMathAPI's limit on `count`. */
#define QUIZ_BATCH_MAX 100

/* The questions of one run, fetched a batch at a time. */
typedef struct {
  long total;       /* how many questions the run has */
  long long seed;   /* what to ask the server to seed with, or -1 */
  int length;       /* how many numbers to ask for, or 0 */
  int brackets;     /* ask for brackets */
  int noNegatives;  /* ask for no number below zero */
  long asked;       /* how many quizNext has handed out */
  const char* url;
  int batched;      /* the server has answered `count` with that many */
  int size;         /* questions fetched into `batch` */
  int next;         /* the first of them not handed out yet */
  Question batch[QUIZ_BATCH_MAX];
  /* Without a seed, a hash of each expression fetched so far, so that a
     later batch does not ask one again: open addressing, 0 for a free slot. */
  unsigned long long* seen;
  size_t seenRoom;  /* slots in `seen`, a power of two, or 0 */
  size_t seenCount;
} Quiz;

/* Starts a run of `options->questions` questions from SAMATH_API_URL, or
   from http://localhost:3000/gen?c=true when that is not set. A seed of 0 or
   more is passed on to the server, which then sends the same questions each
   time; -1 leaves it out. A length, brackets and no negative numbers are
   passed on when asked for, and left to the server otherwise. Nothing is
   fetched until the first question is asked for. */
void quizOpen(Quiz* quiz, const Options* options);

/* Lets go of what quizOpen and quizNext took; no question can be asked for
   after it. */
void quizClose(Quiz* quiz);

/* What quizNext gives back when there is no next question to hand out. */
#define QUIZ_UNREACHABLE (-1)   /* the server could not be reached */
#define QUIZ_REFUSED (-2)       /* it answered with an HTTP error */
#define QUIZ_NOT_QUESTIONS (-3) /* it answered, but not with a question */
#define QUIZ_NO_CURL (-4)       /* there is no curl to ask with */

/* Hands out the next question. Returns 1 and fills `out`, 0 once all `total`
   have been handed out, and one of the QUIZ_ codes above otherwise.

   The first call asks for the whole run in one request, `count` questions at a
   time. A server that does not know `count` sends one question back, and from
   then on every question is its own request, as it always was.

   Without a seed a question that an earlier batch held is left out, so a run
   longer than one batch asks each question once, as one batch does, until
   the server has no new one to give; one asked a question at a time can
   still repeat one. With a seed, question n is always the one for
   seed + n - 1. */
int quizNext(Quiz* quiz, Question* out);

/* Writes `url` with `query` ("count=20") added to its query string into
   `out`; an empty `query` leaves the URL as it is. Returns 0 when the result
   does not fit in `capacity`. */
int urlWithQuery(const char* url, const char* query, char* out,
                 size_t capacity);

#endif
