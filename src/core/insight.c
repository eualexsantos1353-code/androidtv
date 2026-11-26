#include "core/insight.h"
#include "core/tmdb.h"
#include "core/json.h"
#include "core/net.h"
#include "core/i18n.h"
#include "core/addons.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ---- the cast ---- */
#define MAXF 24
static struct { char key[64]; int state, n, tv, s, e, tid; unsigned tok; InsightFace f[MAXF]; } I; /* state: 1 asking, 2 known */
static unsigned tokN;
int insight_state(void) { return I.state; }
static void take(const cJSON *arr) {
  const cJSON *c; cJSON_ArrayForEach(c, arr) { if (I.n >= MAXF) break; const char *pp = js_str(c, "profile_path", ""); if (!*pp) continue;
    const char *nm = js_str(c, "name", ""); for (int i = 0; i < I.n; i++) if (!strcmp(I.f[i].name, nm)) goto next;
    InsightFace *f = &I.f[I.n++]; snprintf(f->name, sizeof f->name, "%s", nm); snprintf(f->photo, sizeof f->photo, "https://image.tmdb.org/t/p/w185%s", pp); f->pid = (int)js_num(c, "id", 0);
    { const char *ch = js_str(c, "character", ""); if (!*ch) ch = js_str(cJSON_GetArrayItem(js_arr(c, "roles"), 0), "character", ""); snprintf(f->role, sizeof f->role, "%s", ch); }
    next:; }
}
static void on_series(int st, const unsigned char *b, size_t len, void *ud) {
  (void)len; if ((unsigned)(uintptr_t)ud != I.tok || I.state != 1) return; cJSON *j = st == 200 ? net_json(b) : NULL;
  take(js_arr(j, "cast")); I.state = 2; net_json_free(j); }
static void on_credits(int st, const unsigned char *b, size_t len, void *ud) {
  (void)len; if ((unsigned)(uintptr_t)ud != I.tok || I.state != 1) return; cJSON *j = st == 200 ? net_json(b) : NULL;
  take(js_arr(j, "cast")); if (I.tv) take(js_arr(j, "guest_stars")); net_json_free(j);
  if (I.tv && I.n < 6) { char p[64]; snprintf(p, sizeof p, "/tv/%d/aggregate_credits", I.tid); if (tmdb_request(p, on_series, (void *)(uintptr_t)I.tok)) return; } /* (an episode with few of its own: the series' too) */
  I.state = 2; }
static void on_find(int st, const unsigned char *b, size_t len, void *ud) {
  (void)len; if ((unsigned)(uintptr_t)ud != I.tok || I.state != 1) return; cJSON *j = st == 200 ? net_json(b) : NULL;
  const cJSON *r = cJSON_GetArrayItem(js_arr(j, I.tv ? "tv_results" : "movie_results"), 0); I.tid = (int)js_num(r, "id", 0); net_json_free(j);
  if (!I.tid) { I.state = 2; return; }
  char p[96]; int epi = I.tv && I.s >= 0 && I.e >= 0;
  if (epi) snprintf(p, sizeof p, "/tv/%d/season/%d/episode/%d/credits", I.tid, I.s, I.e); else if (I.tv) snprintf(p, sizeof p, "/tv/%d/aggregate_credits", I.tid); else snprintf(p, sizeof p, "/movie/%d/credits", I.tid);
  if (!tmdb_request(p, I.tv && !epi ? on_series : on_credits, (void *)(uintptr_t)I.tok)) I.state = 2; }
int insight_get(const char *type, const char *imdb, int season, int episode, const InsightFace **out) {
  if (!imdb || strncmp(imdb, "tt", 2) || !tmdb_has_key()) return 0;
  char id[24]; snprintf(id, sizeof id, "%s", imdb); char *c = strchr(id, ':'); if (c) *c = 0;
  char key[64]; snprintf(key, sizeof key, "%s:%d:%d", id, season, episode);
  if (strcmp(key, I.key)) { memset(&I, 0, sizeof I); snprintf(I.key, sizeof I.key, "%s", key); I.tv = type && !strcmp(type, "series"); I.s = season; I.e = episode; I.state = 1; I.tok = ++tokN;
    char p[80]; snprintf(p, sizeof p, "/find/%s?external_source=imdb_id", id); if (!tmdb_request(p, on_find, (void *)(uintptr_t)I.tok)) I.state = 2; return 0; }
  if (I.state != 2 || !I.n) return 0; *out = I.f; return I.n;
}

/* ---- a person ---- */
static InsightPerson PP; static unsigned ptok;
static int cmp_pop(const void *a, const void *b) { double x = js_num(*(const cJSON *const *)a, "popularity", 0) + js_num(*(const cJSON *const *)a, "vote_count", 0) / 400.0, y = js_num(*(const cJSON *const *)b, "popularity", 0) + js_num(*(const cJSON *const *)b, "vote_count", 0) / 400.0; return x < y ? 1 : x > y ? -1 : 0; }
static void on_person(int st, const unsigned char *b, size_t len, void *ud) {
  (void)len; if ((unsigned)(uintptr_t)ud != ptok) return; cJSON *j = st == 200 ? net_json(b) : NULL;
  snprintf(PP.born, sizeof PP.born, "%s", js_str(j, "birthday", "")); snprintf(PP.died, sizeof PP.died, "%s", js_str(j, "deathday", "")); snprintf(PP.place, sizeof PP.place, "%s", js_str(j, "place_of_birth", ""));
  if (!PP.bio[0]) snprintf(PP.bio, sizeof PP.bio, "%s", js_str(j, "biography", ""));
  const cJSON *cast = js_arr(js_obj(j, "combined_credits"), "cast"); int n = cJSON_GetArraySize(cast);
  if (n > 0) { const cJSON **v = calloc((size_t)n, sizeof *v); int m = 0; const cJSON *c; if (v) { cJSON_ArrayForEach(c, cast) { if (!*js_str(c, "poster_path", "")) continue; const char *ch = js_str(c, "character", ""); if (strstr(ch, "Himself") || strstr(ch, "Herself") || strstr(ch, "Self") || strstr(ch, "self")) continue;
          int skip = 0; const cJSON *g; cJSON_ArrayForEach(g, js_arr(c, "genre_ids")) { int gi = g->valueint; if (gi == 10767 || gi == 10763 || gi == 10764 || gi == 99) skip = 1; } /* talk shows, news, reality, documentaries: not their work */
          if (!skip && !strcmp(js_str(c, "media_type", ""), "tv") && js_num(c, "episode_count", 2) < 2) skip = 1; /* (one episode of a show: a guest spot) */
          if (!skip) v[m++] = c; }
      qsort(v, (size_t)m, sizeof *v, cmp_pop);
      for (int i = 0; i < m && PP.nknown < 8; i++) { const char *t = js_str(v[i], "title", js_str(v[i], "name", "")); int dup = 0; for (int k = 0; k < PP.nknown; k++) if (!strcmp(PP.known[k].title, t)) dup = 1; if (dup) continue;
        InsightWork *w = &PP.known[PP.nknown++]; snprintf(w->title, sizeof w->title, "%s", t); snprintf(w->poster, sizeof w->poster, "https://image.tmdb.org/t/p/w185%s", js_str(v[i], "poster_path", ""));
        snprintf(w->year, sizeof w->year, "%.4s", js_str(v[i], "release_date", js_str(v[i], "first_air_date", ""))); }
      free(v); } }
  net_json_free(j);
  if (!PP.bio[0] && strcmp(i18n_lang(), "en") && !(PP.state & 4)) { PP.state |= 4; char p[64]; snprintf(p, sizeof p, "/person/%d?language=en-US", PP.pid); if (tmdb_request(p, on_person, (void *)(uintptr_t)ptok)) return; } /* (no biography in the app's language: the English one) */
  PP.state = 2; }
const InsightPerson *insight_person(int pid) {
  if (pid <= 0) return NULL;
  if (PP.pid != pid) { memset(&PP, 0, sizeof PP); PP.pid = pid; PP.state = 1; ptok++;
    char p[120]; snprintf(p, sizeof p, "/person/%d?language=%s&append_to_response=combined_credits", pid, i18n_lang()); if (!tmdb_request(p, on_person, (void *)(uintptr_t)ptok)) PP.state = 2; }
  return &PP;
}

/* ---- the trivia ---- */
#define MAXT 24
#define IMDB_GQL "https://api.graphql.imdb.com/"
static struct { char id[24]; int state, n, form, pend; unsigned tok; char *line[MAXT]; } TV;
static void tv_clear(void) { for (int i = 0; i < TV.n; i++) free(TV.line[i]); memset(&TV, 0, sizeof TV); }
static void strip_html(char *s) { char *o = s; int in = 0; for (char *p = s; *p; p++) { if (*p == '<') in = 1; else if (*p == '>') in = 0; else if (!in) *o++ = *p; } *o = 0;
  struct { const char *e, *r; } E[] = { { "&amp;", "&" }, { "&quot;", "\"" }, { "&#39;", "'" }, { "&lt;", "<" }, { "&gt;", ">" }, { "&nbsp;", " " } };
  for (int k = 0; k < 6; k++) { char *p; size_t el = strlen(E[k].e); while ((p = strstr(s, E[k].e))) { *p = E[k].r[0]; memmove(p + 1, p + el, strlen(p + el) + 1); } } }
static void on_tr(int st, const unsigned char *b, size_t len, void *ud) { /* Google's free translation: [[["text","source",...],...],...] */
  (void)len; unsigned u = (unsigned)(uintptr_t)ud; if ((u >> 8) != (TV.tok & 0xffffff)) return; int i = (int)(u & 255); if (i >= TV.n) return;
  cJSON *j = st == 200 ? net_json(b) : NULL; const cJSON *parts = cJSON_GetArrayItem(j, 0);
  if (cJSON_IsArray(parts)) { char out[2400] = ""; size_t o = 0; const cJSON *p; cJSON_ArrayForEach(p, parts) { const cJSON *t = cJSON_GetArrayItem(p, 0); if (cJSON_IsString(t) && o + strlen(t->valuestring) + 1 < sizeof out) o += (size_t)snprintf(out + o, sizeof out - o, "%s", t->valuestring); }
    if (o) { free(TV.line[i]); TV.line[i] = strdup(out); } }
  net_json_free(j); if (--TV.pend <= 0) TV.state = 2; }
static void ask_tr(void);
static void on_trivia(int st, const unsigned char *b, size_t len, void *ud) {
  (void)len; if ((unsigned)(uintptr_t)ud != TV.tok || TV.state != 1) return; cJSON *j = st == 200 ? net_json(b) : NULL;
  const cJSON *ed = js_arr(js_obj(js_obj(js_obj(j, "data"), "title"), "trivia"), "edges"), *e;
  if (!ed && TV.form == 0) { net_json_free(j); TV.form = 1; ask_tr(); return; } /* (the other way of asking: IMDb's own pages have used both) */
  cJSON_ArrayForEach(e, ed) { if (TV.n >= MAXT) break; const cJSON *nd = js_obj(e, "node");
    const char *tx = TV.form == 0 ? js_str(js_obj(nd, "text"), "plainText", "") : js_str(js_obj(js_obj(nd, "displayableArticle"), "body"), "plainText", "");
    if (!*tx) tx = js_str(js_obj(nd, "text"), "plaidHtml", ""); if (!*tx || strlen(tx) > 900) continue;
    char *s = strdup(tx); if (!s) continue; strip_html(s); TV.line[TV.n++] = s; }
  net_json_free(j);
  fprintf(stderr, "[xray] %s: %d trivia (form %d)\n", TV.id, TV.n, TV.form);
  const char *lg = i18n_lang(); if (!TV.n || !strncmp(lg, "en", 2)) { TV.state = 2; return; }
  char tl[16]; snprintf(tl, sizeof tl, "%s", !strcmp(lg, "pt-BR") ? "pt" : lg); TV.pend = TV.n;
  for (int i = 0; i < TV.n; i++) { char q[2800]; addon_urlenc(TV.line[i], q, sizeof q); char url[3200]; snprintf(url, sizeof url, "https://translate.googleapis.com/translate_a/single?client=gtx&sl=en&tl=%s&dt=t&q=%s", tl, q);
    net_get(url, on_tr, (void *)(uintptr_t)((TV.tok & 0xffffff) << 8 | (unsigned)i)); } }
static void ask_tr(void) {
  char q[600]; snprintf(q, sizeof q, TV.form == 0 ? "{ title(id: \"%s\") { trivia(first: 24) { edges { node { text { plainText } } } } } }" : "{ title(id: \"%s\") { trivia(first: 24) { edges { node { displayableArticle { body { plainText } } } } } } }", TV.id);
  cJSON *b = cJSON_CreateObject(); cJSON_AddStringToObject(b, "query", q); char *js = cJSON_PrintUnformatted(b); cJSON_Delete(b); if (!js) { TV.state = 2; return; }
  net_post_hdr_t(IMDB_GQL, js, "application/json", "x-imdb-client-name: imdb-web-next\nUser-Agent: Mozilla/5.0", 12000, on_trivia, (void *)(uintptr_t)TV.tok); free(js); }
int insight_trivia(const char *imdb, const char ***lines, int *state) {
  if (!imdb || strncmp(imdb, "tt", 2)) { *state = 2; return 0; }
  char id[24]; snprintf(id, sizeof id, "%s", imdb); char *c = strchr(id, ':'); if (c) *c = 0;
  if (strcmp(id, TV.id)) { unsigned t = TV.tok; tv_clear(); TV.tok = t + 1; snprintf(TV.id, sizeof TV.id, "%s", id); TV.state = 1; ask_tr(); }
  *state = TV.state; if (TV.state != 2) return 0; *lines = (const char **)TV.line; return TV.n;
}
