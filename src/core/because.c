#include "core/because.h"
#include "core/tmdb.h"
#include "core/i18n.h"
#include "core/store.h"
#include "core/json.h"
#include "core/app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* etapa-87: the last title finished (store "becauseOf", written by the player) -> TMDB's id -> its recommendations ->
   each one's IMDb id (the app's ids) -> the row, those already in the library left out */
#define MAXB 20
typedef struct { Item it; int tm, tv, done; } Rec;
static struct { Rec r[MAXB]; int n, wait; unsigned gen; double askedAt; char forId[24], name[200]; ItemList ready; } B;
extern void board_invalidate(void);

static void assemble(void) {
  itemlist_clear(&B.ready); B.ready.items = calloc(MAXB, sizeof(Item)); if (!B.ready.items) return;
  for (int i = 0; i < B.n; i++) if (B.r[i].done == 1 && !lib_has(B.r[i].it.id)) B.ready.items[B.ready.n++] = B.r[i].it;
  fprintf(stderr, "[because] %d titles for %s\n", B.ready.n, B.name); board_invalidate();
}
static void on_ext(int st, const unsigned char *b, size_t len, void *ud) {
  (void)len; unsigned g = (unsigned)((uintptr_t)ud >> 8); int i = (int)((uintptr_t)ud & 0xff); if (g != (B.gen & 0xffffff) || i >= B.n) return;
  cJSON *j = st == 200 ? net_json(b) : NULL; const char *im = js_str(j, "imdb_id", "");
  if (im && !strncmp(im, "tt", 2)) { snprintf(B.r[i].it.id, sizeof B.r[i].it.id, "%s", im); B.r[i].done = 1; } else B.r[i].done = 2;
  net_json_free(j); if (--B.wait == 0) assemble();
}
static void on_recs(int st, const unsigned char *b, size_t len, void *ud) {
  (void)len; int tv = (int)((uintptr_t)ud & 1); unsigned g = (unsigned)((uintptr_t)ud >> 1); if (g != (B.gen & 0xffffff)) return;
  cJSON *j = st == 200 ? net_json(b) : NULL; const cJSON *m; B.n = 0;
  cJSON_ArrayForEach(m, js_arr(j, "results")) { if (B.n >= MAXB) break; const char *pp = js_str(m, "poster_path", ""); if (!*pp) continue;
    Rec *r = &B.r[B.n]; memset(r, 0, sizeof *r); r->tm = (int)js_num(m, "id", 0); if (!r->tm) continue;
    int isTv = !strcmp(js_str(m, "media_type", tv ? "tv" : "movie"), "tv"); r->tv = isTv;
    snprintf(r->it.type, sizeof r->it.type, "%s", isTv ? "series" : "movie"); snprintf(r->it.name, sizeof r->it.name, "%s", js_str(m, isTv ? "name" : "title", ""));
    snprintf(r->it.poster, sizeof r->it.poster, "https://image.tmdb.org/t/p/w342%s", pp);
    { const char *bp = js_str(m, "backdrop_path", ""); if (*bp) snprintf(r->it.background, sizeof r->it.background, "https://image.tmdb.org/t/p/w1280%s", bp); }
    snprintf(r->it.description, sizeof r->it.description, "%s", js_str(m, "overview", ""));
    { const char *d = js_str(m, isTv ? "first_air_date" : "release_date", ""); if (strlen(d) >= 4) snprintf(r->it.metaLine, sizeof r->it.metaLine, "%.4s", d); }
    B.n++; }
  net_json_free(j);
  B.wait = B.n; if (!B.n) return;
  for (int i = 0; i < B.n; i++) { char p[64]; snprintf(p, sizeof p, "/%s/%d/external_ids", B.r[i].tv ? "tv" : "movie", B.r[i].tm);
    if (!tmdb_request(p, on_ext, (void *)(uintptr_t)(((B.gen & 0xffffff) << 8) | (unsigned)i))) { B.r[i].done = 2; if (--B.wait == 0) assemble(); } }
}
static void on_find(int st, const unsigned char *b, size_t len, void *ud) {
  (void)len; unsigned g = (unsigned)(uintptr_t)ud; if (g != (B.gen & 0xffffff)) return;
  cJSON *j = st == 200 ? net_json(b) : NULL; int tv = 0; const cJSON *hit = cJSON_GetArrayItem(js_arr(j, "movie_results"), 0);
  if (!hit) { hit = cJSON_GetArrayItem(js_arr(j, "tv_results"), 0); tv = 1; }
  int id = hit ? (int)js_num(hit, "id", 0) : 0; net_json_free(j); if (!id) return;
  char p[160]; snprintf(p, sizeof p, "/%s/%d/recommendations?language=%s", tv ? "tv" : "movie", id, i18n_lang());
  tmdb_request(p, on_recs, (void *)(uintptr_t)(((B.gen & 0xffffff) << 1) | (unsigned)tv));
}
static void ask(void) {
  const cJSON *o = store_node("becauseOf"); const char *id = js_str(o, "id", ""); if (!tmdb_has_key() || strncmp(id, "tt", 2)) return;
  B.askedAt = app_now(); B.gen++; B.n = 0; snprintf(B.forId, sizeof B.forId, "%s", id); snprintf(B.name, sizeof B.name, "%s", js_str(o, "name", ""));
  itemlist_clear(&B.ready);
  char p[96]; snprintf(p, sizeof p, "/find/%s?external_source=imdb_id", id);
  tmdb_request(p, on_find, (void *)(uintptr_t)(B.gen & 0xffffff));
}
void because_changed(void) { B.askedAt = 0; }
int because_items(ItemList *out, char *title, size_t cap) {
  memset(out, 0, sizeof *out); if (title && cap) title[0] = 0;
  if (!strcmp(store_get("homeBecause", "true"), "false")) return 0;
  const char *id = js_str(store_node("becauseOf"), "id", "");
  if (!B.askedAt || strcmp(id, B.forId) || app_now() - B.askedAt > 12 * 3600) ask();
  if (!B.ready.n) return 0;
  out->items = calloc((size_t)B.ready.n, sizeof(Item)); if (!out->items) return 0;
  memcpy(out->items, B.ready.items, (size_t)B.ready.n * sizeof(Item)); out->n = B.ready.n;
  if (title && cap) snprintf(title, cap, "%s", T1("Because you watched {t}", "t", B.name));
  return out->n;
}
