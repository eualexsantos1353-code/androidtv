/* Each episode's IMDb rating (NuvioTV's "episode ratings"): one show's episodes asked from IMDb's own GraphQL at once,
   kept for the session. Main thread, never blocks. */
#ifndef SB_EPRATING_H
#define SB_EPRATING_H
/* the rating 0-10, -1 while it is asked for or when IMDb has none; show: the series' IMDb id ("tt..."; "tt1:1:2" takes the show's) */
float eprating_get(const char *show, int season, int episode);
unsigned eprating_generation(void);   /* bumps when a show's ratings come in: draw code reads again */
#endif
