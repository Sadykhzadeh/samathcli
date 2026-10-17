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

/* How many questions a run needs for its slowest to be worth naming, and how
   slow that has to be: with two questions, or none that took long, there is
   nothing to pick out. */
#define SLOWEST_MIN_ANSWERED 3
#define SLOWEST_MIN_SECONDS 2.0

/* The slowest question of a run as one line, "Task 4. 18/3-1 (21 s)": its
   `number` in the run, its `expression` and the `seconds` it took out of
   `answered` questions. Returns 0 when the run is too short or the
   question too quick to be worth a line, or when the line does not fit in
   `capacity`; `out` is then not to be used. */
int formatSlowest(long number, const char* expression, double seconds,
                  long answered, char* out, size_t capacity);

#endif
