/* X-Ray (as Prime Video's, and Apple TV's InSight): who plays in what is on screen and the title's trivia, for the
   player's panel. The cast from TMDB: an episode's own (its regulars and guest stars), else the series' or the film's;
   photo, name and character. A face picked: the person (age, birthplace, a short biography in the app's language,
   what they are known for). The trivia from IMDb, put into the app's language. Needs the TMDB key and an IMDb id.
   Main thread. */
#ifndef SB_INSIGHT_H
#define SB_INSIGHT_H
typedef struct { char name[80], role[96], photo[200]; int pid; } InsightFace;
/* the faces for type/id (season/episode -1 for a film); 0 while asked or when there are none */
int insight_get(const char *type, const char *imdb, int season, int episode, const InsightFace **out);
int insight_state(void);   /* the last asked: 1 asking, 2 known */
typedef struct { char title[120], poster[200], year[8]; } InsightWork;
typedef struct { int pid, state; char born[16], died[16], place[160], bio[2400]; InsightWork known[8]; int nknown; } InsightPerson; /* state 1 asking, 2 known */
const InsightPerson *insight_person(int pid); /* asks the first time; state tells when it is in */
/* the trivia, one fact a line, in the app's language when it could be put into it; 0 while asked or none.
   *state: 1 asking, 2 known */
int insight_trivia(const char *imdb, const char ***lines, int *state);
#endif
