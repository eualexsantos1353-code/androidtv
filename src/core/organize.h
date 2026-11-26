/* As the official Stremio's free tools (Nov 2026): the profiles of this TV, the catalogs of Home and Search put in
   order or hidden, and the streams filtered. Each profile keeps its own addons on or off, its own catalog order and
   its own stream filter (core/store keeps those per profile); the Library and Continue Watching are the account's. */
#ifndef SB_ORGANIZE_H
#define SB_ORGANIZE_H
#include "core/stremio.h"
#define PROF_N 3
int  prof_cur(void);                     /* 0..PROF_N-1 */
const char *prof_name(int i);            /* translated */
/* the catalog's setting key: "coh_<hash>" (Home, area 'h') or "cos_<hash>" (Search, 's'); value show / top / hide */
void catorg_key(const CatalogDef *d, char area, char *out, size_t cap);
int  catorg_apply(CatalogDef *d, int n, char area); /* the ones on top first, then the others, hidden ones left out: the new count */
int  sfilter_pass(const Stream *s);      /* Settings › Filter streams */
int  sfilter_on(void);
#endif
