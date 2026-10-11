#ifndef SAMATH_GET_QUESTION_H
#define SAMATH_GET_QUESTION_H

#include <stddef.h>
#include "question.h"

/* The most questions asked for in one request: SaMathAPI's limit on `count`. */
#define QUIZ_BATCH_MAX 100

/* The questions of one run, fetched a batch at a time. */
typedef struct {
  long total;  /* how many questions the run has */
  long asked;  /* how many quizNext has handed out */
  const char* url;
  int batched; /* the server has answered `count` with that many questions */
  int size;    /* questions fetched into `batch` */
  int next;    /* the first of them not handed out yet */
  Question batch[QUIZ_BATCH_MAX];
} Quiz;

/* Starts a run of `total` questions from SAMATH_API_URL, or from
   http://localhost:3000/gen?c=true when that is not set. Nothing is fetched
   until the first question is asked for. */
void quizOpen(Quiz* quiz, long total);

/* Hands out the next question. Returns 1 and fills `out`, 0 once all `total`
   have been handed out, and -1 when the server could not be reached or did
   not send a question.

   The first call asks for the whole run in one request, `count` questions at a
   time. A server that does not know `count` sends one question back, and from
   then on every question is its own request, as it always was. */
int quizNext(Quiz* quiz, Question* out);

/* Writes `url` with `query` ("count=20") added to its query string into
   `out`; an empty `query` leaves the URL as it is. Returns 0 when the result
   does not fit in `capacity`. */
int urlWithQuery(const char* url, const char* query, char* out,
                 size_t capacity);

#endif
