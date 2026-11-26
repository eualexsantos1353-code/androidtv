/* Simkl (as Trakt, for people who keep their anime and series there): this TV linked once with Simkl's PIN (the
   user's own Simkl app's Client ID pasted from the phone, Settings › Keys › Simkl; then a code entered at
   simkl.com/pin), and what plays is told to it (start, pause, stop: 80 % or more is watched there). Films and
   episodes with an IMDb id. Main thread. */
#ifndef SB_SIMKL_H
#define SB_SIMKL_H
void simkl_init(void);
void simkl_tick(void);              /* every frame: the phone's Client ID, the PIN asked and waited for */
int  simkl_state(void);             /* 0 not set, 1 waiting for the code to be entered, 2 linked */
const char *simkl_code(void);       /* the code to enter at simkl.com/pin ("" when none) */
void simkl_unlink(void);
void simkl_player(const char *type, const char *videoId, int state, double pos, double dur);
void simkl_player_end(double pos, double dur);
#endif
