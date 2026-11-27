/* etapa-72: "Watch later": a quick list on this TV (hold OK on a card), its row on Home, newest first */
#ifndef SB_LATER_H
#define SB_LATER_H
#include "core/stremio.h"
int  later_has(const char *id);
int  later_toggle(const Item *it);   /* 1 = added, 0 = removed */
int  later_items(ItemList *out);     /* a copy, newest first; 0 = empty */
#endif
