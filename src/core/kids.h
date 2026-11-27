/* etapa-74: the Kids profile's Home: films for children, cartoons and kids' series (TMDB: rated for all ages,
   animation and family), the most popular first. Asked once a session; the board is rebuilt when they come. */
#ifndef SB_KIDS_H
#define SB_KIDS_H
#include "core/stremio.h"
#define KIDS_ROWS 3
const char *kids_title(int i);          /* the row's name (untranslated) */
int kids_items(int i, ItemList *out);   /* a copy (0: none yet) */
#endif
