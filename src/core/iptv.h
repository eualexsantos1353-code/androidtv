/* The viewer's own IPTV list (an M3U link) and its programme guide (an XMLTV link). Both links carry a user name and
   password: they are asked for on the phone (the QR page), kept in a file on this TV alone (never in the store that
   syncs, never in a log) and used only to fetch the list and the guide. */
#ifndef SB_IPTV_H
#define SB_IPTV_H
#include <stddef.h>

typedef struct { char id[24], name[96], logo[256], group[64], tvg[64]; } IptvCh;

void     iptv_init(void);                 /* reads the saved links; the phone page's Save is wired here */
int      iptv_has(void);                  /* a list link is saved */
int      iptv_has_epg(void);
unsigned iptv_generation(void);           /* bumps when the phone saves the links */
void     iptv_clear(void);                /* "Remove the IPTV list": the links, the list and the guide go */
void     iptv_refresh(int force);         /* fetches the list (and the guide) when due; the live lists are rebuilt when it lands */
int      iptv_count(void);
const IptvCh *iptv_chan(int i);
int      iptv_is(const char *id);         /* an id of this list ("iptv:...") */
int      iptv_url(const char *id, char *out, size_t cap); /* the channel's stream address, 1 = found */
/* the guide: what is on now and next (titles), pct = how far into the current one (0..100); 1 when something is known */
int      iptv_epg(const char *id, char *now, size_t ncap, char *next, size_t xcap, int *pct);
#endif
