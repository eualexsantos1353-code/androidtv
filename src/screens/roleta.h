#ifndef SB_ROLETA_H
#define SB_ROLETA_H
#include "core/app.h"
#define ROLETA_ID "__roleta"   /* Home's Roulette card: OK opens the Roulette (not a title) */
extern Screen roletaScreen;
void roleta_open(void);    /* Settings > About > Roulette, or the remote's yellow key on Home, Discover and Library */
int  roleta_covers(void);  /* fully in: opaque edge to edge (the page under it is not drawn) */
#endif
