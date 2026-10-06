#ifndef SAMATH_CHECK_ANSWER_H
#define SAMATH_CHECK_ANSWER_H

/* Returns 1 when userAnswer names the option holding ans, 0 otherwise.
   The comparison ignores case, so both "a" and "A" pick the first option. */
int checkAnswer(char* userAnswer, char* ans, char* first, char* second,
                char* third, char* fourth);

#endif
