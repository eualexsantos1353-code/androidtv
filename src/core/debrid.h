/* Debrid, from the app itself (NuvioTV's): the user's TorBox, Real-Debrid or Premiumize key, added from the phone, and
   their cloud: the video files they keep there, played straight from it. Main thread, never blocks (the phone's Save is
   checked on the server thread). */
#ifndef SB_DEBRID_H
#define SB_DEBRID_H
void deb_init(void);                 /* after store_init */
int  deb_has_key(void);
const char *deb_service_name(void);  /* "TorBox", "Real-Debrid", "Premiumize" or "" */
void deb_key_clear(void);
unsigned deb_generation(void);       /* a key came or went, or the cloud's list came in */
typedef struct { char name[256], a[48], b[48], link[1024]; double size; } DebFile;
/* the cloud's video files, newest first: -1 while asked (asked now), else how many (0: none, or no key) */
int  deb_list(const DebFile **out);
void deb_refresh(void);              /* asked again next time */
void deb_play(int i);                /* its link asked for (TorBox, Premiumize) and played */
#endif
