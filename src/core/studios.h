/* The studios and networks behind a title (NuvioTV's company logos): TMDB's logos, found from the IMDb id, kept for the
   session. Main thread, never blocks; nothing without a TMDB key. */
#ifndef SB_STUDIOS_H
#define SB_STUDIOS_H
/* up to 3 logo addresses (and the names, for when a logo fails); 0 while asked, or none. type "movie" / "series" */
int studios_get(const char *type, const char *imdb, const char *logos[3], const char *names[3]);
unsigned studios_generation(void);
#endif
