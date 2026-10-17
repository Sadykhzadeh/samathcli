#include <stdio.h>
#include "report.h"

void formatDuration(double seconds, char* out, size_t capacity) {
  if (seconds < 0) seconds = 0;
  if (seconds < 9.95) {
    snprintf(out, capacity, "%.1f s", seconds);
    return;
  }
  long total = (long)(seconds + 0.5);
  if (total < 60)
    snprintf(out, capacity, "%ld s", total);
  else if (total < 3600)
    snprintf(out, capacity, "%ld min %ld s", total / 60, total % 60);
  else
    snprintf(out, capacity, "%ld h %ld min", total / 3600, total % 3600 / 60);
}

void formatStats(long right, long answered, double seconds, char* out,
                 size_t capacity) {
  long percent = answered ? (right * 100 + answered / 2) / answered : 0;
  /* Rounded, but never to a perfect score with a mistake in it, nor to none
     with a right answer. */
  if (percent == 100 && right < answered) percent = 99;
  if (percent == 0 && right > 0) percent = 1;
  char total[32];
  formatDuration(seconds, total, sizeof total);
  int used = snprintf(out, capacity, "%ld of %ld right (%ld%%), %s", right,
                      answered, percent, total);
  if (answered > 1 && used > 0 && (size_t)used < capacity) {
    char each[32];
    formatDuration(seconds / (double)answered, each, sizeof each);
    snprintf(out + used, capacity - (size_t)used, ", %s a question", each);
  }
}

int formatSlowest(long number, const char* expression, double seconds,
                  long answered, char* out, size_t capacity) {
  if (answered < SLOWEST_MIN_ANSWERED || seconds < SLOWEST_MIN_SECONDS)
    return 0;
  char took[32];
  formatDuration(seconds, took, sizeof took);
  int written =
      snprintf(out, capacity, "Task %ld. %s (%s)", number, expression, took);
  return written >= 0 && (size_t)written < capacity;
}
