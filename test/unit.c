#include <stdio.h>
#include <string.h>
#include "../checkAnswer.h"
#include "../question.h"

static int checks = 0;
static int failures = 0;

#define CHECK(condition)                                                  \
  do {                                                                    \
    checks++;                                                             \
    if (!(condition)) {                                                   \
      failures++;                                                         \
      fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__,          \
              #condition);                                                \
    }                                                                     \
  } while (0)

#define CHECK_STR(actual, expected)                                       \
  do {                                                                    \
    checks++;                                                             \
    if (strcmp((actual), (expected))) {                                   \
      failures++;                                                         \
      fprintf(stderr, "%s:%d: got \"%s\", wanted \"%s\"\n", __FILE__,     \
              __LINE__, (actual), (expected));                            \
    }                                                                     \
  } while (0)

static void parsesAWellFormedLine(void) {
  Question q;
  CHECK(parseQuestion("3*4-1 11,9,12,10 11", &q));
  CHECK_STR(q.expression, "3*4-1");
  CHECK_STR(q.options[0], "11");
  CHECK_STR(q.options[1], "9");
  CHECK_STR(q.options[2], "12");
  CHECK_STR(q.options[3], "10");
  CHECK_STR(q.answer, "11");
}

static void keepsNegativesAndBrackets(void) {
  Question q;
  CHECK(parseQuestion("-3+(4-9)*2 -13,-12,13,-10 -13", &q));
  CHECK_STR(q.expression, "-3+(4-9)*2");
  CHECK_STR(q.options[0], "-13");
  CHECK_STR(q.answer, "-13");
}

static void toleratesTheEndOfLineTheServerSent(void) {
  Question q;
  CHECK(parseQuestion("1+1 2,3,4,5 2\n", &q));
  CHECK(parseQuestion("1+1 2,3,4,5 2\r\n", &q));
  CHECK(parseQuestion("1+1 2,3,4,5 2  \n", &q));
  CHECK_STR(q.answer, "2");
}

static void refusesALineThatIsNotAQuestion(void) {
  Question q;
  CHECK(!parseQuestion("", &q));
  CHECK(!parseQuestion("\n", &q));
  CHECK(!parseQuestion("Hello World", &q));
  CHECK(!parseQuestion("1+1", &q));
  CHECK(!parseQuestion("1+1 2,3,4,5", &q));
  CHECK(!parseQuestion("1+1 2,3,4 2", &q));
  CHECK(!parseQuestion("1+1 2,3,4,5,6 2", &q));
  CHECK(!parseQuestion("1+1 2,3,4,5 2 extra", &q));
  CHECK(!parseQuestion(" 1+1 2,3,4,5 2", &q));
  CHECK(!parseQuestion("1+1 2,,4,5 2", &q));
  CHECK(!parseQuestion("{\"error\":\"boom\"}", &q));
}

static void refusesAnAnswerThatIsNotAmongTheOptions(void) {
  Question q;
  /* The old parser took this for option D and marked a right answer wrong. */
  CHECK(!parseQuestion("1+1 3,4,5,6 2", &q));
  CHECK(!parseQuestion("1+1 2,3,4,5 20", &q));
}

static void refusesNumbersThatAreNotWholeNumbers(void) {
  Question q;
  CHECK(!parseQuestion("1+1 2,x,4,5 2", &q));
  CHECK(!parseQuestion("1+1 2,3.5,4,5 2", &q));
  CHECK(!parseQuestion("1+1 2,3,4,5 two", &q));
  CHECK(!parseQuestion("1+1 2,-,4,5 2", &q));
  CHECK(!parseQuestion("1+1 2,3,4,+5 2", &q));
}

static void keepsEverythingButTheExpressionOffTheTerminal(void) {
  Question q;
  CHECK(!parseQuestion("\033[2J 2,3,4,5 2", &q));
  CHECK(!parseQuestion("1+x 2,3,4,5 2", &q));
  CHECK(!parseQuestion("2^3 8,6,9,5 8", &q));
  CHECK(!parseQuestion("1+1 2,3,4,5 \033[2J", &q));
}

static void refusesWhatDoesNotFit(void) {
  Question q;
  char line[600];
  char digits[40];
  memset(digits, '9', sizeof digits - 1);
  digits[sizeof digits - 1] = '\0';

  /* 255 characters is the longest expression there is room for. */
  memset(line, '1', 255);
  strcpy(line + 255, " 2,3,4,5 2");
  CHECK(parseQuestion(line, &q));
  CHECK(strlen(q.expression) == 255);
  memset(line, '1', 256);
  strcpy(line + 256, " 2,3,4,5 2");
  CHECK(!parseQuestion(line, &q));

  snprintf(line, sizeof line, "1+1 2,3,4,5 %s", digits);
  CHECK(!parseQuestion(line, &q));
  snprintf(line, sizeof line, "1+1 %s,3,4,5 2", digits);
  CHECK(!parseQuestion(line, &q));
}

static void leavesTheResultAloneWhenItRefuses(void) {
  Question q;
  CHECK(parseQuestion("1+1 2,3,4,5 2", &q));
  CHECK(!parseQuestion("garbage", &q));
  CHECK_STR(q.expression, "1+1");
}

static void picksTheOptionThatHoldsTheAnswer(void) {
  char* o[4] = {"11", "9", "12", "10"};
  CHECK(checkAnswer("A", "11", o[0], o[1], o[2], o[3]));
  CHECK(checkAnswer("B", "9", o[0], o[1], o[2], o[3]));
  CHECK(checkAnswer("C", "12", o[0], o[1], o[2], o[3]));
  CHECK(checkAnswer("D", "10", o[0], o[1], o[2], o[3]));
  CHECK(!checkAnswer("B", "11", o[0], o[1], o[2], o[3]));
  CHECK(!checkAnswer("A", "10", o[0], o[1], o[2], o[3]));
}

static void doesNotCareAboutCase(void) {
  char* o[4] = {"1", "2", "3", "4"};
  CHECK(checkAnswer("a", "1", o[0], o[1], o[2], o[3]));
  CHECK(checkAnswer("d", "4", o[0], o[1], o[2], o[3]));
}

static void refusesAnythingButOneLetter(void) {
  char* o[4] = {"1", "2", "3", "4"};
  CHECK(!checkAnswer("", "1", o[0], o[1], o[2], o[3]));
  CHECK(!checkAnswer("AB", "1", o[0], o[1], o[2], o[3]));
  CHECK(!checkAnswer("A1", "1", o[0], o[1], o[2], o[3]));
  CHECK(!checkAnswer("1", "1", o[0], o[1], o[2], o[3]));
  CHECK(!checkAnswer(NULL, "1", o[0], o[1], o[2], o[3]));
  CHECK(!checkAnswer("A", NULL, o[0], o[1], o[2], o[3]));
}

int main(void) {
  parsesAWellFormedLine();
  keepsNegativesAndBrackets();
  toleratesTheEndOfLineTheServerSent();
  refusesALineThatIsNotAQuestion();
  refusesAnAnswerThatIsNotAmongTheOptions();
  refusesNumbersThatAreNotWholeNumbers();
  keepsEverythingButTheExpressionOffTheTerminal();
  refusesWhatDoesNotFit();
  leavesTheResultAloneWhenItRefuses();
  picksTheOptionThatHoldsTheAnswer();
  doesNotCareAboutCase();
  refusesAnythingButOneLetter();

  if (failures) {
    fprintf(stderr, "%d of %d checks failed\n", failures, checks);
    return 1;
  }
  printf("%d checks passed\n", checks);
  return 0;
}
