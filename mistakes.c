#include <stdlib.h>
#include "mistakes.h"

/* A list starts with room for this many, and doubles from there. */
#define MISTAKES_FIRST_ROOM 8

int mistakesAdd(Mistakes* list, const Mistake* m) {
  if (list->count == list->room) {
    long room = list->room ? list->room * 2 : MISTAKES_FIRST_ROOM;
    Mistake* grown = realloc(list->items, (size_t)room * sizeof *grown);
    if (!grown) return 0;
    list->items = grown;
    list->room = room;
  }
  list->items[list->count++] = *m;
  return 1;
}

void mistakesFree(Mistakes* list) {
  free(list->items);
  list->items = NULL;
  list->count = 0;
  list->room = 0;
}
