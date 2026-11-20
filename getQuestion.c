#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <unistd.h>
#include "getQuestion.h"
#include "input.h"

#define apiUrlEnv "SAMATH_API_URL"
#define apiUrlDefault "http://localhost:3000/gen?c=true"

/* A run of 100 questions is a few kilobytes; a server that sends more than
   this is not sending questions. */
#define RESPONSE_MAX (1 << 20)
#define URL_MAX 2048
/* Room for every parameter requestUrl can add, each at its longest. */
#define QUERY_MAX 256

/* The status fetchText gives for a 4xx or 5xx answer, whose body it returns.
   curl only exits with 22 for that under -f, which is not used. */
#define CURL_HTTP_ERROR 22
/* What the child exits with when there is no curl to run, as a shell does. */
#define CURL_NOT_RUN 127
/* curl's exit status for a request that ran out of time. */
#define CURL_TIMED_OUT 28
/* curl's exit statuses for a URL it cannot fetch from: a scheme it does not
   know or may not follow, a URL it cannot parse, and a run of redirects past
   --max-redirs. Asking again does not change any of them. */
#define CURL_UNSUPPORTED_PROTOCOL 1
#define CURL_BAD_URL 3
#define CURL_COULDNT_RESOLVE_HOST 6
/* curl's exit status for a proxy, as http_proxy names it, whose host does not
   resolve. */
#define CURL_COULDNT_RESOLVE_PROXY 5
/* curl's exit status for an option it does not know, which curl before 7.21
   gives for --proto and --proto-redir. */
#define CURL_FAILED_INIT 2
#define CURL_TOO_MANY_REDIRECTS 47
/* curl's exit statuses for a TLS handshake that failed, and for a certificate
   that could not be checked. */
#define CURL_SSL_CONNECT 35
#define CURL_PEER_VERIFY 51
#define CURL_CA_CHECK 60
#define CURL_ISSUER_CHECK 83
/* curl's exit statuses for a connection that was dropped: before all of the
   answer had come, before any of it had, and while sending or receiving. */
#define CURL_CUT_SHORT 18
#define CURL_NO_REPLY 52
#define CURL_SEND_FAILED 55
#define CURL_RECEIVE_FAILED 56
/* What fetchText gives back for a response bigger than RESPONSE_MAX, and for
   one with a NUL byte in it, which is not text. */
#define FETCH_TOO_BIG (-2)
#define FETCH_NOT_TEXT (-3)
/* What fetchText gives back when there is no pipe to read curl through, or no
   process to run it in: the system is out of file descriptors or processes. */
#define FETCH_NO_PROCESS (-4)

/* Whether `url` is an http or https address, the only kind curl is asked to
   fetch from, and so the kind a redirect may be followed to. */
static int isWebUrl(const char* url) {
  return !strncasecmp(url, "http://", 7) || !strncasecmp(url, "https://", 8);
}

/* The query goes before a #fragment, which curl never sends: after it, the
   query would be lost. */
int urlWithQuery(const char* url, const char* query, char* out,
                 size_t capacity) {
  size_t length = strcspn(url, "#");
  const char* separator = "?";
  if (!*query || (length && (url[length - 1] == '?' || url[length - 1] == '&')))
    separator = "";
  else if (memchr(url, '?', length))
    separator = "&";
  int written = snprintf(out, capacity, "%.*s%s%s", (int)length, url, separator,
                         query);
  return written >= 0 && (size_t)written < capacity;
}

/* Runs curl on `url` and collects what it prints. Returns curl's exit status,
   FETCH_TOO_BIG, FETCH_NOT_TEXT, FETCH_NO_PROCESS, or 0 when `*text` holds the
   response (the caller frees it). `*code` is the HTTP status curl got, 0 when
   it got none. curl is started directly rather than through a shell, so the
   URL is never read as anything but a URL. */
static int fetchText(const char* url, char** text, long* code) {
  *code = 0;
  *text = NULL;
  int channel[2];
  if (pipe(channel)) return FETCH_NO_PROCESS;

  pid_t child = fork();
  if (child < 0) {
    close(channel[0]);
    close(channel[1]);
    return FETCH_NO_PROCESS;
  }
  if (child == 0) {
    /* With standard output closed, pipe() may have given it the read end, and
       the write end can be the standard input or error. Both ends are moved
       out of the way before the standard descriptors are set up. */
    int out = fcntl(channel[1], F_DUPFD, STDERR_FILENO + 1);
    close(channel[0]);
    close(channel[1]);
    int silence = open("/dev/null", O_RDONLY);
    if (silence >= 0 && silence != STDIN_FILENO) {
      dup2(silence, STDIN_FILENO);
      close(silence);
    }
    /* curl's own complaints are not shown: the caller says what went wrong,
       in its own words, and a stopped read reads as a write failure to curl. */
    int hush = open("/dev/null", O_WRONLY);
    if (hush >= 0 && hush != STDERR_FILENO) {
      dup2(hush, STDERR_FILENO);
      close(hush);
    }
    dup2(out, STDOUT_FILENO);
    close(out);
    /* -q has to come first: it keeps curl from reading the user's curl
       configuration, whose `request`, `output` or `data` would change what is
       asked for. A host that does not answer is given up on after 4 s, a slow
       answer after 10. curl is told not to try again by itself: into a pipe it
       prints the body of the attempt that failed and then that of the next
       one, with nothing to tell them apart. loadBatch does the trying again. A
       URL that has moved, as http:// does when a server wants https://, is
       followed a few times, to another web address and nowhere else. Only web
       addresses are asked of at all: a file or any other scheme is refused. */
    execlp("curl", "curl", "-q", "-sS", "--connect-timeout", "4", "--max-time", "10",
           "--retry", "0", "-L", "--max-redirs", "5", "--proto", "=http,https",
           "--proto-redir", "=http,https", "-w", "%{http_code}", "--url", url,
           (char*)NULL);
    _exit(CURL_NOT_RUN);
  }
  close(channel[1]);
  inputChild(child);

  size_t size = 0, capacity = 4096;
  char* body = malloc(capacity);
  int complete = body != NULL, tooBig = 0;
  while (complete) {
    /* Ctrl-C ends the wait for the answer, as it ends the wait for a key. */
    if (inputInterrupted()) {
      complete = 0;
      break;
    }
    if (size + 1 >= capacity) {
      if (capacity >= RESPONSE_MAX) {
        complete = 0;
        tooBig = 1;
        break;
      }
      char* grown = realloc(body, capacity * 2);
      if (!grown) {
        complete = 0;
        break;
      }
      body = grown;
      capacity *= 2;
    }
    /* Waiting in short slices lets a Ctrl-C that came in before the read
       is reached end the wait, not only one that interrupts it. */
    struct pollfd watch = {channel[0], POLLIN, 0};
    int ready = poll(&watch, 1, 100);
    if (ready < 0 && errno != EINTR) break;
    if (ready <= 0) continue;
    ssize_t got = read(channel[0], body + size, capacity - size - 1);
    if (got < 0 && errno == EINTR) continue;
    if (got <= 0) break;
    size += (size_t)got;
  }
  close(channel[0]);
  /* What is left of the answer is not wanted, so curl is not waited for: it
     may be stalled on a server that has nothing more to send. */
  if (!complete) kill(child, SIGTERM);

  int status = 0;
  while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
  }
  inputChild(0);
  if (tooBig) {
    free(body);
    return FETCH_TOO_BIG;
  }
  /* curl prints the HTTP status after whatever it sent, always three digits,
     and sends no body at all when it fails with one. */
  if (complete && size >= 3) {
    body[size] = '\0';
    *code = strtol(body + size - 3, NULL, 10);
    size -= 3;
  }
  if (!complete || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    free(body);
    return WIFEXITED(status) && WEXITSTATUS(status) ? WEXITSTATUS(status) : -1;
  }
  body[size] = '\0';
  if (*code >= 400) {
    *text = body;
    return CURL_HTTP_ERROR;
  }
  if (memchr(body, '\0', size)) {
    free(body);
    return FETCH_NOT_TEXT;
  }
  *text = body;
  return 0;
}

/* The length of the well-formed UTF-8 character that starts at `text`, or 0
   when the bytes there are not one. Overlong forms, surrogates and code points
   past U+10FFFF are not well formed. */
static size_t utf8Length(const unsigned char* text) {
  unsigned char low = 0x80, high = 0xBF;
  size_t length;
  if (text[0] >= 0xC2 && text[0] <= 0xDF) {
    length = 2;
  } else if (text[0] >= 0xE0 && text[0] <= 0xEF) {
    length = 3;
    if (text[0] == 0xE0) low = 0xA0;
    if (text[0] == 0xED) high = 0x9F;
  } else if (text[0] >= 0xF0 && text[0] <= 0xF4) {
    length = 4;
    if (text[0] == 0xF0) low = 0x90;
    if (text[0] == 0xF4) high = 0x8F;
  } else {
    return 0;
  }
  if (text[1] < low || text[1] > high) return 0;
  for (size_t i = 2; i < length; i++)
    if (text[i] < 0x80 || text[i] > 0xBF) return 0;
  return length;
}

/* Whether the well-formed 3-byte character at `text` is a bidirectional
   control: U+200E, U+200F, U+202A to U+202E or U+2066 to U+2069. A terminal
   that shows them reorders the text around them. */
static int isBidiControl(const unsigned char* text) {
  if (text[0] != 0xE2) return 0;
  if (text[1] == 0x80) return text[2] == 0x8E || text[2] == 0x8F || (text[2] >= 0xAA && text[2] <= 0xAE);
  if (text[1] == 0x81) return text[2] >= 0xA6 && text[2] <= 0xA9;
  return 0;
}

/* Keeps what a refusing server said, its first line, for the person to read:
   valid UTF-8 only, with the control characters that would move the cursor,
   the C1 controls and the bidirectional controls dropped, a '?' for each byte
   that is no part of a character, and not past REFUSAL_MAX bytes or part of a
   character. */
static void rememberRefusal(Quiz* quiz, const char* body) {
  const unsigned char* c = (const unsigned char*)body;
  size_t length = 0;
  while (*c && *c != '\n') {
    size_t size = *c < 0x80 ? 1 : utf8Length(c);
    if (size == 2 && c[0] == 0xC2 && c[1] <= 0x9F) {
      c += 2;
      continue;
    }
    if (size == 3 && isBidiControl(c)) {
      c += 3;
      continue;
    }
    if (size == 1 && (*c < ' ' || *c == 0x7f)) {
      c++;
      continue;
    }
    if (!size) {
      if (length + 1 >= REFUSAL_MAX) break;
      quiz->said[length++] = '?';
      c++;
      continue;
    }
    if (length + size >= REFUSAL_MAX) break;
    memcpy(quiz->said + length, c, size);
    length += size;
    c += size;
  }
  while (length && quiz->said[length - 1] == ' ') length--;
  quiz->said[length] = '\0';
}

void quizOpen(Quiz* quiz, const Options* options) {
  const char* url = getenv(apiUrlEnv);
  quiz->total = options->questions;
  quiz->seed = options->seed;
  quiz->length = options->length;
  quiz->brackets = options->brackets;
  quiz->noNegatives = options->noNegatives;
  quiz->max = options->max;
  strcpy(quiz->operators, options->operators);
  quiz->ranged = options->ranged;
  quiz->rangeLow = options->rangeLow;
  quiz->rangeHigh = options->rangeHigh;
  quiz->asked = 0;
  quiz->url = url && *url ? url : apiUrlDefault;
  quiz->batched = 1;
  quiz->size = 0;
  quiz->next = 0;
  quiz->seen = NULL;
  quiz->seenRoom = 0;
  quiz->seenCount = 0;
  quiz->said[0] = '\0';
}

void quizClose(Quiz* quiz) {
  free(quiz->seen);
  quiz->seen = NULL;
  quiz->seenRoom = 0;
  quiz->seenCount = 0;
}

/* FNV-1a, never 0, which marks a free slot. */
static unsigned long long hashText(const char* text) {
  unsigned long long hash = 14695981039346656037ULL;
  for (; *text; text++) {
    hash ^= (unsigned char)*text;
    hash *= 1099511628211ULL;
  }
  return hash ? hash : 1;
}

/* The slot that holds `hash`, or the free one where it would go. */
static size_t slotFor(const unsigned long long* slots, size_t room,
                      unsigned long long hash) {
  size_t i = (size_t)hash & (room - 1);
  while (slots[i] && slots[i] != hash) i = (i + 1) & (room - 1);
  return i;
}

/* Adds an expression to those seen. Returns 0 when it was seen before, and
   1 when it is new or there is no memory left to tell. */
static int remember(Quiz* quiz, const char* expression) {
  if (2 * (quiz->seenCount + 1) > quiz->seenRoom) {
    size_t room = quiz->seenRoom ? quiz->seenRoom * 2 : 256;
    unsigned long long* grown = calloc(room, sizeof *grown);
    if (!grown) return 1;
    for (size_t i = 0; i < quiz->seenRoom; i++)
      if (quiz->seen[i])
        grown[slotFor(grown, room, quiz->seen[i])] = quiz->seen[i];
    free(quiz->seen);
    quiz->seen = grown;
    quiz->seenRoom = room;
  }
  unsigned long long hash = hashText(expression);
  size_t slot = slotFor(quiz->seen, quiz->seenRoom, hash);
  if (quiz->seen[slot]) return 0;
  quiz->seen[slot] = hash;
  quiz->seenCount++;
  return 1;
}

/* Appends what `format` gives to `text`, which holds `*used` bytes, and counts
   them into `*used`. Returns 0, and leaves `*used` alone, when it does not fit
   in `capacity`. */
static int appendf(char* text, size_t capacity, size_t* used,
                   const char* format, ...) {
  va_list args;
  va_start(args, format);
  int written = vsnprintf(text + *used, capacity - *used, format, args);
  va_end(args);
  if (written < 0 || (size_t)written >= capacity - *used) return 0;
  *used += (size_t)written;
  return 1;
}

/* The URL that asks for `want` questions. One question with nothing else
   asked for is the plain URL, the request every server has always
   understood. With a seed, the question the request starts at is the seed
   plus the questions already handed out, so a run is the same questions
   however they were batched. */
static int requestUrl(const Quiz* quiz, int want, char* url, size_t capacity) {
  char query[QUERY_MAX];
  size_t used = 0;
  query[0] = '\0';
  if (want > 1 && !appendf(query, sizeof query, &used, "count=%d", want))
    return 0;
  if (quiz->seed >= 0 &&
      !appendf(query, sizeof query, &used, "%sseed=%lld", used ? "&" : "",
               (quiz->seed + quiz->asked) % (SEED_MAX + 1)))
    return 0;
  if (quiz->length &&
      !appendf(query, sizeof query, &used, "%slength=%d", used ? "&" : "",
               quiz->length))
    return 0;
  if (quiz->brackets &&
      !appendf(query, sizeof query, &used, "%sbrackets=%s", used ? "&" : "",
               quiz->brackets == BRACKETS_NEVER ? "never" : "true"))
    return 0;
  if (quiz->max &&
      !appendf(query, sizeof query, &used, "%smax=%d", used ? "&" : "",
               quiz->max))
    return 0;
  if (*quiz->operators &&
      !appendf(query, sizeof query, &used, "%soperators=%s", used ? "&" : "",
               quiz->operators))
    return 0;
  if (quiz->ranged &&
      !appendf(query, sizeof query, &used, "%sanswerMin=%d&answerMax=%d",
               used ? "&" : "", quiz->rangeLow, quiz->rangeHigh))
    return 0;
  if (quiz->noNegatives &&
      !appendf(query, sizeof query, &used, "%snegatives=%s",
               used ? "&" : "",
               quiz->noNegatives == NEGATIVES_ASK ? "true" : "false"))
    return 0;
  return urlWithQuery(quiz->url, query, url, capacity);
}

/* Whether a request that failed with `status` and HTTP status `code` may pass
   if it is made again: a 5xx, 408 or 429, a timeout, a dropped connection.
   Anything else, such as another 4xx or a refused connection, will not. */
static int mayPass(int status, long code) {
  if (status == CURL_HTTP_ERROR)
    return code < 400 || code >= 500 || code == 408 || code == 429;
  return status == CURL_TIMED_OUT || status == CURL_CUT_SHORT ||
         status == CURL_NO_REPLY || status == CURL_SEND_FAILED ||
         status == CURL_RECEIVE_FAILED;
}

/* Fetches the next batch into quiz->batch. Returns 0, or a QUIZ_ code.

   A request that fails in a way that may pass is made once more, a second
   later, before the batch is given up on: with 4 s to connect and 10 to
   answer, a dead host ends the run within about ten seconds and a slow one
   within about twenty.

   After the first batch of a run without a seed, a full batch is asked for
   however few questions are left: some can be ones the run has had, and
   those that are not needed are never handed out. A batch with nothing new
   in it is all the server has, and is handed out as it is. */
static int loadBatch(Quiz* quiz) {
  long left = quiz->total - quiz->asked;
  if (quiz->seed < 0 && quiz->asked) left = QUIZ_BATCH_MAX;
  int want = quiz->batched && left > 1
                 ? (int)(left < QUIZ_BATCH_MAX ? left : QUIZ_BATCH_MAX)
                 : 1;

  char url[URL_MAX];
  char* text = NULL;
  int status;
  long code = 0;
  int retried = 0;
  for (;;) {
    if (!requestUrl(quiz, want, url, sizeof url)) return QUIZ_BAD_URL;
    status = fetchText(url, &text, &code);
    /* A refusal comes with the body the server sent, which says why. */
    if (status == CURL_HTTP_ERROR && text) {
      rememberRefusal(quiz, text);
      free(text);
    }
    if (mayPass(status, code)) {
      /* The same request again, so batching stays on. A signal that ends the
         wait is the person ending the run. */
      if (retried || sleep(1)) break;
      retried = 1;
    } else if (status == CURL_HTTP_ERROR && want > 1) {
      /* A 4xx is a server refusing `count`, which is not asked again: plain
         requests from now on. */
      quiz->batched = 0;
      want = 1;
    } else {
      break;
    }
  }
  if (status == FETCH_TOO_BIG) return QUIZ_TOO_BIG;
  if (status == FETCH_NOT_TEXT) return QUIZ_NOT_QUESTIONS;
  if (status == FETCH_NO_PROCESS) return QUIZ_NO_PROCESS;
  if (status == CURL_NOT_RUN) return QUIZ_NO_CURL;
  if (status == CURL_TIMED_OUT) return QUIZ_TIMED_OUT;
  if (status == CURL_UNSUPPORTED_PROTOCOL && isWebUrl(quiz->url))
    return QUIZ_REDIRECTED_AWAY;
  if (status == CURL_UNSUPPORTED_PROTOCOL || status == CURL_BAD_URL)
    return QUIZ_BAD_URL;
  if (status == CURL_TOO_MANY_REDIRECTS) return QUIZ_TOO_MANY_REDIRECTS;
  if (status == CURL_COULDNT_RESOLVE_PROXY) return QUIZ_NO_PROXY;
  if (status == CURL_FAILED_INIT) return QUIZ_CURL_FAILED;
  if (status == CURL_COULDNT_RESOLVE_HOST) return QUIZ_NO_HOST;
  if (status == CURL_SSL_CONNECT || status == CURL_PEER_VERIFY ||
      status == CURL_CA_CHECK || status == CURL_ISSUER_CHECK)
    return QUIZ_NO_TLS;
  if (status == CURL_HTTP_ERROR) return QUIZ_REFUSED;
  if (status == CURL_CUT_SHORT) return QUIZ_CUT_SHORT;
  if (status) return QUIZ_UNREACHABLE;

  int got = parseQuestions(text, quiz->batch, want);
  free(text);
  if (got == 0) return QUIZ_NOT_QUESTIONS;
  /* A server that ignores `count` sends one question for a request that asks
     for several, from its first answer on. A batch that comes back with one
     question later on is kept, as a server can run out of new questions. */
  if (got == 1 && want > 1 && !quiz->asked) quiz->batched = 0;
  if (quiz->seed < 0) {
    int fresh = 0;
    for (int i = 0; i < got; i++)
      if (remember(quiz, quiz->batch[i].expression))
        quiz->batch[fresh++] = quiz->batch[i];
    if (fresh) got = fresh;
  }
  quiz->size = got;
  quiz->next = 0;
  return 0;
}

int quizNext(Quiz* quiz, Question* out) {
  if (quiz->asked >= quiz->total) return 0;
  if (quiz->next >= quiz->size) {
    int failure = loadBatch(quiz);
    if (failure) return failure;
  }
  *out = quiz->batch[quiz->next++];
  quiz->asked++;
  return 1;
}
