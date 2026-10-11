#ifndef SAMATH_INPUT_H
#define SAMATH_INPUT_H

/* What readChoice gives back: 0 to 3 for A to D, or this. */
#define CHOICE_QUIT (-1)

/* At a terminal, takes the keyboard over so that one key is one answer,
   with no Enter and nothing echoed. The terminal is handed back on the way
   out, also when the run is interrupted. Anywhere else, standard input is
   read as it comes and this does nothing. */
void inputStart(void);

/* Drops keys typed ahead of the question, so that a stray press while the
   questions were loading does not answer the first one. */
void inputDiscard(void);

/* Waits for an answer: a to d in either case. Q, Ctrl-D and the end of the
   input all give CHOICE_QUIT. Every other key is ignored, arrow keys and
   other escape sequences included, whose last letter would otherwise be
   taken for an answer. */
int readChoice(void);

#endif
