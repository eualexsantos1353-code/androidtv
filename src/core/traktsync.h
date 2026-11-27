/* etapa-80: the first profile's Trakt, read for the Calendar and the Retrospective:
   - the shows it follows: those on its calendar (30 days back to 90 ahead, premieres in its lists included) and on
     its watchlist; the Calendar asks each one's episodes as it does the Library's (kept 6 hours)
   - the year's history: hours per month, films and series, the series and the film of the year, the genre
     (kept 12 hours). Nothing without Trakt connected. */
#ifndef SB_TRAKTSYNC_H
#define SB_TRAKTSYNC_H
void tsync_shows_refresh(void);                 /* when stale: asked again (the Calendar opening) */
int  tsync_shows(char ids[][24], int max);      /* the imdb ids known */
void tsync_retro_refresh(int year);             /* when stale or another year */
int  tsync_busy(void);
/* the year's figures, 0 when none yet: hours per month, films, series, the top series and film (name, imdb id, hours), the genre */
typedef struct { int year, movies, series, ok; double hours, month[12]; char showName[160], showId[24], movieName[160], movieId[24], genre[48]; double showH, movieH; int moviePlays, movieRated; } TsyncYear; /* movieRated: its stars on Trakt (1-10), 0: the one seen most */
const TsyncYear *tsync_year(void);
void tsync_tick(void);                          /* etapa-81: the watchlist into the Library (every 3 h) */
void tsync_lib_push(const char *type, const char *id, const char *action); /* added or removed here: on the watchlist too */
#endif
