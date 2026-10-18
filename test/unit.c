#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../checkAnswer.h"
#include "../getQuestion.h"
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
  const char* bad[] = {"0",   "-3", "abc", "2.5", "", "5x",
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
  CHECK(args(&o, &culprit, 2, "5", "-t") == ARGS_BAD_TIME);
  CHECK_STR(culprit, "-t");
  CHECK(args(&o, &culprit, 2, "--time=", "5") == ARGS_BAD_TIME);
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
  CHECK(args(&o, &culprit, 2, "--practice=yes", "5") == ARGS_BAD_OPTION);
}

static void takesFeedback(void) {
  Options o;
  const char* culprit;
  CHECK(args(&o, &culprit, 1, "5") == ARGS_OK && o.feedback == 0);
  CHECK(args(&o, &culprit, 2, "-f", "5") == ARGS_OK && o.feedback == 1);
  CHECK(args(&o, &culprit, 2, "5", "--feedback") == ARGS_OK && o.feedback == 1);
  CHECK(args(&o, &culprit, 4, "-p", "-f", "-b", "5") == ARGS_OK);
  CHECK(o.feedback == 1 && o.practice == 1 && o.brackets == 1);
  CHECK(args(&o, &culprit, 2, "--feedback=yes", "5") == ARGS_BAD_OPTION);
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
  CHECK(args(&o, &culprit, 2, "--brackets=yes", "5") == ARGS_BAD_OPTION);
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
  CHECK(args(&o, &culprit, 2, "--no-negatives=yes", "5") == ARGS_BAD_OPTION);
  CHECK(args(&o, &culprit, 2, "--negatives", "5") == ARGS_BAD_OPTION);
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
  CHECK(args(&o, &culprit, 2, "5", "-l") == ARGS_BAD_LENGTH);
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
  CHECK(args(&o, &culprit, 2, "5", "-s") == ARGS_BAD_SEED);
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
  CHECK(args(&o, &culprit, 2, "5", "-pt") == ARGS_BAD_TIME);
  CHECK_STR(culprit, "-pt");
  CHECK(args(&o, &culprit, 2, "-px", "5") == ARGS_BAD_OPTION);
  CHECK_STR(culprit, "-x");
  CHECK(args(&o, &culprit, 2, "-xp", "5") == ARGS_BAD_OPTION);
  CHECK_STR(culprit, "-x");
  CHECK(args(&o, &culprit, 2, "-pé", "5") == ARGS_BAD_OPTION);
  CHECK_STR(culprit, "-pé");
  /* Long options are not joined, and take a value only if they take one. */
  CHECK(args(&o, &culprit, 2, "--practice=1", "5") == ARGS_BAD_OPTION);
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
  CHECK(args(&o, &culprit, 1, "--help=1") == ARGS_BAD_OPTION);
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
  readsEveryQuestionInAReply();
  skipsWhatIsNotAQuestionInAReply();
  stopsAtTheRoomItWasGiven();
  addsCountToAnyURL();
  leavesTheURLAloneWithoutAQuery();
  saysSoWhenTheURLDoesNotFit();
  saysDurationsTheWayAPersonDoes();
  summarisesARun();
  namesTheSlowestQuestionOfALongEnoughRun();
  doesNotNameTheSlowestWhenThereIsNoneToName();
  keepsAMistakeOffAPerfectScore();
  doesNotRepeatItselfForOneAnswer();
  neverWritesPastTheRoomItWasGiven();
  survivesRandomInput();
  readsTheNumberOfQuestions();
  refusesWhatIsNotANumberOfQuestions();
  takesTheTimeLimitInEveryForm();
  takesTheSeedInEveryForm();
  refusesASeedItCannotUse();
  takesTheLengthAndBrackets();
  refusesALengthItCannotUse();
  takesNoNegatives();
  takesPractice();
  takesFeedback();
  refusesATimeLimitItCannotUse();
  takesShortOptionsTogether();
  asksForHelpAmongShortOptions();
  refusesOptionsThatDoNotExist();
  refusesASecondNumberOfQuestions();
  helpWinsOverEverythingElse();
  aDoubleDashEndsTheOptions();
  keepsEveryMistakeInOrder();
  keepsTheMistakesWhenTheListCannotGrow();

  if (failures) {
    fprintf(stderr, "%d of %d checks failed\n", failures, checks);
    return 1;
  }
  printf("%d checks passed\n", checks);
  return 0;
}
