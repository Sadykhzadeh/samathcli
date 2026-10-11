#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#include "input.h"

#define CTRL_D 4
#define ESCAPE 27
/* How long the rest of an escape sequence is waited for. A terminal sends it
   in one piece, so this only has to outlast the wire. */
#define SEQUENCE_WAIT_MS 30

static struct termios original;
static volatile sig_atomic_t taken = 0;

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

void inputStart(void) {
  if (taken || !isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &original))
    return;

  struct termios keys = original;
  keys.c_lflag &= (tcflag_t) ~(ICANON | ECHO);
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
}

void inputDiscard(void) {
  if (taken) tcflush(STDIN_FILENO, TCIFLUSH);
}

/* One byte of input, or -1 at the end of it. With a wait, also -1 when
   nothing comes in time. */
static int readByte(int waitMs) {
  if (waitMs >= 0) {
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(STDIN_FILENO, &readable);
    struct timeval timeout = {waitMs / 1000, (waitMs % 1000) * 1000};
    int ready = select(STDIN_FILENO + 1, &readable, NULL, NULL, &timeout);
    if (ready <= 0) return -1;
  }
  unsigned char byte;
  for (;;) {
    ssize_t got = read(STDIN_FILENO, &byte, 1);
    if (got == 1) return byte;
    if (got < 0 && errno == EINTR) continue;
    return -1;
  }
}

/* The rest of a key such as an arrow, which arrives as ESC [ A or ESC O A. */
static void skipEscapeSequence(void) {
  int next = readByte(SEQUENCE_WAIT_MS);
  if (next != '[' && next != 'O') return;
  while ((next = readByte(SEQUENCE_WAIT_MS)) >= 0)
    if (next >= 0x40 && next <= 0x7e) return;
}

int readChoice(void) {
  for (;;) {
    int key = readByte(-1);
    if (key < 0 || key == CTRL_D) return CHOICE_QUIT;
    if (key == ESCAPE) {
      skipEscapeSequence();
      continue;
    }
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
