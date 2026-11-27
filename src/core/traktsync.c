#include "core/traktsync.h"
#include "core/scrobble.h"
#include "core/store.h"
#include "core/json.h"
#include "core/net.h"
#include "core/app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

static int busyShows, busyHist; static unsigned gen;
int tsync_busy(void) { return busyShows > 0 || busyHist > 0; }
static void add_show(cJSON *set, const cJSON *show) { const char *im = js_str(js_obj(show, "ids"), "imdb", ""); if (!im || strncmp(im, "tt", 2)) return; if (!cJSON_GetObjectItem(set, im)) cJSON_AddStringToObject(set, im, js_str(show, "title", "")); }
static cJSON *pendSet;
static void shows_done(void) { if (--busyShows > 0) return; cJSON *o = cJSON_CreateObject(); cJSON_AddNumberToObject(o, "at", (double)time(NULL)); cJSON_AddItemToObject(o, "ids", pendSet); pendSet = NULL;
  fprintf(stderr, "[trakt] %d shows followed (calendar and watchlist)\n", cJSON_GetArraySize(cJSON_GetObjectItem(o, "ids"))); store_set_node("traktShows", o); }
static void on_cal(int st, const unsigned char *b, size_t len, void *ud) { (void)len; if ((unsigned)(uintptr_t)ud != gen || !pendSet) return; cJSON *j = st == 200 ? net_json(b) : NULL; const cJSON *e;
  cJSON_ArrayForEach(e, j) add_show(pendSet, js_obj(e, "show")); net_json_free(j); shows_done(); }
void tsync_shows_refresh(void) {
  if (busyShows || scrobble_state() != TRAKT_LINKED) return; const cJSON *o = store_node("traktShows"); if (o && time(NULL) - js_num(o, "at", 0) < 6 * 3600) return;
  gen++; pendSet = cJSON_CreateObject(); char p[96], d[16];
  for (int i = 0; i < 4; i++) { time_t t = time(NULL) + (time_t)(-30 + i * 31) * 86400; struct tm tm; gmtime_r(&t, &tm); strftime(d, sizeof d, "%Y-%m-%d", &tm); snprintf(p, sizeof p, "/calendars/my/shows/%s/31", d);
    if (scrobble_get(p, on_cal, (void *)(uintptr_t)gen)) busyShows++; }
  if (scrobble_get("/sync/watchlist/shows", on_cal, (void *)(uintptr_t)gen)) busyShows++;
  if (!busyShows) { cJSON_Delete(pendSet); pendSet = NULL; } }
int tsync_shows(char ids[][24], int max) { const cJSON *e; int n = 0; cJSON_ArrayForEach(e, js_obj(store_node("traktShows"), "ids")) { if (n >= max) break; if (e->string) snprintf(ids[n++], 24, "%s", e->string); } return n; }

/* ---- the year ---- */
typedef struct { char id[24], name[160]; double h; int movie; } Acc;
static Acc *acc; static int nacc, capacc; static double mon[12]; static int hy, page; static unsigned hgen; static TsyncYear Y;
typedef struct { char g[48]; double h; } Gn; static Gn gn[64]; static int ngn;
static void acc_add(const char *id, const char *name, double h, int movie) { for (int i = 0; i < nacc; i++) if (!strcmp(acc[i].id, id)) { acc[i].h += h; return; }
  if (nacc == capacc) { capacc = capacc ? capacc * 2 : 128; Acc *n = realloc(acc, (size_t)capacc * sizeof *acc); if (!n) return; acc = n; } Acc *a = &acc[nacc++]; snprintf(a->id, sizeof a->id, "%s", id); snprintf(a->name, sizeof a->name, "%s", name); a->h = h; a->movie = movie; }
static void gn_add(const char *g, double h) { for (int i = 0; i < ngn; i++) if (!strcmp(gn[i].g, g)) { gn[i].h += h; return; } if (ngn < 64) { snprintf(gn[ngn].g, sizeof gn[ngn].g, "%s", g); gn[ngn++].h = h; } }
static void hist_done(void) {
  memset(&Y, 0, sizeof Y); Y.year = hy; for (int i = 0; i < 12; i++) { Y.month[i] = mon[i]; Y.hours += mon[i]; }
  for (int i = 0; i < nacc; i++) { if (acc[i].movie) { Y.movies++; if (acc[i].h > Y.movieH) { Y.movieH = acc[i].h; snprintf(Y.movieName, sizeof Y.movieName, "%s", acc[i].name); snprintf(Y.movieId, sizeof Y.movieId, "%s", acc[i].id); } }
    else { Y.series++; if (acc[i].h > Y.showH) { Y.showH = acc[i].h; snprintf(Y.showName, sizeof Y.showName, "%s", acc[i].name); snprintf(Y.showId, sizeof Y.showId, "%s", acc[i].id); } } }
  int bg = -1; for (int i = 0; i < ngn; i++) if (bg < 0 || gn[i].h > gn[bg].h) bg = i; if (bg >= 0) { snprintf(Y.genre, sizeof Y.genre, "%s", gn[bg].g); if (Y.genre[0] >= 'a' && Y.genre[0] <= 'z') Y.genre[0] -= 32; }
  Y.ok = Y.hours > 0; fprintf(stderr, "[trakt] %d: %.0f h, %d films, %d series\n", hy, Y.hours, Y.movies, Y.series);
  cJSON *o = cJSON_CreateObject(); cJSON_AddNumberToObject(o, "at", (double)time(NULL)); cJSON_AddNumberToObject(o, "year", hy); cJSON_AddNumberToObject(o, "movies", Y.movies); cJSON_AddNumberToObject(o, "series", Y.series);
  cJSON *m = cJSON_CreateArray(); for (int i = 0; i < 12; i++) cJSON_AddItemToArray(m, cJSON_CreateNumber(Y.month[i])); cJSON_AddItemToObject(o, "month", m);
  cJSON_AddStringToObject(o, "showName", Y.showName); cJSON_AddStringToObject(o, "showId", Y.showId); cJSON_AddNumberToObject(o, "showH", Y.showH);
  cJSON_AddStringToObject(o, "movieName", Y.movieName); cJSON_AddStringToObject(o, "movieId", Y.movieId); cJSON_AddNumberToObject(o, "movieH", Y.movieH); cJSON_AddStringToObject(o, "genre", Y.genre);
  store_set_node("traktYear", o); busyHist = 0; }
static void ask_page(void);
static void on_hist(int st, const unsigned char *b, size_t len, void *ud) {
  (void)len; if ((unsigned)(uintptr_t)ud != hgen) return; cJSON *j = st == 200 ? net_json(b) : NULL; int n = cJSON_GetArraySize(j); const cJSON *e;
  cJSON_ArrayForEach(e, j) { const char *wa = js_str(e, "watched_at", ""); int mo = atoi(wa + 5) - 1; if (mo < 0 || mo > 11) continue; int movie = !strcmp(js_str(e, "type", ""), "movie");
    const cJSON *x = js_obj(e, movie ? "movie" : "episode"), *sh = movie ? x : js_obj(e, "show"); double rt = js_num(x, "runtime", 0); if (rt <= 0) rt = js_num(sh, "runtime", movie ? 110 : 45); double h = rt / 60.0;
    mon[mo] += h; const char *im = js_str(js_obj(sh, "ids"), "imdb", ""); if (*im) acc_add(im, js_str(sh, "title", ""), h, movie);
    const cJSON *g; cJSON_ArrayForEach(g, js_arr(sh, "genres")) if (cJSON_IsString(g)) gn_add(g->valuestring, h); }
  net_json_free(j); if (n >= 100 && page < 40) { page++; ask_page(); } else hist_done(); }
static void ask_page(void) { char p[160]; snprintf(p, sizeof p, "/sync/history?extended=full&start_at=%d-01-01T00:00:00.000Z&end_at=%d-12-31T23:59:59.000Z&page=%d&limit=100", hy, hy, page);
  if (!scrobble_get(p, on_hist, (void *)(uintptr_t)hgen)) hist_done(); }
void tsync_retro_refresh(int year) {
  if (busyHist || scrobble_state() != TRAKT_LINKED) return; const cJSON *o = store_node("traktYear");
  if (o && (int)js_num(o, "year", 0) == year && time(NULL) - js_num(o, "at", 0) < 12 * 3600) return;
  busyHist = 1; hgen++; hy = year; page = 1; nacc = 0; ngn = 0; memset(mon, 0, sizeof mon); ask_page(); }
const TsyncYear *tsync_year(void) {
  const cJSON *o = store_node("traktYear"); if (!o) return NULL; static TsyncYear c; memset(&c, 0, sizeof c); c.year = (int)js_num(o, "year", 0); c.movies = (int)js_num(o, "movies", 0); c.series = (int)js_num(o, "series", 0);
  int i = 0; const cJSON *m; cJSON_ArrayForEach(m, js_arr(o, "month")) { if (i < 12) { c.month[i] = m->valuedouble; c.hours += m->valuedouble; } i++; }
  snprintf(c.showName, sizeof c.showName, "%s", js_str(o, "showName", "")); snprintf(c.showId, sizeof c.showId, "%s", js_str(o, "showId", "")); c.showH = js_num(o, "showH", 0);
  snprintf(c.movieName, sizeof c.movieName, "%s", js_str(o, "movieName", "")); snprintf(c.movieId, sizeof c.movieId, "%s", js_str(o, "movieId", "")); c.movieH = js_num(o, "movieH", 0);
  snprintf(c.genre, sizeof c.genre, "%s", js_str(o, "genre", "")); c.ok = c.hours > 0; return &c; }
