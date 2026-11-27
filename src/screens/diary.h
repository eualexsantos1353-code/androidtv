/* etapa-79: the film diary: a film watched to its end (85% or more), the Lumen asks its stars (1 to 5) as the player
   closes; kept with the day ("diary", per profile's account). Library > Diary shows them on the Wall; the
   Retrospective counts them. */
#ifndef SB_DIARY_H
#define SB_DIARY_H
#include "core/stremio.h"
void diary_ask(const Item *it);          /* the card, once the player has gone */
int  diary_items(Item *out, int max);    /* newest first, the stars in front of the name */
int  diary_stats(int year, int *n, double *avg);
#endif
