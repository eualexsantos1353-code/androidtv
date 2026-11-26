#include "core/insight.h"
#include "core/tmdb.h"
#include "core/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAXF 6
static struct { char key[64]; int state, n, tv, s, e, tid; unsigned tok; InsightFace f[MAXF]; } I; /* state: 1 asking, 2 known */
static unsigned tokN;
static void take(const cJSON *arr) {
  const cJSON *c; cJSON_ArrayForEach(c, arr) { if (I.n >= MAXF) break; const char *pp = js_str(c, "profile_path", ""); if (!*pp) continue;
    const char *nm = js_str(c, "name", ""); for (int i = 0; i < I.n; i++) if (!strcmp(I.f[i].name, nm)) goto next;
    InsightFace *f = &I.f[I.n++]; snprintf(f->name, sizeof f->name, "%s", nm); snprintf(f->photo, sizeof f->photo, "https://image.tmdb.org/t/p/w185%s", pp);
    { const char *ch = js_str(c, "character", ""); if (!*ch) ch = js_str(cJSON_GetArrayItem(js_arr(c, "roles"), 0), "character", ""); snprintf(f->role, sizeof f->role, "%s", ch); }
    next:; }
}
static void on_series(int st, const unsigned char *b, size_t len, void *ud) {
  (void)len; if ((unsigned)(uintptr_t)ud != I.tok || I.state != 1) return; cJSON *j = st == 200 ? net_json(b) : NULL;
  take(js_arr(j, "cast")); I.state = 2; net_json_free(j); }
static void on_credits(int st, const unsigned char *b, size_t len, void *ud) {
  (void)len; if ((unsigned)(uintptr_t)ud != I.tok || I.state != 1) return; cJSON *j = st == 200 ? net_json(b) : NULL;
  take(js_arr(j, "cast")); if (I.tv) take(js_arr(j, "guest_stars")); net_json_free(j);
  if (!I.n && I.tv && I.s >= 0) { char p[64]; snprintf(p, sizeof p, "/tv/%d/aggregate_credits", I.tid); if (tmdb_request(p, on_series, (void *)(uintptr_t)I.tok)) return; } /* (an episode without its own: the series') */
  I.state = 2; }
static void on_find(int st, const unsigned char *b, size_t len, void *ud) {
  (void)len; if ((unsigned)(uintptr_t)ud != I.tok || I.state != 1) return; cJSON *j = st == 200 ? net_json(b) : NULL;
  const cJSON *r = cJSON_GetArrayItem(js_arr(j, I.tv ? "tv_results" : "movie_results"), 0); I.tid = (int)js_num(r, "id", 0); net_json_free(j);
  if (!I.tid) { I.state = 2; return; }
  char p[96]; if (I.tv && I.s >= 0 && I.e >= 0) snprintf(p, sizeof p, "/tv/%d/season/%d/episode/%d/credits", I.tid, I.s, I.e); else if (I.tv) snprintf(p, sizeof p, "/tv/%d/aggregate_credits", I.tid); else snprintf(p, sizeof p, "/movie/%d/credits", I.tid);
  if (!tmdb_request(p, I.tv && !(I.s >= 0 && I.e >= 0) ? on_series : on_credits, (void *)(uintptr_t)I.tok)) I.state = 2; }
int insight_get(const char *type, const char *imdb, int season, int episode, const InsightFace **out) {
  if (!imdb || strncmp(imdb, "tt", 2) || !tmdb_has_key()) return 0;
  char id[24]; snprintf(id, sizeof id, "%s", imdb); char *c = strchr(id, ':'); if (c) *c = 0;
  char key[64]; snprintf(key, sizeof key, "%s:%d:%d", id, season, episode);
  if (strcmp(key, I.key)) { memset(&I, 0, sizeof I); snprintf(I.key, sizeof I.key, "%s", key); I.tv = type && !strcmp(type, "series"); I.s = season; I.e = episode; I.state = 1; I.tok = ++tokN;
    char p[80]; snprintf(p, sizeof p, "/find/%s?external_source=imdb_id", id); if (!tmdb_request(p, on_find, (void *)(uintptr_t)I.tok)) I.state = 2; return 0; }
  if (I.state != 2 || !I.n) return 0; *out = I.f; return I.n;
}
