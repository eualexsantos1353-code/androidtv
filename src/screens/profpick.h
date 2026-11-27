/* etapa-74: "Who's watching?": the profiles as big avatars (as Netflix and Nuvio), opened at start when there is more
   than one and from Settings > Profiles. OK goes in (a profile without its account yet: its sign-in); Down to
   "Manage profiles", where OK on one edits it: its picture, its name (the keyboard), Kids on or off, removed. */
#ifndef SB_PROFPICK_H
#define SB_PROFPICK_H
void profpick_open(int atStart);
int  profpick_on(void);
void profpick_tick_start(void);   /* from the app's tick: at start, once Home is up */
#endif
