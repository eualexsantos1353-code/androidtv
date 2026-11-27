/* etapa-87: Home's "Because you watched X": TMDB's recommendations for the last title you finished (kept 12 hours) */
#ifndef SB_BECAUSE_H
#define SB_BECAUSE_H
#include "core/stremio.h"
int  because_items(ItemList *out, char *title, size_t cap); /* a copy of the row and its name (0: none yet) */
void because_changed(void);                                 /* a title was finished: asked again */
#endif
