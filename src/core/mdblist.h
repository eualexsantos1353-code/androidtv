/* MDBList: the scores other sites give a title (Rotten Tomatoes, Metacritic, Trakt, Letterboxd...), with the user's own
   free key added from the phone (kept in the data dir as mdblist.key). Without a key nothing is asked and nothing
   shows. One call per title, kept for the session. Main thread, never blocks. */
#ifndef SB_MDBLIST_H
#define SB_MDBLIST_H
void mdb_init(void);                    /* after store_init; otherwise done on first use */
int  mdb_has_key(void);
void mdb_key_clear(void);               /* "Remove the MDBList key" */
unsigned mdb_generation(void);          /* bumps when a key comes or goes, or a title's scores come in: draw code reads again */
/* the title's scores as one line ("RT 92% · MC 71 · Trakt 78% · Letterboxd 3.9"; IMDb is left out: the page has it),
   "" while they are asked for, or when there are none. type "movie" or "series"; id an IMDb id (an episode's "tt1:1:2" takes the show's) */
const char *mdb_line(const char *type, const char *id);
#endif
