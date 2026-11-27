#include "core/releases.h"
#include "core/tmdb.h"
#include "core/i18n.h"
#include "core/store.h"
#include "core/json.h"
#include "core/app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define MAXR 24
typedef struct { Item it; int tm, tv; double pop; int done; } Rel;
static struct { Rel r[MAXR]; int n, wait, lists; unsigned gen; double askedAt; ItemList ready; } R;
extern void board_invalidate(void);

static void day(char *o, size_t n, int back) { time_t t = time(NULL) - (time_t)back * 86400; struct tm tm; localtime_r(&t, &tm); strftime(o, n, "%Y-%m-%d", &tm); }
static int cmp_pop(const void *a, const void *b) { double x = ((const Rel *)a)->pop, y = ((const Rel *)b)->pop; return x > y ? -1 : x < y; }
static void assemble(void) {
  qsort(R.r, (size_t)R.n, sizeof *R.r, cmp_pop);
  itemlist_clear(&R.ready); R.ready.items = calloc(MAXR, sizeof(Item)); if (!R.ready.items) return;
  for (int i = 0; i < R.n; i++) if (R.r[i].done == 1) R.ready.items[R.ready.n++] = R.r[i].it;
  fprintf(stderr, "[releases] %d titles on Home\n", R.ready.n); board_invalidate();
}
static void on_ext(int st, const unsigned char *b, size_t len, void *ud) {
  (void)len; unsigned g = (unsigned)((uintptr_t)ud >> 8); int i = (int)((uintptr_t)ud & 0xff); if (g != (R.gen & 0xffffff) || i >= R.n) return;
  cJSON *j = st == 200 ? net_json(b) : NULL; const char *im = js_str(j, "imdb_id", "");
  if (im && !strncmp(im, "tt", 2)) { snprintf(R.r[i].it.id, sizeof R.r[i].it.id, "%s", im); R.r[i].done = 1; } else R.r[i].done = 2;
  net_json_free(j); if (--R.wait == 0) assemble();
}
static void on_list(int st, const unsigned char *b, size_t len, void *ud) {
  (void)len; int tv = (int)((uintptr_t)ud & 1); unsigned g = (unsigned)((uintptr_t)ud >> 1); if (g != (R.gen & 0xffffff)) return;
  cJSON *j = st == 200 ? net_json(b) : NULL; const cJSON *m; int k = 0;
  cJSON_ArrayForEach(m, js_arr(j, "results")) { if (R.n >= MAXR || k >= (tv ? 8 : 16)) break; const char *pp = js_str(m, "poster_path", ""); if (!*pp) continue;
    Rel *r = &R.r[R.n]; memset(r, 0, sizeof *r); r->tm = (int)js_num(m, "id", 0); r->tv = tv; r->pop = js_num(m, "popularity", 0) * (tv ? 0.8 : 1); if (!r->tm) continue;
    snprintf(r->it.type, sizeof r->it.type, "%s", tv ? "series" : "movie"); snprintf(r->it.name, sizeof r->it.name, "%s", js_str(m, tv ? "name" : "title", ""));
    snprintf(r->it.poster, sizeof r->it.poster, "https://image.tmdb.org/t/p/w342%s", pp);
    { const char *bp = js_str(m, "backdrop_path", ""); if (*bp) snprintf(r->it.background, sizeof r->it.background, "https://image.tmdb.org/t/p/w1280%s", bp); }
    snprintf(r->it.description, sizeof r->it.description, "%s", js_str(m, "overview", ""));
    { const char *d = js_str(m, tv ? "first_air_date" : "release_date", ""); if (strlen(d) >= 4) snprintf(r->it.metaLine, sizeof r->it.metaLine, "%.4s", d); }
    R.n++; k++; }
  net_json_free(j);
  if (--R.lists == 0) { R.wait = R.n; if (!R.n) return;
    for (int i = 0; i < R.n; i++) { char p[64]; snprintf(p, sizeof p, "/%s/%d/external_ids", R.r[i].tv ? "tv" : "movie", R.r[i].tm);
      if (!tmdb_request(p, on_ext, (void *)(uintptr_t)(((R.gen & 0xffffff) << 8) | (unsigned)i))) { R.r[i].done = 2; if (--R.wait == 0) assemble(); } } }
}
static void ask(void) {
  if (!tmdb_has_key()) return;
  R.askedAt = app_now(); R.gen++; R.n = 0; R.lists = 2; char a[16], z[16], a2[16], p[400]; day(a, sizeof a, 45); day(z, sizeof z, 0); day(a2, sizeof a2, 30);
  snprintf(p, sizeof p, "/discover/movie?language=%s&region=BR&watch_region=BR&sort_by=popularity.desc&release_date.gte=%s&release_date.lte=%s&with_release_type=4|5&vote_count.gte=5", i18n_lang(), a, z);
  if (!tmdb_request(p, on_list, (void *)(uintptr_t)(((R.gen & 0xffffff) << 1) | 0))) R.lists--;
  snprintf(p, sizeof p, "/discover/tv?language=%s&sort_by=popularity.desc&first_air_date.gte=%s&first_air_date.lte=%s&with_type=2|4&vote_count.gte=3", i18n_lang(), a2, z);
  if (!tmdb_request(p, on_list, (void *)(uintptr_t)(((R.gen & 0xffffff) << 1) | 1))) R.lists--;
  fprintf(stderr, "[releases] asking TMDB\n");
}
int releases_items(ItemList *out) {
  memset(out, 0, sizeof *out);
  if (!strcmp(store_get("homeReleases", "true"), "false")) return 0;
  if (!R.askedAt || app_now() - R.askedAt > 6 * 3600) ask();
  if (!R.ready.n) return 0;
  out->items = calloc((size_t)R.ready.n, sizeof(Item)); if (!out->items) return 0;
  memcpy(out->items, R.ready.items, (size_t)R.ready.n * sizeof(Item)); out->n = R.ready.n; return out->n;
}
