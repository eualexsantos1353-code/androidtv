/* Tracking addons (Stremio's addon type for Trakt-like services, Nov 2026): installed addons whose manifest lists the
   "player" or "library" resource are told what happens, as the official apps tell them:
   /player/{type}/{videoId}/action=start|pause|stop&currentTime=ms&duration=ms.json (a seek re-sends the state) and
   /library/{type}/{id}/action=libraryAdd|libraryRemove|watched|unwatched[&videoId=a,b].json. Fire and forget; the
   answer is not used. Settings › Tracking addons turns it off. Main thread. */
#ifndef SB_TRACKING_H
#define SB_TRACKING_H
/* the player, every frame it is open: state 0 loading, 1 playing, 2 paused; pos and dur in seconds */
void tracking_player(const char *type, const char *videoId, int state, double pos, double dur);
void tracking_player_end(double pos, double dur);
void tracking_library(const char *type, const char *id, const char *action, const char *videoIds);
int  tracking_count(void); /* how many tracking addons are installed and on */
#endif
