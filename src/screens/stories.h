/* etapa-73: Lumen Stories: the round "Stories" row on Home (new episodes of what you watch, then the week's
   releases); OK opens them full screen, one after another as on Instagram: the backdrop drifting, its logo, what is
   new; OK on one opens its page, Right/Left move, Back closes. */
#ifndef SB_STORIES_H
#define SB_STORIES_H
#include "core/stremio.h"
void stories_open(const Item *items, int n, int at);
int  stories_seen(const char *id);   /* seen already (its ring grey) */
#endif
