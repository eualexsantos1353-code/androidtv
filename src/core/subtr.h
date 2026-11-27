/* Subtitles translated as they play (etapa-71): a downloaded subtitle in another language than the app's is put
   into the app's language through Google's free translator, a block of lines at a time, starting where the film
   is. Lines already translated replace the original ones in place; the rest stay as they came until their turn.
   Settings > Subtitles > Translate subtitles ("When there is none in my language" by default, or Always, or Off). Not for the film's own tracks or styled ASS ones. */
#ifndef SB_SUBTR_H
#define SB_SUBTR_H
#include "core/subs.h"
/* still(cues, n, tok): the player's answer to "are these lines still the ones on?" (asked before each change) */
/* hasMine: the lists have a subtitle in the app's language ("auto" then leaves this one as it is) */
void subtr_start(Cue *cues, int n, const char *lang, int hasMine, double at, unsigned tok, int (*still)(Cue *, int, unsigned));
void subtr_stop(void);
unsigned subtr_gen(void);   /* bumped each time lines change (the drawing's cache) */
int  subtr_active(void);    /* lines are being translated now */
#endif
