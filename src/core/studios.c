#include "core/studios.h"
#include "core/tmdb.h"
#include "core/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define TITLES 8
typedef struct { char id[24]; int state, n, tv; unsigned tok; char logo[3][200], name[3][80]; } St; /* state: 1 asking, 2 known */
static St T_[TITLES]; static int nextSlot; static unsigned gen, tokN;
unsigned studios_generation(void) { return gen; }
static St *by_tok(unsigned t) { for (int i = 0; i < TITLES; i++) if (T_[i].tok == t && T_[i].state == 1) return &T_[i]; return NULL; }
static void add(St *s, const cJSON *arr, int most) {
  const cJSON *c; int k = 0; cJSON_ArrayForEach(c, arr) { if (s->n >= 3 || k >= most) break; const char *lp = js_str(c, "logo_path", ""); if (!*lp) continue;
    for (int i = 0; i < s->n; i++) if (!strcmp(s->name[i], js_str(c, "name", ""))) goto next;
    snprintf(s->logo[s->n], sizeof s->logo[0], "https://image.tmdb.org/t/p/w185%s", lp); snprintf(s->name[s->n], sizeof s->name[0], "%s", js_str(c, "name", "")); s->n++; k++;
    next:; }
}
static void on_details(int st, const unsigned char *b, size_t len, void *ud) {
  (void)len; St *s = by_tok((unsigned)(uintptr_t)ud); if (!s) return; cJSON *j = st == 200 ? net_json(b) : NULL;
  if (s->tv) add(s, js_arr(j, "networks"), 2); add(s, js_arr(j, "production_companies"), 3);
  s->state = 2; gen++; net_json_free(j);
}
static void on_find(int st, const unsigned char *b, size_t len, void *ud) {
  (void)len; St *s = by_tok((unsigned)(uintptr_t)ud); if (!s) return; cJSON *j = st == 200 ? net_json(b) : NULL;
  const cJSON *r = cJSON_GetArrayItem(js_arr(j, s->tv ? "tv_results" : "movie_results"), 0); int id = (int)js_num(r, "id", 0); net_json_free(j);
  if (!id) { s->state = 2; gen++; return; }
  char p[64]; snprintf(p, sizeof p, "/%s/%d", s->tv ? "tv" : "movie", id); if (!tmdb_request(p, on_details, (void *)(uintptr_t)s->tok)) { s->state = 2; gen++; }
}
int studios_get(const char *type, const char *imdb, const char *logos[3], const char *names[3]) {
  if (!imdb || strncmp(imdb, "tt", 2) || !tmdb_has_key()) return 0;
  char id[24]; snprintf(id, sizeof id, "%s", imdb); char *c = strchr(id, ':'); if (c) *c = 0;
  St *s = NULL; for (int i = 0; i < TITLES; i++) if (T_[i].state && !strcmp(T_[i].id, id)) s = &T_[i];
  if (!s) { s = &T_[nextSlot]; nextSlot = (nextSlot + 1) % TITLES; memset(s, 0, sizeof *s); snprintf(s->id, sizeof s->id, "%s", id); s->tv = type && !strcmp(type, "series"); s->state = 1; s->tok = ++tokN;
    char p[80]; snprintf(p, sizeof p, "/find/%s?external_source=imdb_id", id); if (!tmdb_request(p, on_find, (void *)(uintptr_t)s->tok)) s->state = 2; return 0; }
  if (s->state != 2) return 0;
  for (int i = 0; i < s->n; i++) { logos[i] = s->logo[i]; names[i] = s->name[i]; } return s->n;
}
