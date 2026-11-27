/* etapa-72: Home's "New releases" row: films out on streaming / digital in Brazil in the last 45 days and series that
   premiered in the last 30, the most popular first (TMDB, in the app's language). Asked once every 6 hours. */
#ifndef SB_RELEASES_H
#define SB_RELEASES_H
#include "core/stremio.h"
int releases_items(ItemList *out); /* a copy of the row (0: none yet; the first call asks, the board is rebuilt when it comes) */
#endif
