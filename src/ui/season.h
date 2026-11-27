/* etapa-73: the seasons on Home: snow in December (to Epiphany), fireworks around New Year's midnight, confetti in
   Carnival, falling leaves and an orange glow for Halloween week. The logo wears a Santa hat at Christmas.
   Settings > Appearance > Seasonal themes (on). SB_SEASON=xmas|newyear|carnival|halloween forces one (testing). */
#ifndef SB_SEASON_H
#define SB_SEASON_H
enum { SEASON_NONE, SEASON_XMAS, SEASON_NEWYEAR, SEASON_CARNIVAL, SEASON_HALLOWEEN };
int  season_now(void);
void season_draw(float dt, float alpha);                 /* the particles, over the backdrop, under the rows */
void season_hat(float cx, float cy, float R, float a);   /* the Santa hat on a round mark of radius R centred at cx, cy */
#endif
