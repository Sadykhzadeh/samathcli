#ifndef SAMATH_MISTAKES_H
#define SAMATH_MISTAKES_H

#include "question.h"

/* A question answered wrongly, or not in time: its `number` in the run, the
   question itself, and the option picked, 0 to 3, or CHOICE_TIMEOUT. */
typedef struct {
  long number;
  Question question;
  int chosen;
} Mistake;

/* The mistakes of a run, in the order they were made. `items` is NULL until
   the first one. */
typedef struct {
  Mistake* items;
  long count;
  long room; /* how many `items` has room for */
} Mistakes;

/* Adds `m` to the end of `list`, growing it as needed. Returns 0 when there
   is no memory to grow it with, and then the list is as it was. */
int mistakesAdd(Mistakes* list, const Mistake* m);

/* Lets go of the memory of `list`. */
void mistakesFree(Mistakes* list);

#endif
