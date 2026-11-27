#include "core/traktsync.h"
#include "core/scrobble.h"
#include "core/trakt.h"
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
typedef struct { char id[24], name[160]; double h; int movie, plays; } Acc;
static Acc *acc; static int nacc, capacc; static double mon[12]; static int hy, page; static unsigned hgen; static TsyncYear Y;
typedef struct { char g[48]; double h; } Gn; static Gn gn[64]; static int ngn;
static void acc_add(const char *id, const char *name, double h, int movie) { for (int i = 0; i < nacc; i++) if (!strcmp(acc[i].id, id)) { acc[i].h += h; acc[i].plays++; return; }
  if (nacc == capacc) { capacc = capacc ? capacc * 2 : 128; Acc *n = realloc(acc, (size_t)capacc * sizeof *acc); if (!n) return; acc = n; } Acc *a = &acc[nacc++]; snprintf(a->id, sizeof a->id, "%s", id); snprintf(a->name, sizeof a->name, "%s", name); a->h = h; a->movie = movie; a->plays = 1; }
static void gn_add(const char *g, double h) { for (int i = 0; i < ngn; i++) if (!strcmp(gn[i].g, g)) { gn[i].h += h; return; } if (ngn < 64) { snprintf(gn[ngn].g, sizeof gn[ngn].g, "%s", g); gn[ngn++].h = h; } }
static void ask_ratings(void);
static void hist_done(void) {
  memset(&Y, 0, sizeof Y); Y.year = hy; for (int i = 0; i < 12; i++) { Y.month[i] = mon[i]; Y.hours += mon[i]; }
  for (int i = 0; i < nacc; i++) { if (acc[i].movie) { Y.movies++; if (acc[i].plays > Y.moviePlays || (acc[i].plays == Y.moviePlays && acc[i].h > Y.movieH)) { Y.moviePlays = acc[i].plays; Y.movieH = acc[i].h; snprintf(Y.movieName, sizeof Y.movieName, "%s", acc[i].name); snprintf(Y.movieId, sizeof Y.movieId, "%s", acc[i].id); } }
    else { Y.series++; if (acc[i].h > Y.showH) { Y.showH = acc[i].h; snprintf(Y.showName, sizeof Y.showName, "%s", acc[i].name); snprintf(Y.showId, sizeof Y.showId, "%s", acc[i].id); } } }
  int bg = -1; for (int i = 0; i < ngn; i++) if (bg < 0 || gn[i].h > gn[bg].h) bg = i; if (bg >= 0) { snprintf(Y.genre, sizeof Y.genre, "%s", gn[bg].g); if (Y.genre[0] >= 'a' && Y.genre[0] <= 'z') Y.genre[0] -= 32; }
  Y.ok = Y.hours > 0; fprintf(stderr, "[trakt] %d: %.0f h, %d films, %d series\n", hy, Y.hours, Y.movies, Y.series);
  cJSON *o = cJSON_CreateObject(); cJSON_AddNumberToObject(o, "at", (double)time(NULL)); cJSON_AddNumberToObject(o, "year", hy); cJSON_AddNumberToObject(o, "movies", Y.movies); cJSON_AddNumberToObject(o, "series", Y.series);
  cJSON *m = cJSON_CreateArray(); for (int i = 0; i < 12; i++) cJSON_AddItemToArray(m, cJSON_CreateNumber(Y.month[i])); cJSON_AddItemToObject(o, "month", m);
  cJSON_AddStringToObject(o, "showName", Y.showName); cJSON_AddStringToObject(o, "showId", Y.showId); cJSON_AddNumberToObject(o, "showH", Y.showH);
  cJSON_AddStringToObject(o, "movieName", Y.movieName); cJSON_AddStringToObject(o, "movieId", Y.movieId); cJSON_AddNumberToObject(o, "movieH", Y.movieH); cJSON_AddStringToObject(o, "genre", Y.genre);
  cJSON_AddNumberToObject(o, "moviePlays", Y.moviePlays); cJSON_AddNumberToObject(o, "movieRated", 0);
  store_set_node("traktYear", o); ask_ratings(); }
/* the film of the year is the one rated highest this year (Trakt's ratings); none rated: the one seen most */
static void on_ratings(int st, const unsigned char *b, size_t len, void *ud) {
  (void)len; if ((unsigned)(uintptr_t)ud != hgen) { return; } cJSON *j = st == 200 ? net_json(b) : NULL; const cJSON *e, *best = NULL; int br = 0; char yp[8]; snprintf(yp, sizeof yp, "%d-", hy);
  cJSON_ArrayForEach(e, j) { if (strncmp(js_str(e, "rated_at", ""), yp, 5)) continue; int r = (int)js_num(e, "rating", 0); if (r > br || (r == br && best && strcmp(js_str(e, "rated_at", ""), js_str(best, "rated_at", "")) > 0)) { br = r; best = e; } }
  if (best && br > 0) { cJSON *o = cJSON_Duplicate(store_node("traktYear"), 1); const cJSON *m = js_obj(best, "movie"); const char *im = js_str(js_obj(m, "ids"), "imdb", "");
    if (o && *im) { cJSON_DeleteItemFromObject(o, "movieName"); cJSON_AddStringToObject(o, "movieName", js_str(m, "title", "")); cJSON_DeleteItemFromObject(o, "movieId"); cJSON_AddStringToObject(o, "movieId", im);
      cJSON_DeleteItemFromObject(o, "movieRated"); cJSON_AddNumberToObject(o, "movieRated", br); cJSON_DeleteItemFromObject(o, "at"); cJSON_AddNumberToObject(o, "at", (double)time(NULL)); store_set_node("traktYear", o); fprintf(stderr, "[trakt] film of the year: rated %d\n", br); } else cJSON_Delete(o); }
  net_json_free(j); busyHist = 0; }
static void ask_ratings(void) { if (!scrobble_get("/sync/ratings/movies", on_ratings, (void *)(uintptr_t)hgen)) busyHist = 0; }
static void ask_page(void);
static void ask_ratings(void);
static void on_hist(int st, const unsigned char *b, size_t len, void *ud) {
  (void)len; if ((unsigned)(uintptr_t)ud != hgen) return; cJSON *j = st == 200 ? net_json(b) : NULL; int n = cJSON_GetArraySize(j); const cJSON *e;
  cJSON_ArrayForEach(e, j) { const char *wa = js_str(e, "watched_at", ""); int mo = atoi(wa + 5) - 1; if (mo < 0 || mo > 11) continue; int movie = !strcmp(js_str(e, "type", ""), "movie");
    const cJSON *x = js_obj(e, movie ? "movie" : "episode"), *sh = movie ? x : js_obj(e, "show"); double rt = js_num(x, "runtime", 0); if (rt <= 0) rt = js_num(sh, "runtime", movie ? 110 : 45); double h = rt / 60.0;
    if (!movie) { int soap = js_num(sh, "aired_episodes", 0) >= 120; const cJSON *g0; cJSON_ArrayForEach(g0, js_arr(sh, "genres")) if (cJSON_IsString(g0) && (!strcmp(g0->valuestring, "soap") || !strcmp(g0->valuestring, "telenovela"))) soap = 1;
      if (soap) continue; } /* etapa-81: soaps and telenovelas (hundreds of episodes) are left out of the year */
    mon[mo] += h; const char *im = js_str(js_obj(sh, "ids"), "imdb", ""); if (*im) acc_add(im, js_str(sh, "title", ""), h, movie);
    const cJSON *g; cJSON_ArrayForEach(g, js_arr(sh, "genres")) if (cJSON_IsString(g)) gn_add(g->valuestring, h); }
  net_json_free(j); if (n >= 100 && page < 40) { page++; ask_page(); } else hist_done(); }
static void ask_page(void) { char p[160]; snprintf(p, sizeof p, "/sync/history?extended=full&start_at=%d-01-01T00:00:00.000Z&end_at=%d-12-31T23:59:59.000Z&page=%d&limit=100", hy, hy, page);
  if (!scrobble_get(p, on_hist, (void *)(uintptr_t)hgen)) hist_done(); }
void tsync_retro_refresh(int year) {
  if (busyHist || scrobble_state() != TRAKT_LINKED) return; const cJSON *o = store_node("traktYear");
  if (o && cJSON_GetObjectItem(o, "moviePlays") && (int)js_num(o, "year", 0) == year && time(NULL) - js_num(o, "at", 0) < 12 * 3600) return; /* (one from before etapa-81: asked again) */
  busyHist = 1; hgen++; hy = year; page = 1; nacc = 0; ngn = 0; memset(mon, 0, sizeof mon); ask_page(); }
const TsyncYear *tsync_year(void) {
  const cJSON *o = store_node("traktYear"); if (!o) return NULL; static TsyncYear c; memset(&c, 0, sizeof c); c.year = (int)js_num(o, "year", 0); c.movies = (int)js_num(o, "movies", 0); c.series = (int)js_num(o, "series", 0);
  int i = 0; const cJSON *m; cJSON_ArrayForEach(m, js_arr(o, "month")) { if (i < 12) { c.month[i] = m->valuedouble; c.hours += m->valuedouble; } i++; }
  snprintf(c.showName, sizeof c.showName, "%s", js_str(o, "showName", "")); snprintf(c.showId, sizeof c.showId, "%s", js_str(o, "showId", "")); c.showH = js_num(o, "showH", 0);
  snprintf(c.movieName, sizeof c.movieName, "%s", js_str(o, "movieName", "")); snprintf(c.movieId, sizeof c.movieId, "%s", js_str(o, "movieId", "")); c.movieH = js_num(o, "movieH", 0); c.moviePlays = (int)js_num(o, "moviePlays", 0); c.movieRated = (int)js_num(o, "movieRated", 0);
  snprintf(c.genre, sizeof c.genre, "%s", js_str(o, "genre", "")); c.ok = c.hours > 0; return &c; }

/* ---- etapa-81: the Library and Trakt's watchlist, as Nuvio does: what is on the watchlist comes into the Library
   (once: removed here, it stays out), and what is added or removed here goes to the watchlist ---- */
#include "core/stremio.h"
#include "core/organize.h"
extern void board_invalidate(void);
static int importing; static double pullAt; static int pulling;
static int seen_has(const char *id) { return cJSON_GetObjectItem(store_node("traktLibSeen"), id) != NULL; }
static void seen_add(const char *id) { cJSON *o = cJSON_Duplicate(store_node("traktLibSeen"), 1); if (!cJSON_IsObject(o)) { cJSON_Delete(o); o = cJSON_CreateObject(); } if (!cJSON_GetObjectItem(o, id)) cJSON_AddTrueToObject(o, id); store_set_node("traktLibSeen", o); }
static void on_wl(int st, const unsigned char *b, size_t len, void *ud) {
  (void)len; int shows = (int)(uintptr_t)ud; pulling--; cJSON *j = st == 200 ? net_json(b) : NULL; const cJSON *e; int n = 0;
  cJSON_ArrayForEach(e, j) { const cJSON *x = js_obj(e, shows ? "show" : "movie"); const char *im = js_str(js_obj(x, "ids"), "imdb", ""); if (!im || strncmp(im, "tt", 2) || seen_has(im)) continue;
    seen_add(im); if (lib_has(im)) continue;
    Item it; memset(&it, 0, sizeof it); snprintf(it.id, sizeof it.id, "%s", im); snprintf(it.type, sizeof it.type, "%s", shows ? "series" : "movie"); snprintf(it.name, sizeof it.name, "%s", js_str(x, "title", ""));
    snprintf(it.poster, sizeof it.poster, "https://images.metahub.space/poster/medium/%s/img", im); snprintf(it.background, sizeof it.background, "https://images.metahub.space/background/medium/%s/img", im);
    importing = 1; lib_toggle(&it); importing = 0; n++; }
  net_json_free(j); if (n) { fprintf(stderr, "[trakt] %d from the watchlist into the Library\n", n); board_invalidate(); } }
static void fresh_refresh(void);
void tsync_tick(void) {
  if (prof_cur() == 0 && scrobble_state() == TRAKT_LINKED && app_splash_done && sb_signed_in()) fresh_refresh();
  if (pulling || prof_cur() != 0 || scrobble_state() != TRAKT_LINKED || !app_splash_done || !sb_signed_in() || !strcmp(store_get("traktLibrary", "true"), "false")) return;
  if (pullAt && app_now() - pullAt < 3 * 3600) return; pullAt = app_now();
  if (scrobble_get("/sync/watchlist/movies", on_wl, (void *)(uintptr_t)0)) pulling++; if (scrobble_get("/sync/watchlist/shows", on_wl, (void *)(uintptr_t)1)) pulling++;
  tsync_shows_refresh(); }
void tsync_lib_push(const char *type, const char *id, const char *action) {
  if (importing || prof_cur() != 0 || scrobble_state() != TRAKT_LINKED || !id || strncmp(id, "tt", 2) || !strcmp(store_get("traktLibrary", "true"), "false")) return;
  int add = !strcmp(action, "libraryAdd"), rm = !strcmp(action, "libraryRemove"); if (!add && !rm) return;
  seen_add(id); cJSON *body = cJSON_CreateObject(), *arr = cJSON_AddArrayToObject(body, type && !strcmp(type, "series") ? "shows" : "movies"), *o = cJSON_CreateObject(), *ids = cJSON_AddObjectToObject(o, "ids");
  cJSON_AddStringToObject(ids, "imdb", id); cJSON_AddItemToArray(arr, o);
  scrobble_post(add ? "/sync/watchlist" : "/sync/watchlist/remove", body, NULL, NULL); fprintf(stderr, "[trakt] watchlist: %s %s\n", add ? "added" : "removed", id); }

/* ---- etapa-82: "New episodes" from Trakt's progress: a show watched in the last year (anywhere: other apps, the
   phone) whose latest episode came out in the last 3 weeks and isn't watched yet; kept as "traktFresh" (3 h) ---- */
typedef struct { char id[24], slug[96], name[160]; int s, e, wantLast; double aired; } Fr;
static Fr *fr; static int nfr, frPend; static unsigned frGen; static double frAt;
static double iso_t(const char *s) { struct tm tm; memset(&tm, 0, sizeof tm); int hh = 0, mi = 0; if (sscanf(s, "%d-%d-%dT%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &hh, &mi) < 3) return 0; tm.tm_year -= 1900; tm.tm_mon--; tm.tm_hour = hh; tm.tm_min = mi; return (double)timegm(&tm); }
static void fr_done(void) { if (--frPend > 0) return; cJSON *a = cJSON_CreateArray(); int n = 0;
  for (int i = 0; i < nfr; i++) if (fr[i].aired > 0 && fr[i].s > 0) { cJSON *o = cJSON_CreateObject(); cJSON_AddStringToObject(o, "id", fr[i].id); cJSON_AddStringToObject(o, "name", fr[i].name); cJSON_AddNumberToObject(o, "s", fr[i].s); cJSON_AddNumberToObject(o, "e", fr[i].e); cJSON_AddNumberToObject(o, "aired", fr[i].aired); cJSON_AddItemToArray(a, o); n++; }
  cJSON *o = cJSON_CreateObject(); cJSON_AddNumberToObject(o, "at", (double)time(NULL)); cJSON_AddItemToObject(o, "shows", a); store_set_node("traktFresh", o); fprintf(stderr, "[trakt] %d shows with a new episode\n", n); board_invalidate(); }
static void on_last(int st, const unsigned char *b, size_t len, void *ud) { (void)len; unsigned g = (unsigned)((uintptr_t)ud >> 8); int i = (int)((uintptr_t)ud & 0xff); if (g != (frGen & 0xffffff)) return;
  cJSON *j = st == 200 ? net_json(b) : NULL; double t = iso_t(js_str(j, "first_aired", "")); if (!(t > 0 && t > (double)time(NULL) - 21 * 86400.0)) fr[i].aired = 0; else fr[i].aired = t; net_json_free(j); fr_done(); }
static void on_prog(int st, const unsigned char *b, size_t len, void *ud) { (void)len; unsigned g = (unsigned)((uintptr_t)ud >> 8); int i = (int)((uintptr_t)ud & 0xff); if (g != (frGen & 0xffffff)) return;
  cJSON *j = st == 200 ? net_json(b) : NULL; int aired = (int)js_num(j, "aired", 0), done = (int)js_num(j, "completed", 0); const cJSON *nx = js_obj(j, "next_episode");
  if (aired > done && nx) { fr[i].s = (int)js_num(nx, "season", 0); fr[i].e = (int)js_num(nx, "number", 0); char p[160]; snprintf(p, sizeof p, "/shows/%s/last_episode?extended=full", fr[i].slug);
    net_json_free(j); if (fr[i].s > 0 && trakt_request(p, on_last, ud)) return; fr[i].s = 0; fr_done(); return; }
  net_json_free(j); fr_done(); }
static void on_watched(int st, const unsigned char *b, size_t len, void *ud) {
  (void)len; if ((unsigned)(uintptr_t)ud != frGen) return; cJSON *j = st == 200 ? net_json(b) : NULL; const cJSON *e; double cut = (double)time(NULL) - 365 * 86400.0;
  free(fr); fr = calloc(48, sizeof *fr); nfr = 0; frPend = 1;
  cJSON_ArrayForEach(e, j) { if (nfr >= 48 || !fr) break; if (iso_t(js_str(e, "last_watched_at", "")) < cut) continue; const cJSON *sh = js_obj(e, "show"), *ids = js_obj(sh, "ids"); const char *im = js_str(ids, "imdb", ""), *sl = js_str(ids, "slug", "");
    if (!*im || !*sl) continue; int soap = 0; { const cJSON *g; cJSON_ArrayForEach(g, js_arr(sh, "genres")) if (cJSON_IsString(g) && !strcmp(g->valuestring, "soap")) soap = 1; } if (soap) continue;
    Fr *f = &fr[nfr]; snprintf(f->id, sizeof f->id, "%s", im); snprintf(f->slug, sizeof f->slug, "%s", sl); snprintf(f->name, sizeof f->name, "%s", js_str(sh, "title", "")); char p[160];
    snprintf(p, sizeof p, "/shows/%s/progress/watched?hidden=false&specials=false", sl); if (scrobble_get(p, on_prog, (void *)(uintptr_t)(((frGen & 0xffffff) << 8) | (unsigned)nfr))) { frPend++; nfr++; } }
  net_json_free(j); fr_done(); }
static void fresh_refresh(void) { if (frPend > 0) return; const cJSON *o = store_node("traktFresh"); if (o && time(NULL) - js_num(o, "at", 0) < 3 * 3600) return; if (frAt && app_now() - frAt < 600) return; frAt = app_now();
  frGen++; if (!scrobble_get("/sync/watched/shows?extended=noseasons", on_watched, (void *)(uintptr_t)frGen)) frAt = 0; }
int tsync_fresh(Item *out, int max) { const cJSON *e; int n = 0;
  cJSON_ArrayForEach(e, js_arr(store_node("traktFresh"), "shows")) { if (n >= max) break; Item *it = &out[n++]; memset(it, 0, sizeof *it); const char *im = js_str(e, "id", "");
    snprintf(it->id, sizeof it->id, "%s", im); snprintf(it->type, sizeof it->type, "series"); snprintf(it->name, sizeof it->name, "%s", js_str(e, "name", ""));
    snprintf(it->poster, sizeof it->poster, "https://images.metahub.space/poster/medium/%s/img", im); snprintf(it->background, sizeof it->background, "https://images.metahub.space/background/medium/%s/img", im);
    snprintf(it->videoId, sizeof it->videoId, "%s:%d:%d", im, (int)js_num(e, "s", 0), (int)js_num(e, "e", 0)); it->airAt = js_num(e, "aired", 0); }
  return n; }
