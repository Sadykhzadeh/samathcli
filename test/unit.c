#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../checkAnswer.h"
#include "../getQuestion.h"
#include "../input.h"
#include "../mistakes.h"
#include "../options.h"
#include "../question.h"
#include "../report.h"

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
  CHECK(!parseQuestion(" 2,3,4,5 2", &q));
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

static void skipsALineAsLongAsTheBufferThatReadsIt(void) {
  Question q[2];
  char line[QUESTION_LINE_MAX + 2];
  char text[2 * QUESTION_LINE_MAX + 32];
  memset(line, ' ', QUESTION_LINE_MAX + 1);
  memcpy(line, "1+1 2,3,4,5 2", 13);
  line[QUESTION_LINE_MAX - 1] = '\0';
  snprintf(text, sizeof text, "%s\n2+2 4,5,6,7 4\n", line);
  CHECK(parseQuestions(text, q, 2) == 2);
  line[QUESTION_LINE_MAX - 1] = ' ';
  line[QUESTION_LINE_MAX] = '\0';
  snprintf(text, sizeof text, "%s\n2+2 4,5,6,7 4\n", line);
  CHECK(parseQuestions(text, q, 2) == 1);
  CHECK_STR(q[0].expression, "2+2");
}

static void readsEveryQuestionInAReply(void) {
  Question q[8];
  CHECK(parseQuestions("1+1 2,3,4,5 2\n2+2 4,5,6,7 4\n3+3 9,8,7,6 6\n", q, 8) == 3);
  CHECK_STR(q[0].expression, "1+1");
  CHECK_STR(q[1].expression, "2+2");
  CHECK_STR(q[2].expression, "3+3");
  CHECK_STR(q[2].answer, "6");
  CHECK(parseQuestions("1+1 2,3,4,5 2\r\n2+2 4,5,6,7 4\r\n", q, 8) == 2);
  CHECK(parseQuestions("1+1 2,3,4,5 2\n2+2 4,5,6,7 4", q, 8) == 2);
  CHECK_STR(q[1].answer, "4");
}

static void skipsWhatIsNotAQuestionInAReply(void) {
  Question q[8];
  char hundreds[700];
  CHECK(parseQuestions("\n\n1+1 2,3,4,5 2\n\nnonsense\n2+2 4,5,6,7 4\n", q, 8) == 2);
  CHECK_STR(q[0].expression, "1+1");
  CHECK_STR(q[1].expression, "2+2");
  CHECK(parseQuestions("", q, 8) == 0);
  CHECK(parseQuestions("<html>\n<body>502</body>\n</html>\n", q, 8) == 0);
  memset(hundreds, '1', sizeof hundreds - 1);
  hundreds[sizeof hundreds - 1] = '\0';
  CHECK(parseQuestions(hundreds, q, 8) == 0);
}

static void stopsAtTheRoomItWasGiven(void) {
  Question q[2];
  CHECK(parseQuestions("1+1 2,3,4,5 2\n2+2 4,5,6,7 4\n3+3 9,8,7,6 6\n", q, 2) == 2);
  CHECK_STR(q[1].expression, "2+2");
  CHECK(parseQuestions("1+1 2,3,4,5 2\n", q, 0) == 0);
}

static void addsCountToAnyURL(void) {
  char out[64];
  CHECK(urlWithQuery("http://h/gen?c=true", "count=5", out, sizeof out));
  CHECK_STR(out, "http://h/gen?c=true&count=5");
  CHECK(urlWithQuery("http://h/gen", "count=5", out, sizeof out));
  CHECK_STR(out, "http://h/gen?count=5");
  CHECK(urlWithQuery("http://h/gen?", "count=5", out, sizeof out));
  CHECK_STR(out, "http://h/gen?count=5");
  CHECK(urlWithQuery("http://h/gen?c=true&", "count=5", out, sizeof out));
  CHECK_STR(out, "http://h/gen?c=true&count=5");
}

static void putsTheQueryBeforeAFragment(void) {
  char out[64];
  CHECK(urlWithQuery("http://h/gen#x", "count=5", out, sizeof out));
  CHECK_STR(out, "http://h/gen?count=5");
  CHECK(urlWithQuery("http://h/gen?c=true#x", "count=5", out, sizeof out));
  CHECK_STR(out, "http://h/gen?c=true&count=5");
  CHECK(urlWithQuery("http://h/gen#x", "", out, sizeof out));
  CHECK_STR(out, "http://h/gen");
  CHECK(urlWithQuery("http://h/gen#x?y", "count=5", out, sizeof out));
  CHECK_STR(out, "http://h/gen?count=5");
}

static void leavesTheURLAloneWithoutAQuery(void) {
  char out[64];
  CHECK(urlWithQuery("http://h/gen?c=true", "", out, sizeof out));
  CHECK_STR(out, "http://h/gen?c=true");
  CHECK(urlWithQuery("http://h/gen", "", out, sizeof out));
  CHECK_STR(out, "http://h/gen");
}

static void saysSoWhenTheURLDoesNotFit(void) {
  char out[32];
  /* "http://h/gen?c=1&count=5" is 24 characters and needs 25 bytes. */
  CHECK(!urlWithQuery("http://h/gen?c=1", "count=5", out, 24));
  CHECK(urlWithQuery("http://h/gen?c=1", "count=5", out, 25));
  CHECK_STR(out, "http://h/gen?c=1&count=5");
  CHECK(!urlWithQuery("http://host.example/gen?c=true", "count=5", out, 8));
}

static void saysDurationsTheWayAPersonDoes(void) {
  char out[32];
  formatDuration(0, out, sizeof out);
  CHECK_STR(out, "0.0 s");
  formatDuration(-3, out, sizeof out);
  CHECK_STR(out, "0.0 s");
  formatDuration(7.24, out, sizeof out);
  CHECK_STR(out, "7.2 s");
  formatDuration(9.94, out, sizeof out);
  CHECK_STR(out, "9.9 s");
  formatDuration(9.96, out, sizeof out);
  CHECK_STR(out, "10 s");
  formatDuration(45.6, out, sizeof out);
  CHECK_STR(out, "46 s");
  formatDuration(45.5, out, sizeof out);
  CHECK_STR(out, "46 s");
  formatDuration(59.4, out, sizeof out);
  CHECK_STR(out, "59 s");
  /* Rounding up to 60 is a minute, not "60 s". */
  formatDuration(59.6, out, sizeof out);
  CHECK_STR(out, "1 min 0 s");
  formatDuration(72, out, sizeof out);
  CHECK_STR(out, "1 min 12 s");
  formatDuration(185, out, sizeof out);
  CHECK_STR(out, "3 min 5 s");
  formatDuration(3725, out, sizeof out);
  CHECK_STR(out, "1 h 2 min");
  formatDuration(7140, out, sizeof out);
  CHECK_STR(out, "1 h 59 min");
  formatDuration(3600, out, sizeof out);
  CHECK_STR(out, "1 h 0 min");
  formatDuration(9.95, out, sizeof out);
  CHECK_STR(out, "10 s");
}

static void summarisesARun(void) {
  char out[128];
  formatStats(7, 10, 72, out, sizeof out);
  CHECK_STR(out, "7 of 10 right (70%), 1 min 12 s, 7.2 s a question");
  formatStats(2, 3, 30, out, sizeof out);
  CHECK_STR(out, "2 of 3 right (67%), 30 s, 10 s a question");
  formatStats(0, 4, 8, out, sizeof out);
  CHECK_STR(out, "0 of 4 right (0%), 8.0 s, 2.0 s a question");
  formatStats(10, 10, 31, out, sizeof out);
  CHECK_STR(out, "10 of 10 right (100%), 31 s, 3.1 s a question");
  formatStats(1, 8, 16, out, sizeof out);
  CHECK_STR(out, "1 of 8 right (13%), 16 s, 2.0 s a question");
  formatStats(0, 0, 0, out, sizeof out);
  CHECK_STR(out, "0 of 0 right (0%), 0.0 s");
}

static void keepsAMistakeOffAPerfectScore(void) {
  char out[128];
  formatStats(199, 200, 600, out, sizeof out);
  CHECK_STR(out, "199 of 200 right (99%), 10 min 0 s, 3.0 s a question");
  formatStats(1, 201, 600, out, sizeof out);
  CHECK_STR(out, "1 of 201 right (1%), 10 min 0 s, 3.0 s a question");
  formatStats(399, 400, 600, out, sizeof out);
  CHECK_STR(out, "399 of 400 right (99%), 10 min 0 s, 1.5 s a question");
  formatStats(198, 200, 600, out, sizeof out);
  CHECK_STR(out, "198 of 200 right (99%), 10 min 0 s, 3.0 s a question");
}

static void doesNotRepeatItselfForOneAnswer(void) {
  char out[128];
  formatStats(1, 1, 3.2, out, sizeof out);
  CHECK_STR(out, "1 of 1 right (100%), 3.2 s");
}

static void namesTheSlowestQuestionOfALongEnoughRun(void) {
  char out[64];
  CHECK(formatSlowest(4, "18/3-1", 21.4, 10, out, sizeof out));
  CHECK_STR(out, "Task 4. 18/3-1 (21 s)");
  CHECK(formatSlowest(12, "2+3*4", 2.0, 3, out, sizeof out));
  CHECK_STR(out, "Task 12. 2+3*4 (2.0 s)");
  CHECK(!formatSlowest(4, "18/3-1", 21.4, 10, out, 21));
  CHECK(formatSlowest(4, "18/3-1", 21.4, 10, out, 22));
}

static void doesNotNameTheSlowestWhenThereIsNoneToName(void) {
  char out[64] = "untouched";
  /* Two questions are not enough to pick one out of. */
  CHECK(!formatSlowest(2, "1+1", 30, 2, out, sizeof out));
  /* Nor are questions that all went by quickly. */
  CHECK(!formatSlowest(2, "1+1", 1.9, 10, out, sizeof out));
  CHECK(!formatSlowest(0, "", 0, 0, out, sizeof out));
  /* Nor is a line that would be cut short. */
  CHECK(!formatSlowest(4, "18/3-1", 21, 10, out, 12));
}

static void neverWritesPastTheRoomItWasGiven(void) {
  char out[12];
  memset(out, 'x', sizeof out);
  formatStats(7, 10, 72, out, 10);
  CHECK(strlen(out) <= 9);
  CHECK(out[10] == 'x' && out[11] == 'x');
  formatDuration(3725, out, 4);
  CHECK(strlen(out) <= 3);
}

/* Garbage in, nothing out of bounds: random text through the parsers and
   random room through the formatters. Run it with -fsanitize=address,undefined
   (see the README) and an overrun is an error; without, the canary bytes after
   the room given still show one. */
static unsigned long long randomState = 88172645463325252ULL;

static unsigned randomBelow(unsigned limit) {
  randomState ^= randomState << 13;
  randomState ^= randomState >> 7;
  randomState ^= randomState << 17;
  return (unsigned)(randomState % limit);
}

/* Whether nothing after `capacity` was written to, and, when `written` is set,
   `out` holds a string that ends inside it. */
static int keptToTheRoom(const char* out, size_t capacity, size_t whole,
                         int written) {
  if (!capacity) return 1;
  if (written && !memchr(out, '\0', capacity)) return 0;
  for (size_t i = capacity; i < whole; i++)
    if (out[i] != 'x') return 0;
  return 1;
}

static void survivesRandomInput(void) {
  static const char alphabet[] = "0123456789+-*/() ,\n\r\tab\0";
  char text[1200];
  Question questions[8];
  int safe = 1;
  for (int round = 0; round < 20000; round++) {
    size_t length = randomBelow(sizeof text - 1);
    for (size_t i = 0; i < length; i++)
      text[i] = alphabet[randomBelow(sizeof alphabet - 1)];
    text[length] = '\0';
    Question one;
    if (parseQuestion(text, &one)) safe &= strlen(one.expression) > 0;
    safe &= parseQuestions(text, questions, 8) <= 8;

    char out[48];
    memset(out, 'x', sizeof out);
    size_t room = randomBelow(sizeof out + 1);
    urlWithQuery(text, "count=5", out, room);
    safe &= keptToTheRoom(out, room, sizeof out, 1);
    memset(out, 'x', sizeof out);
    room = randomBelow(sizeof out + 1);
    formatStats((long)randomBelow(300), (long)randomBelow(300),
                randomBelow(1u << (1 + randomBelow(20))) / 7.0, out, room);
    safe &= keptToTheRoom(out, room, sizeof out, 1);
    memset(out, 'x', sizeof out);
    room = randomBelow(sizeof out + 1);
    formatDuration(randomBelow(1u << (1 + randomBelow(24))) / 3.0, out, room);
    safe &= keptToTheRoom(out, room, sizeof out, 1);
    memset(out, 'x', sizeof out);
    room = randomBelow(sizeof out + 1);
    int worth = formatSlowest((long)randomBelow(1000), text, randomBelow(60),
                              (long)randomBelow(8), out, room);
    safe &= keptToTheRoom(out, room, sizeof out, worth);
  }
  CHECK(safe);
}

/* The result of parseArgs for a command line given as a list of words. */
static ArgsResult args(Options* options, const char** culprit, int n, ...) {
  char* argv[16] = {"samath"};
  va_list words;
  va_start(words, n);
  for (int i = 0; i < n; i++) argv[i + 1] = va_arg(words, char*);
  va_end(words);
  return parseArgs(n + 1, argv, options, culprit);
}

static void readsTheNumberOfQuestions(void) {
  Options o;
  const char* culprit;
  CHECK(args(&o, &culprit, 1, "5") == ARGS_OK);
  CHECK(o.questions == 5 && o.seconds == 0 && o.seed == -1);
  CHECK(args(&o, &culprit, 1, "1") == ARGS_OK && o.questions == 1);
  CHECK(args(&o, &culprit, 1, "2147483647") == ARGS_OK);
  CHECK(args(&o, &culprit, 0) == ARGS_NONE);
}

static void refusesWhatIsNotANumberOfQuestions(void) {
  Options o;
  const char* culprit;
  const char* bad[] = {"0",   "-0", "-3", "abc", "2.5", "", "5x",
                       "2147483648", "99999999999999999999", "-"};
  for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
    CHECK(args(&o, &culprit, 1, bad[i]) == ARGS_BAD_COUNT);
    CHECK_STR(culprit, bad[i]);
  }
}

static void takesTheTimeLimitInEveryForm(void) {
  Options o;
  const char* culprit;
  CHECK(args(&o, &culprit, 3, "-t", "10", "5") == ARGS_OK);
  CHECK(o.questions == 5 && o.seconds == 10);
  CHECK(args(&o, &culprit, 3, "5", "-t", "10") == ARGS_OK);
  CHECK(o.questions == 5 && o.seconds == 10);
  CHECK(args(&o, &culprit, 2, "-t10", "5") == ARGS_OK && o.seconds == 10);
  CHECK(args(&o, &culprit, 3, "--time", "7", "5") == ARGS_OK && o.seconds == 7);
  CHECK(args(&o, &culprit, 2, "--time=3600", "5") == ARGS_OK);
  CHECK(o.seconds == 3600);
  CHECK(args(&o, &culprit, 4, "-t", "3", "-t", "4") == ARGS_NONE);
  CHECK(args(&o, &culprit, 5, "-t", "3", "-t", "4", "9") == ARGS_OK);
  CHECK(o.seconds == 4);
}

static void refusesATimeLimitItCannotUse(void) {
  Options o;
  const char* culprit;
  CHECK(args(&o, &culprit, 3, "-t", "0", "5") == ARGS_BAD_TIME);
  CHECK_STR(culprit, "0");
  CHECK(args(&o, &culprit, 3, "-t", "3601", "5") == ARGS_BAD_TIME);
  CHECK(args(&o, &culprit, 3, "-t", "-4", "5") == ARGS_BAD_TIME);
  CHECK(args(&o, &culprit, 3, "-t", "x", "5") == ARGS_BAD_TIME);
  CHECK_STR(culprit, "x");
  CHECK(args(&o, &culprit, 3, "-t", "2.5", "5") == ARGS_BAD_TIME);
  CHECK(args(&o, &culprit, 2, "--time=", "5") == ARGS_BAD_TIME);
}

static void countsTheSecondsLeftUpForACountdown(void) {
  CHECK(secondsLeft(0) == 0);
  CHECK(secondsLeft(0.5) == 1);
  CHECK(secondsLeft(1) == 1);
  CHECK(secondsLeft(1.000001) == 2);
  CHECK(secondsLeft(9.999) == 10);
  CHECK(secondsLeft(59.2) == 60);
}

static void takesTheWholeRunsTimeInEveryForm(void) {
  Options o;
  const char* culprit;
  CHECK(args(&o, &culprit, 3, "-T", "300", "5") == ARGS_OK);
  CHECK(o.questions == 5 && o.total == 300 && o.seconds == 0);
  CHECK(args(&o, &culprit, 3, "5", "--total", "60") == ARGS_OK && o.total == 60);
  CHECK(args(&o, &culprit, 2, "-T300", "5") == ARGS_OK && o.total == 300);
  CHECK(args(&o, &culprit, 2, "--total=3600", "5") == ARGS_OK && o.total == 3600);
  CHECK(args(&o, &culprit, 5, "-t", "10", "-T", "90", "5") == ARGS_OK);
  CHECK(o.seconds == 10 && o.total == 90);
  CHECK(args(&o, &culprit, 2, "-pT30", "5") == ARGS_OK && o.practice && o.total == 30);
  CHECK(args(&o, &culprit, 3, "-T", "1", "5") == ARGS_OK && o.total == 1);
}

static void refusesAWholeRunsTimeItCannotUse(void) {
  Options o;
  const char* culprit;
  CHECK(args(&o, &culprit, 3, "-T", "0", "5") == ARGS_BAD_TOTAL);
  CHECK_STR(culprit, "0");
  CHECK(args(&o, &culprit, 3, "-T", "3601", "5") == ARGS_BAD_TOTAL);
  CHECK(args(&o, &culprit, 3, "-T", "-4", "5") == ARGS_BAD_TOTAL);
  CHECK(args(&o, &culprit, 3, "-T", "soon", "5") == ARGS_BAD_TOTAL);
  CHECK_STR(culprit, "soon");
  CHECK(args(&o, &culprit, 2, "--total=", "5") == ARGS_BAD_TOTAL);
  CHECK(args(&o, &culprit, 2, "5", "-T") == ARGS_NO_VALUE);
  CHECK_STR(culprit, "-T");
}

static void saysWhenAnOptionHasNoValue(void) {
  Options o;
  const char* culprit;
  CHECK(args(&o, &culprit, 2, "5", "-t") == ARGS_NO_VALUE);
  CHECK_STR(culprit, "-t");
  CHECK(args(&o, &culprit, 2, "5", "--time") == ARGS_NO_VALUE);
  CHECK_STR(culprit, "--time");
  CHECK(args(&o, &culprit, 2, "5", "--operators") == ARGS_NO_VALUE);
  CHECK_STR(culprit, "--operators");
}

static void takesTheSeedInEveryForm(void) {
  Options o;
  const char* culprit;
  CHECK(args(&o, &culprit, 3, "-s", "42", "5") == ARGS_OK);
  CHECK(o.questions == 5 && o.seed == 42);
  CHECK(args(&o, &culprit, 2, "5", "-s7") == ARGS_OK && o.seed == 7);
  CHECK(args(&o, &culprit, 3, "--seed", "0", "5") == ARGS_OK && o.seed == 0);
  CHECK(args(&o, &culprit, 2, "--seed=4294967295", "5") == ARGS_OK);
  CHECK(o.seed == 4294967295LL);
  CHECK(args(&o, &culprit, 5, "-s", "3", "-t", "9", "5") == ARGS_OK);
  CHECK(o.seed == 3 && o.seconds == 9 && o.questions == 5);
}

static void takesPractice(void) {
  Options o;
  const char* culprit;
  CHECK(args(&o, &culprit, 1, "5") == ARGS_OK && o.practice == 0);
  CHECK(args(&o, &culprit, 2, "-p", "5") == ARGS_OK && o.practice == 1);
  CHECK(args(&o, &culprit, 2, "5", "--practice") == ARGS_OK && o.practice == 1);
  CHECK(args(&o, &culprit, 6, "-p", "-t", "4", "-s", "1", "5") == ARGS_OK);
  CHECK(o.practice == 1 && o.seconds == 4 && o.seed == 1 && o.questions == 5);
  /* It takes no value. */
  CHECK(args(&o, &culprit, 2, "--practice=yes", "5") == ARGS_UNWANTED_VALUE);
  CHECK_STR(culprit, "--practice=yes");
  CHECK(args(&o, &culprit, 2, "--practice=", "5") == ARGS_UNWANTED_VALUE);
}

static void takesFeedback(void) {
  Options o;
  const char* culprit;
  CHECK(args(&o, &culprit, 1, "5") == ARGS_OK && o.feedback == 0);
  CHECK(args(&o, &culprit, 2, "-f", "5") == ARGS_OK && o.feedback == 1);
  CHECK(args(&o, &culprit, 2, "5", "--feedback") == ARGS_OK && o.feedback == 1);
  CHECK(args(&o, &culprit, 4, "-p", "-f", "-b", "5") == ARGS_OK);
  CHECK(o.feedback == 1 && o.practice == 1 && o.brackets == 1);
  CHECK(args(&o, &culprit, 2, "--feedback=yes", "5") == ARGS_UNWANTED_VALUE);
}

static void takesWorking(void) {
  Options o;
  const char* culprit;
  CHECK(args(&o, &culprit, 1, "5") == ARGS_OK && o.working == 0);
  CHECK(args(&o, &culprit, 2, "-w", "5") == ARGS_OK && o.working == 1);
  CHECK(args(&o, &culprit, 2, "5", "--working") == ARGS_OK && o.working == 1);
  CHECK(args(&o, &culprit, 3, "-pw", "-f", "5") == ARGS_OK);
  CHECK(o.working == 1 && o.practice == 1 && o.feedback == 1);
  CHECK(args(&o, &culprit, 2, "--working=yes", "5") == ARGS_UNWANTED_VALUE);
}

static void takesTheLengthAndBrackets(void) {
  Options o;
  const char* culprit;
  CHECK(args(&o, &culprit, 1, "5") == ARGS_OK);
  CHECK(o.length == 0 && o.brackets == 0);
  CHECK(args(&o, &culprit, 3, "-l", "6", "5") == ARGS_OK && o.length == 6);
  CHECK(args(&o, &culprit, 2, "5", "-l2") == ARGS_OK && o.length == 2);
  CHECK(args(&o, &culprit, 3, "--length", "12", "5") == ARGS_OK);
  CHECK(o.length == 12);
  CHECK(args(&o, &culprit, 2, "--length=7", "5") == ARGS_OK && o.length == 7);
  CHECK(args(&o, &culprit, 2, "-b", "5") == ARGS_OK && o.brackets == 1);
  CHECK(args(&o, &culprit, 2, "5", "--brackets") == ARGS_OK);
  CHECK(o.brackets == 1);
  CHECK(args(&o, &culprit, 5, "-b", "-l", "4", "-p", "5") == ARGS_OK);
  CHECK(o.brackets == 1 && o.length == 4 && o.practice == 1);
  CHECK(args(&o, &culprit, 2, "--brackets=yes", "5") == ARGS_UNWANTED_VALUE);
  CHECK(args(&o, &culprit, 2, "-B", "5") == ARGS_OK && o.brackets == BRACKETS_NEVER);
  CHECK(args(&o, &culprit, 2, "5", "--no-brackets") == ARGS_OK);
  CHECK(o.brackets == BRACKETS_NEVER);
  CHECK(args(&o, &culprit, 3, "-pB", "-l4", "5") == ARGS_OK);
  CHECK(o.brackets == BRACKETS_NEVER && o.practice == 1 && o.length == 4);
  CHECK(args(&o, &culprit, 3, "-B", "-b", "5") == ARGS_OK && o.brackets == BRACKETS_ASK);
  CHECK(args(&o, &culprit, 3, "-b", "-B", "5") == ARGS_OK && o.brackets == BRACKETS_NEVER);
  CHECK(args(&o, &culprit, 2, "--no-brackets=yes", "5") == ARGS_UNWANTED_VALUE);
}

static void takesNoNegatives(void) {
  Options o;
  const char* culprit;
  CHECK(args(&o, &culprit, 1, "5") == ARGS_OK && o.noNegatives == 0);
  CHECK(args(&o, &culprit, 2, "-n", "5") == ARGS_OK && o.noNegatives == 1);
  CHECK(args(&o, &culprit, 2, "5", "--no-negatives") == ARGS_OK);
  CHECK(o.noNegatives == 1);
  CHECK(args(&o, &culprit, 5, "-n", "-l", "3", "-b", "5") == ARGS_OK);
  CHECK(o.noNegatives == 1 && o.length == 3 && o.brackets == 1);
  CHECK(args(&o, &culprit, 2, "--no-negatives=yes", "5") == ARGS_UNWANTED_VALUE);
  CHECK(args(&o, &culprit, 2, "-N", "5") == ARGS_OK && o.noNegatives == NEGATIVES_ASK);
  CHECK(args(&o, &culprit, 2, "5", "--negatives") == ARGS_OK);
  CHECK(o.noNegatives == NEGATIVES_ASK);
  CHECK(args(&o, &culprit, 3, "-pN", "-l4", "5") == ARGS_OK);
  CHECK(o.noNegatives == NEGATIVES_ASK && o.practice == 1 && o.length == 4);
  CHECK(args(&o, &culprit, 3, "-n", "-N", "5") == ARGS_OK && o.noNegatives == NEGATIVES_ASK);
  CHECK(args(&o, &culprit, 3, "-N", "-n", "5") == ARGS_OK && o.noNegatives == 1);
  CHECK(args(&o, &culprit, 2, "--negatives=yes", "5") == ARGS_UNWANTED_VALUE);
}

static void takesAnAnswerRange(void) {
  Options o;
  const char* culprit;
  CHECK(args(&o, &culprit, 1, "5") == ARGS_OK && o.ranged == 0);
  CHECK(args(&o, &culprit, 3, "-r", "50-100", "5") == ARGS_OK);
  CHECK(o.ranged == 1 && o.rangeLow == 50 && o.rangeHigh == 100);
  CHECK(args(&o, &culprit, 2, "5", "-r0-20") == ARGS_OK);
  CHECK(o.ranged == 1 && o.rangeLow == 0 && o.rangeHigh == 20);
  CHECK(args(&o, &culprit, 2, "--range=9900-10000", "5") == ARGS_OK);
  CHECK(o.rangeLow == 9900 && o.rangeHigh == 10000);
  CHECK(args(&o, &culprit, 3, "--range", "7-7", "5") == ARGS_OK);
  CHECK(o.rangeLow == 7 && o.rangeHigh == 7);
  CHECK(args(&o, &culprit, 3, "-r", "10000-10000", "5") == ARGS_OK);
  CHECK(o.rangeLow == 10000 && o.rangeHigh == 10000);
  CHECK(args(&o, &culprit, 3, "-pr", "1-5", "5") == ARGS_OK && o.practice);
  const char* const bad[] = {"20-10", "0-101", "10", "10-", "-10", "a-b",
                             "1-2-3", "10000-10001", "1 - 5", "+1-5",
                             "1e1-20", ""};
  for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
    CHECK(args(&o, &culprit, 3, "-r", bad[i], "5") == ARGS_BAD_RANGE);
    CHECK_STR(culprit, bad[i]);
  }
  CHECK(args(&o, &culprit, 2, "5", "-r") == ARGS_NO_VALUE);
  CHECK(args(&o, &culprit, 2, "--range=", "5") == ARGS_BAD_RANGE);
}

static void refusesRangeAboveMax(void) {
  Options o;
  const char* culprit;
  CHECK(args(&o, &culprit, 5, "-m", "20", "-r", "50-100", "5") ==
        ARGS_RANGE_ABOVE_MAX);
  CHECK(o.max == 20 && o.rangeLow == 50 && o.rangeHigh == 100);
  CHECK(args(&o, &culprit, 4, "5", "-r", "21-30", "-m20") ==
        ARGS_RANGE_ABOVE_MAX);
  CHECK(args(&o, &culprit, 5, "-m", "20", "-r", "15-30", "5") ==
        ARGS_RANGE_HIGH_ABOVE_MAX);
  CHECK(o.max == 20 && o.rangeLow == 15 && o.rangeHigh == 30);
  CHECK(args(&o, &culprit, 5, "-m", "20", "-r", "0-100", "5") ==
        ARGS_RANGE_HIGH_ABOVE_MAX);
  /* Answers up to the largest number, and no -m at all, are fine. */
  CHECK(args(&o, &culprit, 5, "-m", "20", "-r", "10-20", "5") == ARGS_OK);
  CHECK(args(&o, &culprit, 5, "-m", "20", "-r", "0-20", "5") == ARGS_OK);
  CHECK(args(&o, &culprit, 5, "-m", "20", "-r", "20-20", "5") == ARGS_OK);
  CHECK(args(&o, &culprit, 3, "-r", "50-100", "5") == ARGS_OK);
}

static void takesMaxAndOperators(void) {
  Options o;
  const char* culprit;
  CHECK(args(&o, &culprit, 1, "5") == ARGS_OK);
  CHECK(o.max == 0 && !strcmp(o.operators, ""));
  CHECK(args(&o, &culprit, 3, "-m", "20", "5") == ARGS_OK && o.max == 20);
  CHECK(args(&o, &culprit, 2, "5", "-m10") == ARGS_OK && o.max == 10);
  CHECK(args(&o, &culprit, 2, "--max=10000", "5") == ARGS_OK);
  CHECK(o.max == 10000);
  CHECK(args(&o, &culprit, 3, "-o", "add,subtract", "5") == ARGS_OK);
  CHECK_STR(o.operators, "add,subtract");
  CHECK(args(&o, &culprit, 2, "5", "--operators=divide") == ARGS_OK);
  CHECK_STR(o.operators, "divide");
  CHECK(args(&o, &culprit, 3, "-o", "divide,multiply,subtract,add", "5") ==
        ARGS_OK);
  CHECK_STR(o.operators, "divide,multiply,subtract,add");
  CHECK(args(&o, &culprit, 3, "-pm20o", "multiply", "5") == ARGS_BAD_MAX);
  CHECK(args(&o, &culprit, 3, "-pfom", "add", "5") == ARGS_BAD_OPERATORS);
  CHECK(args(&o, &culprit, 3, "-pm", "20", "5") == ARGS_OK && o.practice);
  CHECK(args(&o, &culprit, 6, "-m", "20", "-o", "add", "-n", "5") == ARGS_OK);
  CHECK(o.max == 20 && o.noNegatives && !strcmp(o.operators, "add"));
}

static void refusesAMaxItCannotUse(void) {
  Options o;
  const char* culprit;
  CHECK(args(&o, &culprit, 3, "-m", "9", "5") == ARGS_BAD_MAX);
  CHECK_STR(culprit, "9");
  CHECK(args(&o, &culprit, 3, "-m", "10001", "5") == ARGS_BAD_MAX);
  CHECK(args(&o, &culprit, 3, "-m", "-20", "5") == ARGS_BAD_MAX);
  CHECK(args(&o, &culprit, 3, "-m", "twenty", "5") == ARGS_BAD_MAX);
  CHECK(args(&o, &culprit, 3, "-m", "2.5e1", "5") == ARGS_BAD_MAX);
  CHECK(args(&o, &culprit, 2, "5", "-m") == ARGS_NO_VALUE);
  CHECK_STR(culprit, "-m");
  CHECK(args(&o, &culprit, 2, "--max=", "5") == ARGS_BAD_MAX);
}

static void refusesOperatorsItCannotUse(void) {
  Options o;
  const char* culprit;
  CHECK(args(&o, &culprit, 3, "-o", "plus", "5") == ARGS_BAD_OPERATORS);
  CHECK_STR(culprit, "plus");
  CHECK(args(&o, &culprit, 3, "-o", "add,add", "5") == ARGS_BAD_OPERATORS);
  CHECK(args(&o, &culprit, 3, "-o", "add,", "5") == ARGS_BAD_OPERATORS);
  CHECK(args(&o, &culprit, 3, "-o", ",add", "5") == ARGS_BAD_OPERATORS);
  CHECK(args(&o, &culprit, 3, "-o", "add,,divide", "5") == ARGS_BAD_OPERATORS);
  CHECK(args(&o, &culprit, 3, "-o", "add, divide", "5") == ARGS_BAD_OPERATORS);
  CHECK(args(&o, &culprit, 3, "-o", "Add", "5") == ARGS_BAD_OPERATORS);
  CHECK(args(&o, &culprit, 3, "-o", "addd", "5") == ARGS_BAD_OPERATORS);
  CHECK(args(&o, &culprit, 3, "-o", "ad", "5") == ARGS_BAD_OPERATORS);
  CHECK(args(&o, &culprit, 3, "-o", "", "5") == ARGS_BAD_OPERATORS);
  CHECK(args(&o, &culprit, 3, "-o", "+", "5") == ARGS_BAD_OPERATORS);
  CHECK(args(&o, &culprit, 3, "-o",
             "add,subtract,multiply,divide,add,subtract,multiply,divide", "5") ==
        ARGS_BAD_OPERATORS);
  CHECK(args(&o, &culprit, 2, "5", "-o") == ARGS_NO_VALUE);
  CHECK_STR(culprit, "-o");
}

static void refusesALengthItCannotUse(void) {
  Options o;
  const char* culprit;
  CHECK(args(&o, &culprit, 3, "-l", "1", "5") == ARGS_BAD_LENGTH);
  CHECK_STR(culprit, "1");
  CHECK(args(&o, &culprit, 3, "-l", "13", "5") == ARGS_BAD_LENGTH);
  CHECK(args(&o, &culprit, 3, "-l", "-4", "5") == ARGS_BAD_LENGTH);
  CHECK(args(&o, &culprit, 3, "-l", "long", "5") == ARGS_BAD_LENGTH);
  CHECK_STR(culprit, "long");
  CHECK(args(&o, &culprit, 3, "-l", "4.5", "5") == ARGS_BAD_LENGTH);
  CHECK(args(&o, &culprit, 2, "5", "-l") == ARGS_NO_VALUE);
  CHECK_STR(culprit, "-l");
  CHECK(args(&o, &culprit, 2, "--length=", "5") == ARGS_BAD_LENGTH);
}

static void refusesASeedItCannotUse(void) {
  Options o;
  const char* culprit;
  CHECK(args(&o, &culprit, 3, "-s", "-1", "5") == ARGS_BAD_SEED);
  CHECK_STR(culprit, "-1");
  CHECK(args(&o, &culprit, 3, "-s", "4294967296", "5") == ARGS_BAD_SEED);
  CHECK(args(&o, &culprit, 3, "-s", "abc", "5") == ARGS_BAD_SEED);
  CHECK_STR(culprit, "abc");
  CHECK(args(&o, &culprit, 3, "-s", "1.5", "5") == ARGS_BAD_SEED);
  CHECK(args(&o, &culprit, 2, "5", "-s") == ARGS_NO_VALUE);
  CHECK_STR(culprit, "-s");
  CHECK(args(&o, &culprit, 2, "--seed=", "5") == ARGS_BAD_SEED);
}

static void takesShortOptionsTogether(void) {
  Options o;
  const char* culprit;
  CHECK(args(&o, &culprit, 2, "-pf", "5") == ARGS_OK);
  CHECK(o.practice && o.feedback && !o.brackets && !o.noNegatives);
  CHECK(args(&o, &culprit, 2, "5", "-nbfp") == ARGS_OK);
  CHECK(o.practice && o.feedback && o.brackets && o.noNegatives);
  /* The first that takes a value takes the rest of the word, or the next. */
  CHECK(args(&o, &culprit, 3, "-pt", "10", "5") == ARGS_OK);
  CHECK(o.practice && o.seconds == 10 && o.questions == 5);
  CHECK(args(&o, &culprit, 2, "-pt10", "5") == ARGS_OK && o.seconds == 10);
  CHECK(args(&o, &culprit, 2, "-fs42", "5") == ARGS_OK);
  CHECK(o.feedback && o.seed == 42);
  CHECK(args(&o, &culprit, 3, "-bl", "6", "5") == ARGS_OK);
  CHECK(o.brackets && o.length == 6);
  /* A letter after the one that takes a value is part of the value. */
  CHECK(args(&o, &culprit, 3, "-tp", "10", "5") == ARGS_BAD_TIME);
  CHECK_STR(culprit, "p");
  CHECK(args(&o, &culprit, 2, "5", "-pt") == ARGS_NO_VALUE);
  CHECK_STR(culprit, "-pt");
  CHECK(args(&o, &culprit, 2, "-px", "5") == ARGS_BAD_OPTION);
  CHECK_STR(culprit, "-x");
  CHECK(args(&o, &culprit, 2, "-xp", "5") == ARGS_BAD_OPTION);
  CHECK_STR(culprit, "-x");
  CHECK(args(&o, &culprit, 2, "-pé", "5") == ARGS_BAD_OPTION);
  CHECK_STR(culprit, "-pé");
  /* Long options are not joined, and take a value only if they take one. */
  CHECK(args(&o, &culprit, 2, "--practice=1", "5") == ARGS_UNWANTED_VALUE);
  CHECK(args(&o, &culprit, 2, "--pf", "5") == ARGS_BAD_OPTION);
  CHECK(args(&o, &culprit, 3, "-pf", "--time=9", "5") == ARGS_OK);
  CHECK(o.seconds == 9 && o.practice && o.feedback);
}

static void asksForHelpAmongShortOptions(void) {
  Options o;
  const char* culprit;
  CHECK(args(&o, &culprit, 1, "-ph") == ARGS_HELP);
  CHECK(args(&o, &culprit, 2, "-hf", "0") == ARGS_HELP);
  CHECK(args(&o, &culprit, 2, "-xh", "0") == ARGS_HELP);
  CHECK(args(&o, &culprit, 3, "-pfh", "-t", "x") == ARGS_HELP);
  /* After an option that takes a value, h is part of the value. */
  CHECK(args(&o, &culprit, 2, "-th", "5") == ARGS_BAD_TIME);
  CHECK(args(&o, &culprit, 3, "--", "-ph", "5") == ARGS_BAD_COUNT);
  CHECK(args(&o, &culprit, 1, "--help=1") == ARGS_UNWANTED_VALUE);
}

static void refusesOptionsThatDoNotExist(void) {
  Options o;
  const char* culprit;
  CHECK(args(&o, &culprit, 2, "-x", "5") == ARGS_BAD_OPTION);
  CHECK_STR(culprit, "-x");
  CHECK(args(&o, &culprit, 2, "--nope", "5") == ARGS_BAD_OPTION);
  CHECK(args(&o, &culprit, 2, "--timeout=5", "5") == ARGS_BAD_OPTION);
  CHECK_STR(culprit, "--timeout=5");
  CHECK(args(&o, &culprit, 2, "--seeds=5", "5") == ARGS_BAD_OPTION);
}

static void takesPrint(void) {
  Options o;
  const char* culprit;
  CHECK(args(&o, &culprit, 1, "5") == ARGS_OK && o.print == 0);
  CHECK(args(&o, &culprit, 2, "-P", "5") == ARGS_OK && o.print == 1);
  CHECK(args(&o, &culprit, 2, "5", "--print") == ARGS_OK && o.print == 1);
  CHECK(args(&o, &culprit, 2, "-Pf", "5") == ARGS_OK && o.print == 1 && o.feedback == 1);
  CHECK(args(&o, &culprit, 2, "-p", "5") == ARGS_OK && o.print == 0 && o.practice == 1);
  CHECK(args(&o, &culprit, 2, "--print=yes", "5") == ARGS_UNWANTED_VALUE);
}

static void asksForTheVersion(void) {
  Options o;
  const char* culprit;
  CHECK(args(&o, &culprit, 1, "--version") == ARGS_VERSION);
  CHECK(args(&o, &culprit, 2, "5", "--version") == ARGS_VERSION);
  CHECK(args(&o, &culprit, 2, "--", "--version") == ARGS_BAD_COUNT);
  CHECK(args(&o, &culprit, 2, "--version", "-h") == ARGS_HELP);
  CHECK(args(&o, &culprit, 1, "--version=1") == ARGS_UNWANTED_VALUE);
}

static void suggestsTheOptionThatWasMeant(void) {
  CHECK_STR(nearestOption("--practise"), "--practice");
  CHECK_STR(nearestOption("--timeout=5"), "--time");
  CHECK_STR(nearestOption("--Feedbak"), "--feedback");
  CHECK_STR(nearestOption("--no-negative"), "--no-negatives");
  CHECK_STR(nearestOption("--bracket"), "--brackets");
  CHECK_STR(nearestOption("--helpp"), "--help");
  CHECK_STR(nearestOption("--versoin"), "--version");
  CHECK(nearestOption("--nope") == NULL);
  CHECK(nearestOption("--zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz") == NULL);
  CHECK(nearestOption("-x") == NULL);
  CHECK(nearestOption("--") == NULL);
  CHECK(nearestOption("--ma") == NULL);
  CHECK_STR(nearestOption("--prxxt"), "--print");
}

static void refusesASecondNumberOfQuestions(void) {
  Options o;
  const char* culprit;
  CHECK(args(&o, &culprit, 2, "5", "6") == ARGS_EXTRA);
  CHECK_STR(culprit, "6");
}

static void helpWinsOverEverythingElse(void) {
  Options o;
  const char* culprit;
  CHECK(args(&o, &culprit, 1, "-h") == ARGS_HELP);
  CHECK(args(&o, &culprit, 1, "--help") == ARGS_HELP);
  CHECK(args(&o, &culprit, 3, "0", "-x", "--help") == ARGS_HELP);
  CHECK(args(&o, &culprit, 3, "--", "-h", "5") == ARGS_BAD_COUNT);
}

static void aDoubleDashEndsTheOptions(void) {
  Options o;
  const char* culprit;
  CHECK(args(&o, &culprit, 3, "-t", "5", "--") == ARGS_NONE);
  CHECK(args(&o, &culprit, 2, "--", "5") == ARGS_OK && o.questions == 5);
  CHECK(args(&o, &culprit, 2, "--", "-5") == ARGS_BAD_COUNT);
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

static void countsEveryOptionThatReadsAsTheAnswer(void) {
  char* o[4] = {"9", "9", "4", "1"};
  CHECK(checkAnswer("A", "9", o[0], o[1], o[2], o[3]));
  CHECK(checkAnswer("B", "9", o[0], o[1], o[2], o[3]));
  CHECK(!checkAnswer("C", "9", o[0], o[1], o[2], o[3]));
  CHECK(!checkAnswer("D", "9", o[0], o[1], o[2], o[3]));
}

static void readsTheTasksOfAWorkedAnswer(void) {
  Question q[4];
  CHECK(parseWorkedQuestions("[{\"task\":[3,\"*\",4],\"answer\":12,\"quizOptions\":[10,12,11,9],\"working\":[\"3*4\",\"12\"]},"
                             "{\"task\":[\"(\",-5,\"+\",2,\")\"],\"answer\":-3,\"quizOptions\":[-3,-7,3,7],\"working\":[\"(-5+2)\",\"-3\"]}]",
                             q, 4) == 2);
  CHECK_STR(q[0].expression, "3*4");
  CHECK_STR(q[0].answer, "12");
  CHECK_STR(q[0].options[3], "9");
  CHECK_STR(q[0].working, "3*4 = 12");
  CHECK_STR(q[1].expression, "(-5+2)");
  CHECK_STR(q[1].answer, "-3");
  CHECK_STR(q[1].working, "(-5+2) = -3");
}

static void readsOneTaskAndTheOrderOfItsMembers(void) {
  Question q[2];
  CHECK(parseWorkedQuestions("{\"working\":[\"14-3\",\"11\"],\"answer\":11,"
                             "\"quizOptions\":[15,12,14,11],\"task\":[14,\"-\",3],"
                             "\"extra\":{\"deep\":[true,null,false,1.5e3]}}",
                             q, 2) == 1);
  CHECK_STR(q[0].expression, "14-3");
  CHECK_STR(q[0].working, "14-3 = 11");
  CHECK(parseWorkedQuestions("{\"task\":[14,\"-\",3],\"answer\":11,\"quizOptions\":[15,12,14,11]}", q, 2) == 1);
  CHECK_STR(q[0].working, "");
  CHECK(parseWorkedQuestions("{\"task\":[14,\"-\",3],\"answer\":11,\"quizOptions\":[15,12,14,11]},"
                             "{\"task\":[2,\"+\",2],\"answer\":4,\"quizOptions\":[4,5,6,7]}", q, 2) == 1);
  CHECK_STR(q[0].expression, "14-3");
}

static void skipsATaskThatIsNotAQuestion(void) {
  Question q[4];
  /* The second task's answer is not one of its options, so it is left out. */
  CHECK(parseWorkedQuestions("[{\"task\":[1,\"+\",1],\"answer\":2,\"quizOptions\":[2,3,4,5],\"working\":[\"1+1\",\"2\"]},"
                             "{\"task\":[2,\"+\",2],\"answer\":9,\"quizOptions\":[4,5,6,7]},"
                             "{\"task\":[\"x\"],\"answer\":4,\"quizOptions\":[4,5,6,7]},"
                             "{\"task\":[3,\"+\",3],\"answer\":6,\"quizOptions\":[9,8,7,6]}]",
                             q, 4) == 2);
  CHECK_STR(q[0].expression, "1+1");
  CHECK_STR(q[1].expression, "3+3");
  /* A member that is not an object is skipped as well. */
  CHECK(parseWorkedQuestions("[2,{\"task\":[1,\"+\",1],\"answer\":2,\"quizOptions\":[2,3,4,5]}]", q, 4) == 1);
  CHECK_STR(q[0].expression, "1+1");
  CHECK(parseWorkedQuestions("[\"x\",null,{\"task\":[1,\"+\",1],\"answer\":2,\"quizOptions\":[2,3,4,5]}]", q, 4) == 1);
  CHECK(parseWorkedQuestions("{}", q, 4) == 0);
  char ones[QUESTION_LINE_MAX];
  char json[QUESTION_LINE_MAX + 128];
  memset(ones, '1', sizeof ones - 1);
  ones[sizeof ones - 1] = '\0';
  /* A member too long to keep leaves the task short, and a short task is
     not the question it was. */
  snprintf(json, sizeof json, "{\"task\":[1,\"%s\",1],\"answer\":2,\"quizOptions\":[2,3,4,5]}", ones);
  CHECK(parseWorkedQuestions(json, q, 4) == 0);
  char full[QUESTION_LINE_MAX + 1];
  memset(full, '1', sizeof full - 1);
  full[sizeof full - 1] = '\0';
  snprintf(json, sizeof json, "{\"task\":[\"%s\"],\"answer\":2,\"quizOptions\":[2,3,4,5]}", full);
  CHECK(parseWorkedQuestions(json, q, 4) == 0);
}

static void dropsAWorkingThatIsNotPlainArithmetic(void) {
  Question q[2];
  char working[QUESTION_WORKING_MAX + 64];
  CHECK(parseWorkedQuestions("{\"task\":[1,\"+\",1],\"answer\":2,\"quizOptions\":[2,3,4,5],\"working\":[\"1+1\",\"2;rm\"]}",
                             q, 2) == 1);
  CHECK_STR(q[0].working, "");
  CHECK_STR(q[0].answer, "2");
  CHECK(parseWorkedQuestions("{\"task\":[1,\"+\",1],\"answer\":2,\"quizOptions\":[2,3,4,5],\"working\":[\"1+1\x1b[2J\",\"2\"]}",
                             q, 2) == 1);
  CHECK_STR(q[0].working, "");
  char big[QUESTION_WORKING_MAX + 64];
  char worked[QUESTION_WORKING_MAX + 256];
  memset(big, '1', sizeof big - 1);
  big[sizeof big - 1] = '\0';
  snprintf(worked, sizeof worked, "{\"task\":[1,\"+\",1],\"answer\":2,\"quizOptions\":[2,3,4,5],\"working\":[\"1+1\",\"%s\",\"2\"]}", big);
  CHECK(parseWorkedQuestions(worked, q, 2) == 1);
  CHECK_STR(q[0].working, "");
  /* A working too long for its room is left out, and the question kept. */
  size_t at = 0;
  while (at + 8 < sizeof working - 1) {
    memcpy(working + at, "1+1 = 2", 7);
    at += 7;
    working[at++] = ',';
  }
  working[at] = '\0';
  char json[QUESTION_WORKING_MAX + 256];
  snprintf(json, sizeof json, "{\"task\":[1,\"+\",1],\"answer\":2,\"quizOptions\":[2,3,4,5],\"working\":[\"%s\"]}", working);
  CHECK(parseWorkedQuestions(json, q, 2) == 1);
  CHECK_STR(q[0].working, "");
}

static void stopsAtWhatIsNotJSON(void) {
  Question q[4];
  const char* task = "{\"task\":[1,\"+\",1],\"answer\":2,\"quizOptions\":[2,3,4,5]}";
  CHECK(parseWorkedQuestions("", q, 4) == 0);
  CHECK(parseWorkedQuestions("<html>502</html>", q, 4) == 0);
  char cut[] = "{\"task\":[1,\"+\",1],\"answer\":2,\"quizOptions\":[2,3,4,5],\"x\":\"ab\\";
  CHECK(parseWorkedQuestions(cut, q, 4) == 0);
  CHECK(parseWorkedQuestions("1+1 2,3,4,5 2", q, 4) == 0);
  CHECK(parseWorkedQuestions("[]", q, 4) == 0);
  CHECK(parseWorkedQuestions("[", q, 4) == 0);
  CHECK(parseWorkedQuestions("{\"error\":\"no\"}", q, 4) == 0);
  CHECK(parseWorkedQuestions("[{\"task\":[1", q, 4) == 0);
  CHECK(parseWorkedQuestions("{\"task\":[1,\"+\",1],\"answer\":2.5,\"quizOptions\":[2,3,4,5]}", q, 4) == 0);
  /* A task whose answer is not a whole number is skipped, and the reading
     goes on past it. */
  CHECK(parseWorkedQuestions("[{\"task\":[1,\"+\",1],\"answer\":2.5,\"quizOptions\":[2,3,4,5]},"
                             "{\"task\":[1,\"+\",1],\"answer\":2,\"quizOptions\":[2,3,4,5]}]", q, 4) == 1);
  CHECK_STR(q[0].answer, "2");
  CHECK(parseWorkedQuestions("[{\"task\":[1,\"+\",1],\"answer\":2e0,\"quizOptions\":[2,3,4,5]},"
                             "{\"task\":[1,\"+\",1],\"answer\":2,\"quizOptions\":[2,3,4,5]}]", q, 4) == 1);
  CHECK_STR(q[0].answer, "2");
  CHECK(parseWorkedQuestions("{\"task\":[1,\"+\",1],\"answer\":2,\"quizOptions\":[2,3,4,5],\"working\":[\"1+1\\u0041\"]}", q, 4) == 1);
  CHECK_STR(q[0].working, "");
  CHECK(parseWorkedQuestions(task, q, 0) == 0);
  /* A list that stops after a good task still gives that task. */
  char list[256];
  snprintf(list, sizeof list, "[%s,%s", task, "{\"task\":[");
  CHECK(parseWorkedQuestions(list, q, 4) == 1);
  CHECK(parseWorkedQuestions(task, q, 4) == 1);
  CHECK_STR(q[0].answer, "2");
}

static void skipsWhatItDoesNotUse(void) {
  Question q[4];
  /* A working that is null or has an escape in it is left out, and the
     question kept; other fields are skipped, escapes and all. */
  CHECK(parseWorkedQuestions("[{\"task\":[1,\"+\",1],\"answer\":2,\"quizOptions\":[2,3,4,5],\"working\":null},"
                             "{\"msg\":\"say \\\"hi\\\"\",\"task\":[3,\"+\",3],\"answer\":6,\"quizOptions\":[9,8,7,6],"
                             "\"working\":[\"3+3\\u001b\",\"6\"]},"
                             "{\"task\":[2,\"+\",2],\"answer\":4,\"quizOptions\":[4,5,6,7],"
                             "\"x\":{\"s\":\"a\\\\\"},\"working\":[\"2+2\",\"4\"]}]",
                             q, 4) == 3);
  CHECK_STR(q[0].expression, "1+1");
  CHECK_STR(q[0].working, "");
  CHECK_STR(q[1].working, "");
  CHECK_STR(q[2].working, "2+2 = 4");
  /* A task that is not a list is a task that is not a question, and the
     reading goes on with the next one. */
  CHECK(parseWorkedQuestions("[{\"task\":null,\"answer\":2},{\"task\":[1,\"+\",1],\"answer\":2,\"quizOptions\":[2,3,4,5]}]", q, 4) == 1);
  CHECK_STR(q[0].expression, "1+1");
  CHECK(parseWorkedQuestions("[{\"x\":[],\"task\":[1,\"+\",1],\"y\":{},\"answer\":2,\"quizOptions\":[2,3,4,5],\"z\":[[],{}]}]", q, 4) == 1);
  CHECK_STR(q[0].expression, "1+1");
}

static void refusesDeepNesting(void) {
  Question q[2];
  char deep[600];
  size_t at = 0;
  while (at < 200) deep[at++] = '[';
  deep[at] = '\0';
  char json[700];
  snprintf(json, sizeof json, "[{\"task\":[1,\"+\",1],\"answer\":2,\"quizOptions\":[2,3,4,5],\"x\":%s}]", deep);
  CHECK(parseWorkedQuestions(json, q, 2) == 0);
  snprintf(json, sizeof json, "{\"x\":%s", deep);
  CHECK(parseWorkedQuestions(json, q, 2) == 0);
  CHECK(parseWorkedQuestions("[{\"task\":[1,\"+\",1],\"answer\":2,\"quizOptions\":[2,3,4,5],\"x\":[[1]]}]", q, 2) == 1);
  CHECK(parseWorkedQuestions("[{\"task\":[1,\"+\",1],\"answer\":2,\"quizOptions\":[2,3,4,5],\"x\":[[[1]]]}]", q, 2) == 0);
}

static void urlLosesAParameter(void) {
  char out[96];
  CHECK(urlWithoutParameter("http://h/gen?c=true&seed=3", "c", out, sizeof out));
  CHECK_STR(out, "http://h/gen?seed=3");
  CHECK(urlWithoutParameter("http://h/gen?c=true", "c", out, sizeof out));
  CHECK_STR(out, "http://h/gen");
  CHECK(urlWithoutParameter("http://h/gen?x=1&c=true&y=2#f", "c", out, sizeof out));
  CHECK_STR(out, "http://h/gen?x=1&y=2#f");
  CHECK(urlWithoutParameter("http://h/gen?c=true#f", "c", out, sizeof out));
  CHECK_STR(out, "http://h/gen#f");
  CHECK(urlWithoutParameter("http://h/gen#f?c=true", "c", out, sizeof out));
  CHECK_STR(out, "http://h/gen#f?c=true");
  CHECK(urlWithoutParameter("http://h/gen?cc=1&c", "c", out, sizeof out));
  CHECK_STR(out, "http://h/gen?cc=1");
  CHECK(urlWithoutParameter("http://h/gen?&c=true&&x=1&", "c", out, sizeof out));
  CHECK_STR(out, "http://h/gen?x=1");
  CHECK(urlWithoutParameter("http://h/gen", "c", out, sizeof out));
  CHECK_STR(out, "http://h/gen");
  CHECK(urlWithoutParameter("http://h/gen?working=true&c=true", "working", out, sizeof out));
  CHECK_STR(out, "http://h/gen?c=true");
  CHECK(!urlWithoutParameter("http://h/gen?x=1&c=true", "c", out, 16));
}

static void keepsEveryMistakeInOrder(void) {
  Mistakes list = {NULL, 0, 0};
  Mistake m;
  memset(&m, 0, sizeof m);
  for (long i = 1; i <= 20; i++) {
    m.number = i;
    m.chosen = (int)(i % 4);
    CHECK(mistakesAdd(&list, &m));
  }
  CHECK(list.count == 20);
  CHECK(list.items[0].number == 1);
  CHECK(list.items[19].number == 20);
  CHECK(list.items[6].chosen == 3);
  mistakesFree(&list);
  CHECK(list.items == NULL && list.count == 0 && list.room == 0);
}

static void keepsTheMistakesWhenTheListCannotGrow(void) {
  Mistakes list = {NULL, 0, 0};
  Mistake m;
  memset(&m, 0, sizeof m);
  m.number = 1;
  CHECK(mistakesAdd(&list, &m));
  m.number = 2;
  CHECK(mistakesAdd(&list, &m));
  /* A list at the most it could hold has no memory to grow into. */
  long most = (long)(SIZE_MAX / sizeof(Mistake) / 2);
  Mistakes full = {list.items, most, most};
  m.number = 3;
  CHECK(!mistakesAdd(&full, &m));
  CHECK(full.items == list.items && full.count == most);
  CHECK(list.items[0].number == 1 && list.items[1].number == 2);
  mistakesFree(&list);
}

static void knowsTheBidirectionalControls(void) {
  /* The controls are U+200E, U+200F, U+202A to U+202E and U+2066 to U+2069;
     the code points either side of those ranges are not controls. */
  CHECK(isBidiControl((const unsigned char*)"\xe2\x80\x8e"));
  CHECK(isBidiControl((const unsigned char*)"\xe2\x80\x8f"));
  CHECK(!isBidiControl((const unsigned char*)"\xe2\x80\x8d"));
  CHECK(!isBidiControl((const unsigned char*)"\xe2\x80\x90"));
  CHECK(!isBidiControl((const unsigned char*)"\xe2\x80\xa9"));
  CHECK(isBidiControl((const unsigned char*)"\xe2\x80\xaa"));
  CHECK(isBidiControl((const unsigned char*)"\xe2\x80\xae"));
  CHECK(!isBidiControl((const unsigned char*)"\xe2\x80\xaf"));
  CHECK(!isBidiControl((const unsigned char*)"\xe2\x81\xa5"));
  CHECK(isBidiControl((const unsigned char*)"\xe2\x81\xa6"));
  CHECK(isBidiControl((const unsigned char*)"\xe2\x81\xa9"));
  CHECK(!isBidiControl((const unsigned char*)"\xe2\x81\xaa"));
  CHECK(!isBidiControl((const unsigned char*)"\xe3\x80\xaa"));
}

static void refusesOverlongUTF8(void) {
  CHECK(utf8Length((const unsigned char*)"\xC2\x80") == 2);
  CHECK(utf8Length((const unsigned char*)"\xC0\x80") == 0);
  CHECK(utf8Length((const unsigned char*)"\xC1\xBF") == 0);
  CHECK(utf8Length((const unsigned char*)"\xE0\xA0\x80") == 3);
  CHECK(utf8Length((const unsigned char*)"\xE0\x9F\xBF") == 0);
  CHECK(utf8Length((const unsigned char*)"\xF0\x90\x80\x80") == 4);
  CHECK(utf8Length((const unsigned char*)"\xF0\x8F\xBF\xBF") == 0);
}

static void refusesUTF8PastTheCharacters(void) {
  CHECK(utf8Length((const unsigned char*)"\xED\x9F\xBF") == 3);
  CHECK(utf8Length((const unsigned char*)"\xED\xA0\x80") == 0);
  CHECK(utf8Length((const unsigned char*)"\xF4\x8F\xBF\xBF") == 4);
  CHECK(utf8Length((const unsigned char*)"\xF4\x90\x80\x80") == 0);
  CHECK(utf8Length((const unsigned char*)"\xF5\x80\x80\x80") == 0);
  CHECK(utf8Length((const unsigned char*)"\xFF\x80") == 0);
}

static void refusesUTF8WithBadContinuation(void) {
  CHECK(utf8Length((const unsigned char*)"\xE2\x80\x8E") == 3);
  CHECK(utf8Length((const unsigned char*)"\xE2\xC0\x80") == 0);
  CHECK(utf8Length((const unsigned char*)"\xE2\x80\xC0") == 0);
  CHECK(utf8Length((const unsigned char*)"\xF0\x9F\x98\x80") == 4);
  CHECK(utf8Length((const unsigned char*)"\xF0\x9F\x98\xC0") == 0);
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
  countsEveryOptionThatReadsAsTheAnswer();
  readsEveryQuestionInAReply();
  skipsALineAsLongAsTheBufferThatReadsIt();
  skipsWhatIsNotAQuestionInAReply();
  stopsAtTheRoomItWasGiven();
  addsCountToAnyURL();
  putsTheQueryBeforeAFragment();
  leavesTheURLAloneWithoutAQuery();
  saysSoWhenTheURLDoesNotFit();
  saysDurationsTheWayAPersonDoes();
  summarisesARun();
  namesTheSlowestQuestionOfALongEnoughRun();
  doesNotNameTheSlowestWhenThereIsNoneToName();
  keepsAMistakeOffAPerfectScore();
  doesNotRepeatItselfForOneAnswer();
  neverWritesPastTheRoomItWasGiven();
  knowsTheBidirectionalControls();
  refusesOverlongUTF8();
  refusesUTF8PastTheCharacters();
  refusesUTF8WithBadContinuation();
  survivesRandomInput();
  readsTheNumberOfQuestions();
  refusesWhatIsNotANumberOfQuestions();
  takesTheTimeLimitInEveryForm();
  countsTheSecondsLeftUpForACountdown();
  takesTheWholeRunsTimeInEveryForm();
  takesTheSeedInEveryForm();
  refusesASeedItCannotUse();
  takesTheLengthAndBrackets();
  refusesALengthItCannotUse();
  takesNoNegatives();
  takesAnAnswerRange();
  refusesRangeAboveMax();
  takesMaxAndOperators();
  refusesAMaxItCannotUse();
  refusesOperatorsItCannotUse();
  takesPractice();
  takesFeedback();
  takesPrint();
  refusesATimeLimitItCannotUse();
  refusesAWholeRunsTimeItCannotUse();
  saysWhenAnOptionHasNoValue();
  takesShortOptionsTogether();
  asksForHelpAmongShortOptions();
  refusesOptionsThatDoNotExist();
  asksForTheVersion();
  suggestsTheOptionThatWasMeant();
  refusesASecondNumberOfQuestions();
  helpWinsOverEverythingElse();
  aDoubleDashEndsTheOptions();
  takesWorking();
  readsTheTasksOfAWorkedAnswer();
  readsOneTaskAndTheOrderOfItsMembers();
  skipsATaskThatIsNotAQuestion();
  skipsWhatItDoesNotUse();
  dropsAWorkingThatIsNotPlainArithmetic();
  stopsAtWhatIsNotJSON();
  refusesDeepNesting();
  urlLosesAParameter();
  keepsEveryMistakeInOrder();
  keepsTheMistakesWhenTheListCannotGrow();

  if (failures) {
    fprintf(stderr, "%d of %d checks failed\n", failures, checks);
    return 1;
  }
  printf("%d checks passed\n", checks);
  return 0;
}
