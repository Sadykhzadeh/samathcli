#ifndef SAMATH_CHECK_ANSWER_H
#define SAMATH_CHECK_ANSWER_H

/* Returns 1 when userAnswer names an option that reads as ans, 0 otherwise.
   The letter ignores case, so both "a" and "A" pick the first option. */
int checkAnswer(const char* userAnswer, const char* ans, const char* first,
                const char* second, const char* third, const char* fourth);

#endif
