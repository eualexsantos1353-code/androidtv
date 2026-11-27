#include "core/kids.h"
#include "core/tmdb.h"
#include "core/i18n.h"
#include "core/json.h"
#include "core/app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAXK 20
static const char *TITLES[KIDS_ROWS] = { "Films for children", "Cartoons", "Kids series" };
typedef struct { Item it; int tm, done; } K;
static struct { K r[KIDS_ROWS][MAXK]; int n[KIDS_ROWS], wait[KIDS_ROWS]; ItemList ready[KIDS_ROWS]; int asked; } S;
extern void board_invalidate(void);
const char *kids_title(int i) { return TITLES[i]; }
static void done_row(int row) {
  itemlist_clear(&S.ready[row]); S.ready[row].items = calloc(MAXK, sizeof(Item)); if (!S.ready[row].items) return;
  for (int i = 0; i < S.n[row]; i++) if (S.r[row][i].done == 1) S.ready[row].items[S.ready[row].n++] = S.r[row][i].it;
  fprintf(stderr, "[kids] %s: %d\n", TITLES[row], S.ready[row].n); board_invalidate();
}
static void on_ext(int st, const unsigned char *b, size_t len, void *ud) {
  (void)len; int row = (int)((uintptr_t)ud >> 8), i = (int)((uintptr_t)ud & 0xff); cJSON *j = st == 200 ? net_json(b) : NULL; const char *im = js_str(j, "imdb_id", "");
  if (im && !strncmp(im, "tt", 2)) { snprintf(S.r[row][i].it.id, sizeof S.r[row][i].it.id, "%s", im); S.r[row][i].done = 1; } else S.r[row][i].done = 2;
  net_json_free(j); if (--S.wait[row] == 0) done_row(row);
}
static void on_list(int st, const unsigned char *b, size_t len, void *ud) {
  (void)len; int row = (int)(uintptr_t)ud, tv = row == 2; cJSON *j = st == 200 ? net_json(b) : NULL; const cJSON *m;
  cJSON_ArrayForEach(m, js_arr(j, "results")) { if (S.n[row] >= MAXK) break; const char *pp = js_str(m, "poster_path", ""); int tm = (int)js_num(m, "id", 0); if (!*pp || !tm) continue;
    K *k = &S.r[row][S.n[row]++]; memset(k, 0, sizeof *k); k->tm = tm; snprintf(k->it.type, sizeof k->it.type, "%s", tv ? "series" : "movie"); snprintf(k->it.name, sizeof k->it.name, "%s", js_str(m, tv ? "name" : "title", ""));
    snprintf(k->it.poster, sizeof k->it.poster, "https://image.tmdb.org/t/p/w342%s", pp); { const char *bp = js_str(m, "backdrop_path", ""); if (*bp) snprintf(k->it.background, sizeof k->it.background, "https://image.tmdb.org/t/p/w1280%s", bp); }
    snprintf(k->it.description, sizeof k->it.description, "%s", js_str(m, "overview", "")); }
  net_json_free(j); S.wait[row] = S.n[row];
  for (int i = 0; i < S.n[row]; i++) { char p[64]; snprintf(p, sizeof p, "/%s/%d/external_ids", tv ? "tv" : "movie", S.r[row][i].tm);
    if (!tmdb_request(p, on_ext, (void *)(uintptr_t)((row << 8) | i))) { S.r[row][i].done = 2; if (--S.wait[row] == 0) done_row(row); } }
}
static void ask(void) {
  if (S.asked || !tmdb_has_key()) return; S.asked = 1; char p[400]; const char *l = i18n_lang();
  snprintf(p, sizeof p, "/discover/movie?language=%s&sort_by=popularity.desc&certification_country=US&certification.lte=PG&with_genres=10751&vote_count.gte=50", l); tmdb_request(p, on_list, (void *)(uintptr_t)0);
  snprintf(p, sizeof p, "/discover/movie?language=%s&sort_by=popularity.desc&certification_country=US&certification.lte=G&with_genres=16&vote_count.gte=30", l); tmdb_request(p, on_list, (void *)(uintptr_t)1);
  snprintf(p, sizeof p, "/discover/tv?language=%s&sort_by=popularity.desc&with_genres=10762&vote_count.gte=10", l); tmdb_request(p, on_list, (void *)(uintptr_t)2);
}
int kids_items(int i, ItemList *out) {
  memset(out, 0, sizeof *out); ask(); if (i < 0 || i >= KIDS_ROWS || !S.ready[i].n) return 0;
  out->items = calloc((size_t)S.ready[i].n, sizeof(Item)); if (!out->items) return 0; memcpy(out->items, S.ready[i].items, (size_t)S.ready[i].n * sizeof(Item)); out->n = S.ready[i].n; return out->n;
}
