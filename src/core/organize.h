/* As the official Stremio's free tools (Nov 2026): the profiles of this TV, the catalogs of Home and Search put in
   order or hidden, and the streams filtered. Each profile keeps its own addons on or off, its own catalog order and
   its own stream filter (core/store keeps those per profile); the Library and Continue Watching are the account's. */
#ifndef SB_ORGANIZE_H
#define SB_ORGANIZE_H
#include "core/stremio.h"
#define PROF_N 5                         /* etapa-74: up to five, each with its own Stremio account (the Kids one: the first's) */
int  prof_cur(void);                     /* 0..prof_count()-1 */
const char *prof_name(int i);            /* as named (the defaults translated) */
int  prof_count(void);
int  prof_avatar(int i);                 /* 0..PROF_AV-1 */
int  prof_kid(int i);                    /* a Kids profile (-1: the current one) */
#define PROF_AV 8
void prof_set_name(int i, const char *name);
void prof_set_avatar(int i, int av);
void prof_set_kid(int i, int on);
int  prof_add(void);                     /* the new one's index, -1 when five already */
void prof_remove(int i);                 /* (never the first) */
int  prof_switch(int to);                /* its account swapped in: 1 signed in already, 0 it goes to sign in */
void prof_avatar_draw(int av, float cx, float cy, float r, const char *name, float a);
/* the catalog's setting key: "coh_<hash>" (Home, area 'h') or "cos_<hash>" (Search, 's'); value show / top / hide */
void catorg_key(const CatalogDef *d, char area, char *out, size_t cap);
int  catorg_apply(CatalogDef *d, int n, char area); /* the ones on top first, then the others, hidden ones left out: the new count */
int  sfilter_pass(const Stream *s);      /* Settings › Filter streams */
int  sfilter_on(void);
#endif
