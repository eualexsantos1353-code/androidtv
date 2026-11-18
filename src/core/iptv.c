#include "platform/plat.h"
#include "core/iptv.h"
#include "core/net.h"
#include "core/store.h"
#include "core/httpd.h"
#include "core/live.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <zlib.h>

typedef struct { char tvg[64]; time_t s, e; char title[96]; } Prog;
typedef struct { IptvCh c; char *url; } Row;

static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static char cfgPath[1200], m3uUrl[2048], epgUrl[2048];  /* the links: in memory and in cfgPath only */
static unsigned gen, loadedGen; static double listAt = -1e9, epgAt = -1e9; static int listBusy, epgBusy;
static Row *rows; static int nrows;
static Prog *progs; static int nprogs, capprogs;
#define MAX_ROWS 6000
#define MAX_PROGS 60000

static void trim(char *s) { size_t n = strlen(s); while (n && isspace((unsigned char)s[n - 1])) s[--n] = 0; char *p = s; while (*p && isspace((unsigned char)*p)) p++; if (p != s) memmove(s, p, strlen(p) + 1); }
static int is_http(const char *s) { return !strncmp(s, "http://", 7) || !strncmp(s, "https://", 8); }
static void save(void) { char tmp[1300]; snprintf(tmp, sizeof tmp, "%s.tmp", cfgPath); FILE *f = fopen(tmp, "wb"); if (!f) return; fprintf(f, "%s\n%s\n", m3uUrl, epgUrl); fclose(f); rename(tmp, cfgPath); }
static void free_rows(void) { for (int i = 0; i < nrows; i++) free(rows[i].url); free(rows); rows = NULL; nrows = 0; }

/* the phone page's Save: the list link must be an http(s) address; the guide link may be empty */
static int from_phone(const char *m3u, const char *epg) {
  char a[2048], b[2048]; snprintf(a, sizeof a, "%s", m3u ? m3u : ""); snprintf(b, sizeof b, "%s", epg ? epg : ""); trim(a); trim(b);
  if (!is_http(a) || (b[0] && !is_http(b))) return 0;
  pthread_mutex_lock(&mu); snprintf(m3uUrl, sizeof m3uUrl, "%s", a); snprintf(epgUrl, sizeof epgUrl, "%s", b); gen++; save(); pthread_mutex_unlock(&mu);
  return 1;
}
void iptv_init(void) {
  static int done; if (done) return; done = 1;
  snprintf(cfgPath, sizeof cfgPath, "%s/iptv.cfg", store_data_dir());
  FILE *f = fopen(cfgPath, "rb"); if (f) { if (fgets(m3uUrl, sizeof m3uUrl, f)) trim(m3uUrl); if (fgets(epgUrl, sizeof epgUrl, f)) trim(epgUrl); fclose(f); }
  if (!is_http(m3uUrl)) m3uUrl[0] = 0; if (!is_http(epgUrl)) epgUrl[0] = 0;
  httpd_set_iptv_handler(from_phone);
}
int iptv_has(void) { iptv_init(); return m3uUrl[0] != 0; }
int iptv_has_epg(void) { iptv_init(); return epgUrl[0] != 0; }
unsigned iptv_generation(void) { pthread_mutex_lock(&mu); unsigned g = gen; pthread_mutex_unlock(&mu); return g; }
void iptv_clear(void) {
  iptv_init(); pthread_mutex_lock(&mu); m3uUrl[0] = epgUrl[0] = 0; remove(cfgPath); gen++; pthread_mutex_unlock(&mu);
  free_rows(); free(progs); progs = NULL; nprogs = capprogs = 0; loadedGen = 0; listAt = epgAt = -1e9; live_refresh(1);
}

/* ---- the list ---- */
static unsigned hash_of(const char *s) { unsigned h = 2166136261u; for (; *s; s++) h = (h ^ (unsigned char)*s) * 16777619u; return h; }
static void attr(const char *line, const char *name, char *out, size_t cap) { /* name="value" inside an #EXTINF line */
  out[0] = 0; char pat[40]; snprintf(pat, sizeof pat, "%s=\"", name); const char *p = line;
  for (;;) { p = strstr(p, pat); if (!p) return; if (p == line || p[-1] == ' ' || p[-1] == '\t') break; p++; }
  p += strlen(pat); size_t o = 0; while (*p && *p != '"' && o + 1 < cap) out[o++] = *p++; out[o] = 0; }
static void parse_m3u(const char *t) {
  Row *R = calloc(MAX_ROWS, sizeof *R); if (!R) return; int n = 0; IptvCh cur; int have = 0; memset(&cur, 0, sizeof cur);
  const char *p = t;
  while (*p && n < MAX_ROWS) {
    const char *e = strchr(p, '\n'); size_t len = e ? (size_t)(e - p) : strlen(p); char line[4096]; if (len >= sizeof line) len = sizeof line - 1;
    memcpy(line, p, len); line[len] = 0; p = e ? e + 1 : p + len; trim(line);
    if (!line[0]) continue;
    if (!strncmp(line, "#EXTINF", 7)) { memset(&cur, 0, sizeof cur); have = 1; attr(line, "tvg-id", cur.tvg, sizeof cur.tvg); attr(line, "tvg-logo", cur.logo, sizeof cur.logo); attr(line, "group-title", cur.group, sizeof cur.group);
      char *comma = NULL; { int q = 0; for (char *c = line; *c; c++) { if (*c == '"') q = !q; else if (*c == ',' && !q) { comma = c; break; } } } if (comma) { snprintf(cur.name, sizeof cur.name, "%s", comma + 1); trim(cur.name); } if (!cur.name[0]) attr(line, "tvg-name", cur.name, sizeof cur.name); continue; }
    if (line[0] == '#') continue;
    if (have && is_http(line) && cur.name[0]) { snprintf(cur.id, sizeof cur.id, "iptv:%08x", hash_of(line));
      int dup = 0; for (int i = 0; i < n; i++) if (!strcmp(R[i].c.id, cur.id)) { dup = 1; break; }
      if (!dup) { R[n].c = cur; R[n].url = strdup(line); if (R[n].url) n++; } }
    have = 0; }
  free_rows(); rows = R; nrows = n;
  fprintf(stderr, "[iptv] list: %d channels\n", n); /* (never the address) */
}
static void on_list(int st, const unsigned char *b, size_t n, void *ud) {
  (void)ud; listBusy = 0; if (st != 200 || !b || !n) { fprintf(stderr, "[iptv] list: no answer (%d)\n", st); return; }
  parse_m3u((const char *)b); listAt = SDL_GetTicks() / 1000.0; live_refresh(1);
}

/* ---- the guide (XMLTV, plain or gzipped; read in pieces, only what is on now or soon is kept) ---- */
static time_t xmltime(const char *s) { /* 20261118120000 +0000 */
  struct tm t; memset(&t, 0, sizeof t); if (strlen(s) < 14) return 0; for (int i = 0; i < 14; i++) if (s[i] < '0' || s[i] > '9') return 0;
  #define D2(o) ((s[o] - '0') * 10 + (s[(o) + 1] - '0'))
  t.tm_year = D2(0) * 100 + D2(2) - 1900; t.tm_mon = D2(4) - 1; t.tm_mday = D2(6); t.tm_hour = D2(8); t.tm_min = D2(10); t.tm_sec = D2(12);
  #undef D2
  time_t r = timegm(&t); const char *z = s + 14; while (*z == ' ') z++;
  if ((*z == '+' || *z == '-') && isdigit((unsigned char)z[1]) && isdigit((unsigned char)z[2]) && isdigit((unsigned char)z[3]) && isdigit((unsigned char)z[4])) {
    int off = ((z[1] - '0') * 10 + (z[2] - '0')) * 3600 + ((z[3] - '0') * 10 + (z[4] - '0')) * 60; r += (*z == '+') ? -off : off; }
  return r; }
static void unesc(char *s) { char *o = s; while (*s) { if (*s == '&') { if (!strncmp(s, "&amp;", 5)) { *o++ = '&'; s += 5; continue; } if (!strncmp(s, "&lt;", 4)) { *o++ = '<'; s += 4; continue; } if (!strncmp(s, "&gt;", 4)) { *o++ = '>'; s += 4; continue; }
      if (!strncmp(s, "&quot;", 6)) { *o++ = '"'; s += 6; continue; } if (!strncmp(s, "&apos;", 6) || !strncmp(s, "&#39;", 5)) { *o++ = '\''; s += s[2] == 'p' ? 6 : 5; continue; } } *o++ = *s++; } *o = 0; }
static void prog_add(const char *tag, size_t tlen, const char *body, size_t blen, time_t now) { /* one <programme ...>...</programme> */
  char head[512]; if (tlen >= sizeof head) tlen = sizeof head - 1; memcpy(head, tag, tlen); head[tlen] = 0;
  Prog g; memset(&g, 0, sizeof g); char st[64] = "", sp[64] = ""; attr(head, "start", st, sizeof st); attr(head, "stop", sp, sizeof sp); attr(head, "channel", g.tvg, sizeof g.tvg);
  g.s = xmltime(st); g.e = xmltime(sp); if (!g.tvg[0] || !g.s || !g.e || g.e <= now || g.s > now + 8 * 3600) return;
  const char *a = NULL; for (const char *q = body; q + 6 < body + blen; q++) if (!strncmp(q, "<title", 6) && (q[6] == '>' || q[6] == ' ')) { a = q; break; }
  if (a) { const char *gt = memchr(a, '>', (size_t)(body + blen - a)); if (gt) { gt++; const char *en = gt; while (en < body + blen && *en != '<') en++; size_t l = (size_t)(en - gt); if (l >= sizeof g.title) l = sizeof g.title - 1; memcpy(g.title, gt, l); g.title[l] = 0; unesc(g.title); trim(g.title); } }
  if (!g.title[0] || nprogs >= MAX_PROGS) return;
  unesc(g.tvg);
  if (nprogs >= capprogs) { int nc = capprogs ? capprogs * 2 : 4096; Prog *np = realloc(progs, sizeof(Prog) * (size_t)nc); if (!np) return; progs = np; capprogs = nc; }
  progs[nprogs++] = g; }
/* feeds a piece; returns how many bytes from its start were used (the rest, an unfinished programme, goes with the next piece) */
static size_t xml_feed(const char *b, size_t n, time_t now) {
  size_t used = 0; const char *p = b, *end = b + n;
  for (;;) { const char *s = NULL; for (const char *q = p; q + 11 < end; q++) if (q[0] == '<' && !strncmp(q, "<programme ", 11)) { s = q; break; } if (!s) { used = n > 12 ? n - 12 : 0; break; }
    const char *gt = memchr(s, '>', (size_t)(end - s)); if (!gt) { used = (size_t)(s - b); break; }
    if (gt[-1] == '/') { p = gt + 1; used = (size_t)(p - b); continue; }
    const char *cl = NULL; for (const char *q = gt; q + 12 <= end; q++) if (q[0] == '<' && !strncmp(q, "</programme>", 12)) { cl = q; break; }
    if (!cl) { used = (size_t)(s - b); break; }
    prog_add(s, (size_t)(gt - s), gt + 1, (size_t)(cl - gt - 1), now); p = cl + 12; used = (size_t)(p - b); }
  return used; }
static void parse_epg(const unsigned char *b, size_t n) {
  time_t now = time(NULL); free(progs); progs = NULL; nprogs = capprogs = 0;
  if (n > 2 && b[0] == 0x1f && b[1] == 0x8b) {
    z_stream z; memset(&z, 0, sizeof z); if (inflateInit2(&z, 16 + MAX_WBITS) != Z_OK) return; z.next_in = (Bytef *)b; z.avail_in = (uInt)n;
    size_t cap = 1 << 20, have = 0; char *buf = malloc(cap + 1); if (!buf) { inflateEnd(&z); return; }
    for (;;) { if (have + (1 << 18) > cap) { cap *= 2; char *nb = realloc(buf, cap + 1); if (!nb) break; buf = nb; }
      z.next_out = (Bytef *)buf + have; z.avail_out = (uInt)(cap - have); int r = inflate(&z, Z_NO_FLUSH); have = cap - z.avail_out;
      size_t u = xml_feed(buf, have, now); if (u) { memmove(buf, buf + u, have - u); have -= u; }
      if (r == Z_STREAM_END || r != Z_OK) break; }
    free(buf); inflateEnd(&z);
  } else { char *tmp = malloc(n + 1); if (!tmp) return; memcpy(tmp, b, n); tmp[n] = 0; xml_feed(tmp, n, now); free(tmp); }
  fprintf(stderr, "[iptv] guide: %d programmes\n", nprogs); }
static void on_epg(int st, const unsigned char *b, size_t n, void *ud) {
  (void)ud; epgBusy = 0; if (st != 200 || !b || !n) { fprintf(stderr, "[iptv] guide: no answer (%d)\n", st); return; }
  parse_epg(b, n); epgAt = SDL_GetTicks() / 1000.0; }
void iptv_refresh(int force) {
  iptv_init(); if (!m3uUrl[0]) { if (nrows) { free_rows(); live_refresh(1); } return; }
  double t = SDL_GetTicks() / 1000.0; unsigned g = iptv_generation(); if (g != loadedGen) { loadedGen = g; force = 1; }
  char u1[2048], u2[2048]; pthread_mutex_lock(&mu); snprintf(u1, sizeof u1, "%s", m3uUrl); snprintf(u2, sizeof u2, "%s", epgUrl); pthread_mutex_unlock(&mu);
  if (!listBusy && (force || t - listAt > 6 * 3600)) { listBusy = 1; net_get_timeout(u1, 90000, on_list, NULL); }
  if (u2[0] && !epgBusy && (force || t - epgAt > 3 * 3600)) { epgBusy = 1; net_get_timeout(u2, 120000, on_epg, NULL); }
}
int iptv_count(void) { return nrows; }
const IptvCh *iptv_chan(int i) { return i >= 0 && i < nrows ? &rows[i].c : NULL; }
int iptv_is(const char *id) { return id && !strncmp(id, "iptv:", 5); }
int iptv_url(const char *id, char *out, size_t cap) { for (int i = 0; i < nrows; i++) if (!strcmp(rows[i].c.id, id)) { snprintf(out, cap, "%s", rows[i].url); return 1; } return 0; }
int iptv_epg(const char *id, char *now, size_t ncap, char *next, size_t xcap, int *pct) {
  if (!nprogs) return 0; const char *tvg = NULL; for (int i = 0; i < nrows; i++) if (!strcmp(rows[i].c.id, id)) { tvg = rows[i].c.tvg; break; }
  if (!tvg || !tvg[0]) return 0; time_t t = time(NULL); const Prog *cur = NULL, *nx = NULL;
  for (int i = 0; i < nprogs; i++) if (!strcmp(progs[i].tvg, tvg)) { if (progs[i].s <= t && progs[i].e > t) cur = &progs[i]; }
  time_t after = cur ? cur->e : t; for (int i = 0; i < nprogs; i++) if (!strcmp(progs[i].tvg, tvg) && progs[i].s >= after - 60 && progs[i].s >= t - 60 && (!nx || progs[i].s < nx->s) && progs[i].s != (cur ? cur->s : 0)) nx = &progs[i];
  if (!cur && !nx) return 0; if (now && ncap) snprintf(now, ncap, "%s", cur ? cur->title : ""); if (next && xcap) snprintf(next, xcap, "%s", nx ? nx->title : "");
  if (pct) *pct = cur && cur->e > cur->s ? (int)((t - cur->s) * 100 / (cur->e - cur->s)) : 0; return 1; }
