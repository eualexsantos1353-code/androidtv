/* "In this scene" (as Apple TV's InSight and Prime Video's X-Ray): who plays in what is on screen, shown while the film
   is paused. The cast from TMDB: an episode's own (its regulars and guest stars), else the series' or the film's;
   photo, name and character, the first 6 billed. Needs the TMDB key and an IMDb id. Main thread. */
#ifndef SB_INSIGHT_H
#define SB_INSIGHT_H
typedef struct { char name[80], role[80], photo[200]; } InsightFace;
/* the faces for type/id (season/episode -1 for a film); 0 while asked or when there are none */
int insight_get(const char *type, const char *imdb, int season, int episode, const InsightFace **out);
#endif
