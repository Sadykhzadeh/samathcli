#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include "input.h"

#define CTRL_D 4
#define ESCAPE 27
/* How long the rest of an escape sequence is waited for. A terminal sends it
   in one piece, so this only has to outlast the wire. */
#define SEQUENCE_WAIT_MS 30
/* How long after a question is shown a key does not answer it. Nobody reads
   a sum this fast, but a key pressed twice comes this soon. */
#define SETTLE_SECONDS 0.25

/* What readByte gives besides a byte. */
#define BYTE_END (-1)
#define BYTE_NONE (-2)

static struct termios original;
static volatile sig_atomic_t taken = 0;
static double settled = 0; /* when keys start to answer the question */

static void giveBack(void) {
  if (taken) {
    taken = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &original);
  }
}

/* Leaves the terminal as it was, then dies of the same signal as it would
   have. Only calls that are safe inside a handler. */
static void onSignal(int signal) {
  giveBack();
  struct sigaction fallback;
  fallback.sa_handler = SIG_DFL;
  sigemptyset(&fallback.sa_mask);
  fallback.sa_flags = 0;
  sigaction(signal, &fallback, NULL);
  raise(signal);
}

double monotonicSeconds(void) {
  struct timespec time;
  clock_gettime(CLOCK_MONOTONIC, &time);
  return (double)time.tv_sec + (double)time.tv_nsec / 1e9;
}

void inputStart(void) {
  if (taken || !isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &original))
    return;

  struct termios keys = original;
  keys.c_lflag &= (tcflag_t) ~(ICANON | ECHO);
  /* Ctrl-S would freeze the screen while keys still answered questions that
     could not be seen; it is one more key to ignore. */
  keys.c_iflag &= (tcflag_t) ~IXON;
  keys.c_cc[VMIN] = 1;
  keys.c_cc[VTIME] = 0;
#if defined(VSUSP) && defined(_POSIX_VDISABLE)
  /* A suspended run would leave the terminal as it is now. */
  keys.c_cc[VSUSP] = _POSIX_VDISABLE;
#endif
  if (tcsetattr(STDIN_FILENO, TCSANOW, &keys)) return;
  taken = 1;
  atexit(giveBack);

  struct sigaction handler;
  handler.sa_handler = onSignal;
  sigemptyset(&handler.sa_mask);
  handler.sa_flags = 0;
  sigaction(SIGINT, &handler, NULL);
  sigaction(SIGTERM, &handler, NULL);
  sigaction(SIGHUP, &handler, NULL);
  sigaction(SIGQUIT, &handler, NULL);
  /* Output piped into something that exits first, as in `samath 10 | head`. */
  sigaction(SIGPIPE, &handler, NULL);
}

void inputDiscard(void) {
  if (!taken) return;
  tcflush(STDIN_FILENO, TCIFLUSH);
  settled = monotonicSeconds() + SETTLE_SECONDS;
}

/* One byte of input, BYTE_END at the end of it, or, when `waitMs` is not
   negative and nothing came in that long, BYTE_NONE. */
static int readByte(int waitMs) {
  if (waitMs >= 0) {
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(STDIN_FILENO, &readable);
    struct timeval timeout = {waitMs / 1000, (waitMs % 1000) * 1000};
    int ready = select(STDIN_FILENO + 1, &readable, NULL, NULL, &timeout);
    if (ready <= 0) return BYTE_NONE;
  }
  unsigned char byte;
  for (;;) {
    ssize_t got = read(STDIN_FILENO, &byte, 1);
    if (got == 1) return byte;
    if (got < 0 && errno == EINTR) continue;
    return BYTE_END;
  }
}

/* The rest of a key such as an arrow, which arrives as ESC [ A or ESC O A. */
static void skipEscapeSequence(void) {
  int next = readByte(SEQUENCE_WAIT_MS);
  if (next != '[' && next != 'O') return;
  while ((next = readByte(SEQUENCE_WAIT_MS)) >= 0)
    if (next >= 0x40 && next <= 0x7e) return;
}

int readChoice(int seconds, void (*tick)(int left)) {
  double deadline = monotonicSeconds() + seconds;
  int shown = seconds;

  for (;;) {
    int waitMs = -1;
    if (seconds > 0) {
      double remaining = deadline - monotonicSeconds();
      if (remaining <= 0) return CHOICE_TIMEOUT;
      /* Whole seconds still to go, counting the one running. */
      int left = (int)remaining;
      if ((double)left < remaining) left++;
      if (left != shown) {
        shown = left;
        if (tick) tick(left);
      }
      waitMs = (int)((remaining - (double)(left - 1)) * 1000) + 1;
    }

    int key = readByte(waitMs);
    if (key == BYTE_NONE) continue;
    if (key == BYTE_END || key == CTRL_D) return CHOICE_QUIT;
    if (key == ESCAPE) {
      skipEscapeSequence();
      continue;
    }
    if (tolower(key) != 'q' && monotonicSeconds() < settled) continue;
    switch (tolower(key)) {
      case 'a':
        return 0;
      case 'b':
        return 1;
      case 'c':
        return 2;
      case 'd':
        return 3;
      case 'q':
        return CHOICE_QUIT;
    }
  }
}
