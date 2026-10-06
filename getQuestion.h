#ifndef SAMATH_GET_QUESTION_H
#define SAMATH_GET_QUESTION_H

/* Fetches question `number`, shows it and reads the answer.
   Returns 1 when answered correctly, 0 when not, and -1 when the question
   could not be fetched, parsed or answered. */
int displayQuestion(int number);

#endif
