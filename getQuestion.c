#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "getQuestion.h"

#define apiUrlEnv "SAMATH_API_URL"
#define apiUrlDefault "http://localhost:3000/gen?c=true"

/* A run of 100 questions is a few kilobytes; a server that sends more than
   this is not sending questions. */
#define RESPONSE_MAX (1 << 20)
#define URL_MAX 2048

/* curl's exit status for an HTTP error, which -f turns a 4xx or 5xx into. */
#define CURL_HTTP_ERROR 22
/* What the child exits with when there is no curl to run, as a shell does. */
#define CURL_NOT_RUN 127
/* curl's exit status for a request that ran out of time. */
#define CURL_TIMED_OUT 28
/* What fetchText gives back for a response bigger than RESPONSE_MAX, and for
   one with a NUL byte in it, which is not text. */
#define FETCH_TOO_BIG (-2)
#define FETCH_NOT_TEXT (-3)

int urlWithQuery(const char* url, const char* query, char* out,
                 size_t capacity) {
  size_t length = strlen(url);
  const char* separator = "?";
  if (!*query || (length && (url[length - 1] == '?' || url[length - 1] == '&')))
    separator = "";
  else if (strchr(url, '?'))
    separator = "&";
  int written = snprintf(out, capacity, "%s%s%s", url, separator, query);
  return written >= 0 && (size_t)written < capacity;
}

/* Runs curl on `url` and collects what it prints. Returns curl's exit status,
   FETCH_TOO_BIG, FETCH_NOT_TEXT, or 0 when `*text` holds the response (the
   caller frees it). `*code` is the HTTP status curl got, 0 when it got none.
   curl is started directly rather than through a shell, so the URL is never
   read as anything but a URL. */
static int fetchText(const char* url, char** text, long* code) {
  *code = 0;
  int channel[2];
  if (pipe(channel)) return -1;

  pid_t child = fork();
  if (child < 0) {
    close(channel[0]);
    close(channel[1]);
    return -1;
  }
  if (child == 0) {
    int silence = open("/dev/null", O_RDONLY);
    if (silence >= 0 && silence != STDIN_FILENO) {
      dup2(silence, STDIN_FILENO);
      close(silence);
    }
    dup2(channel[1], STDOUT_FILENO);
    close(channel[0]);
    close(channel[1]);
    /* A host that does not answer is given up on after 4 s, a slow answer after
       10. A failure that may pass (a 5xx, a timeout, a dropped connection) is
       tried once more, but not past 8 s in all, so a dead host still ends the
       run within about ten. Anything else, such as a 4xx or a refused
       connection, fails at once. */
    execlp("curl", "curl", "-sS", "--connect-timeout", "4", "--max-time", "10",
           "--retry", "1", "--retry-max-time", "8", "-w", "%{http_code}",
           "--url", url, (char*)NULL);
    _exit(CURL_NOT_RUN);
  }
  close(channel[1]);

  size_t size = 0, capacity = 4096;
  char* body = malloc(capacity);
  int complete = body != NULL, tooBig = 0;
  while (complete) {
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
    ssize_t got = read(channel[0], body + size, capacity - size - 1);
    if (got < 0 && errno == EINTR) continue;
    if (got <= 0) break;
    size += (size_t)got;
  }
  close(channel[0]);

  int status = 0;
  while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
  }
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

/* Keeps what a refusing server said, its first line, for the person to read:
   without the control characters that would move the cursor, and not past
   REFUSAL_MAX bytes, or part of a UTF-8 character. */
static void rememberRefusal(Quiz* quiz, const char* body) {
  size_t length = 0;
  for (const char* c = body; *c && *c != '\n'; c++) {
    if (length + 1 >= REFUSAL_MAX) {
      if (((unsigned char)*c & 0xC0) == 0x80) {
        while (length && ((unsigned char)quiz->said[length - 1] & 0xC0) == 0x80)
          length--;
        if (length) length--;
      }
      break;
    }
    if ((unsigned char)*c >= ' ' && *c != 0x7f) quiz->said[length++] = *c;
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

/* The URL that asks for `want` questions. One question with nothing else
   asked for is the plain URL, the request every server has always
   understood. With a seed, the question the request starts at is the seed
   plus the questions already handed out, so a run is the same questions
   however they were batched. */
static int requestUrl(const Quiz* quiz, int want, char* url, size_t capacity) {
  char query[160] = "";
  size_t used = 0;
  if (want > 1) used = (size_t)snprintf(query, sizeof query, "count=%d", want);
  if (quiz->seed >= 0)
    used += (size_t)snprintf(query + used, sizeof query - used, "%sseed=%lld",
                             used ? "&" : "",
                             (quiz->seed + quiz->asked) % (SEED_MAX + 1));
  if (quiz->length)
    used += (size_t)snprintf(query + used, sizeof query - used, "%slength=%d",
                             used ? "&" : "", quiz->length);
  if (quiz->brackets)
    used += (size_t)snprintf(query + used, sizeof query - used,
                             "%sbrackets=true", used ? "&" : "");
  if (quiz->max)
    used += (size_t)snprintf(query + used, sizeof query - used, "%smax=%d",
                             used ? "&" : "", quiz->max);
  if (*quiz->operators)
    used += (size_t)snprintf(query + used, sizeof query - used,
                             "%soperators=%s", used ? "&" : "",
                             quiz->operators);
  if (quiz->noNegatives)
    snprintf(query + used, sizeof query - used, "%snegatives=false",
             used ? "&" : "");
  return urlWithQuery(quiz->url, query, url, capacity);
}

/* Fetches the next batch into quiz->batch. Returns 0, or a QUIZ_ code.

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
    if (!requestUrl(quiz, want, url, sizeof url)) return QUIZ_UNREACHABLE;
    status = fetchText(url, &text, &code);
    if (status == CURL_HTTP_ERROR) {
      rememberRefusal(quiz, text);
      free(text);
    }
    if (status != CURL_HTTP_ERROR || want == 1) break;
    /* A 4xx is a server refusing `count`, which is not asked again: plain
       requests from now on. A 5xx, 408 or 429 may pass, so the batch is
       tried once more before it is given up on, and batching stays on. */
    if (code >= 400 && code < 500 && code != 408 && code != 429) {
      quiz->batched = 0;
      want = 1;
    } else if (retried) {
      break;
    } else {
      retried = 1;
    }
  }
  if (status == FETCH_TOO_BIG || status == FETCH_NOT_TEXT)
    return QUIZ_NOT_QUESTIONS;
  if (status == CURL_NOT_RUN) return QUIZ_NO_CURL;
  if (status == CURL_TIMED_OUT) return QUIZ_TIMED_OUT;
  if (status == CURL_HTTP_ERROR) return QUIZ_REFUSED;
  if (status) return QUIZ_UNREACHABLE;

  int got = parseQuestions(text, quiz->batch, want);
  free(text);
  if (got == 0) return QUIZ_NOT_QUESTIONS;
  /* One question where several were asked for: this server ignores `count`. */
  if (got < want) quiz->batched = 0;
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
