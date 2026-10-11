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
   0 when `*text` holds the response (the caller frees it). curl is started
   directly rather than through a shell, so the URL is never read as anything
   but a URL. */
static int fetchText(const char* url, char** text) {
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
    if (silence >= 0) dup2(silence, STDIN_FILENO);
    dup2(channel[1], STDOUT_FILENO);
    close(channel[0]);
    close(channel[1]);
    execlp("curl", "curl", "-fsS", "--max-time", "10", "--url", url,
           (char*)NULL);
    static const char missing[] = "samath: could not run curl\n";
    ssize_t ignored = write(STDERR_FILENO, missing, sizeof missing - 1);
    (void)ignored;
    _exit(127);
  }
  close(channel[1]);

  size_t size = 0, capacity = 4096;
  char* body = malloc(capacity);
  int complete = body != NULL;
  while (complete) {
    if (size + 1 >= capacity) {
      if (capacity >= RESPONSE_MAX) {
        complete = 0;
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
  if (!complete || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    free(body);
    return WIFEXITED(status) && WEXITSTATUS(status) ? WEXITSTATUS(status) : -1;
  }
  body[size] = '\0';
  *text = body;
  return 0;
}

void quizOpen(Quiz* quiz, long total) {
  const char* url = getenv(apiUrlEnv);
  quiz->total = total;
  quiz->asked = 0;
  quiz->url = url && *url ? url : apiUrlDefault;
  quiz->batched = 1;
  quiz->size = 0;
  quiz->next = 0;
}

/* The URL that asks for `want` questions. One question is the plain URL, the
   request every server has always understood. */
static int requestUrl(const Quiz* quiz, int want, char* url, size_t capacity) {
  if (want <= 1) return urlWithQuery(quiz->url, "", url, capacity);
  char query[32];
  snprintf(query, sizeof query, "count=%d", want);
  return urlWithQuery(quiz->url, query, url, capacity);
}

/* Fetches the next batch into quiz->batch. */
static int loadBatch(Quiz* quiz) {
  long left = quiz->total - quiz->asked;
  int want = quiz->batched && left > 1
                 ? (int)(left < QUIZ_BATCH_MAX ? left : QUIZ_BATCH_MAX)
                 : 1;

  char url[URL_MAX];
  char* text = NULL;
  int status;
  for (;;) {
    if (!requestUrl(quiz, want, url, sizeof url)) return 0;
    status = fetchText(url, &text);
    /* A server that refuses `count` outright gets the plain request instead. */
    if (status == CURL_HTTP_ERROR && want > 1) {
      quiz->batched = 0;
      want = 1;
      continue;
    }
    break;
  }
  if (status) return 0;

  int got = parseQuestions(text, quiz->batch, want);
  free(text);
  if (got == 0) return 0;
  /* One question where several were asked for: this server ignores `count`. */
  if (got < want) quiz->batched = 0;
  quiz->size = got;
  quiz->next = 0;
  return 1;
}

int quizNext(Quiz* quiz, Question* out) {
  if (quiz->asked >= quiz->total) return 0;
  if (quiz->next >= quiz->size && !loadBatch(quiz)) return -1;
  *out = quiz->batch[quiz->next++];
  quiz->asked++;
  return 1;
}
