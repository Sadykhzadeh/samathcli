#ifndef SAMATH_REPORT_H
#define SAMATH_REPORT_H

#include <stddef.h>

/* A span of time the way a person says it: "7.2 s", "46 s", "3 min 5 s",
   "1 h 2 min". Writes at most `capacity` bytes, the end included. */
void formatDuration(double seconds, char* out, size_t capacity);

/* One line about a finished run: "7 of 10 right (70%), 1 min 12 s, 7.2 s a
   question". `seconds` is the time spent answering, `answered` how many were
   answered and `right` how many of those were right. With one answer there
   is no per-question figure to add. */
void formatStats(long right, long answered, double seconds, char* out,
                 size_t capacity);

#endif
