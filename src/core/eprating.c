#include "core/eprating.h"
#include "core/net.h"
#include "core/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define IMDB_GQL "https://api.graphql.imdb.com/"
#define SHOWS 6
#define PAGES 8
typedef struct { short s, e, r; } EpR; /* r: the rating x10 */
typedef struct { char id[24]; int state, pages; unsigned tok; EpR *r; int n, cap; } Show; /* state: 0 empty, 1 asking, 2 known, 3 none */
static Show S[SHOWS]; static int nextSlot; static unsigned gen, tokN;
static int form = -1; /* which way of asking for the numbers IMDb answered (two schemas seen) */

unsigned eprating_generation(void) { return gen; }

static void ask(Show *s, const char *after, int f);
static void on_page(int st, const unsigned char *body, size_t len, void *ud) {
  (void)len; unsigned tok = (unsigned)(uintptr_t)ud; Show *s = NULL; for (int i = 0; i < SHOWS; i++) if (S[i].tok == tok && S[i].state == 1) s = &S[i];
  if (!s) return;
  cJSON *j = st == 200 ? net_json(body) : NULL; int f = s->pages >> 8; s->pages &= 255;
  const cJSON *ep = js_obj(js_obj(js_obj(js_obj(j, "data"), "title"), "episodes"), "episodes");
  if (!ep) { /* this way of asking refused: the other, once */
    if (j && js_arr(j, "errors") && form < 0 && f == 0) { net_json_free(j); s->pages = (1 << 8) | s->pages; ask(s, NULL, 1); return; }
    fprintf(stderr, "[eprating] %s: IMDb answered %d, no episodes\n", s->id, st); s->state = s->n ? 2 : 3; gen++; net_json_free(j); return; }
  if (form < 0) form = f;
  const cJSON *e; cJSON_ArrayForEach(e, js_arr(ep, "edges")) {
    const cJSON *nd = js_obj(e, "node"), *sr = js_obj(nd, "series"); int se = -1, en = -1;
    if (f == 0) { const cJSON *x = js_obj(sr, "episodeNumber"); se = (int)js_num(x, "seasonNumber", -1); en = (int)js_num(x, "episodeNumber", -1); }
    else { const cJSON *x = js_obj(sr, "displayableEpisodeNumber"); se = atoi(js_str(js_obj(x, "displayableSeason"), "text", "-1")); en = atoi(js_str(js_obj(x, "episodeNumber"), "text", "-1")); }
    double r = js_num(js_obj(nd, "ratingsSummary"), "aggregateRating", 0);
    if (se < 0 || en < 0 || r <= 0) continue;
    if (s->n == s->cap) { s->cap = s->cap ? s->cap * 2 : 64; s->r = realloc(s->r, s->cap * sizeof *s->r); }
    s->r[s->n++] = (EpR){ (short)se, (short)en, (short)(r * 10 + 0.5) };
  }
  const cJSON *pi = js_obj(ep, "pageInfo"); const cJSON *hn = cJSON_GetObjectItemCaseSensitive(pi, "hasNextPage"); char cur[200]; snprintf(cur, sizeof cur, "%s", js_str(pi, "endCursor", ""));
  net_json_free(j);
  if (cJSON_IsTrue(hn) && *cur && ++s->pages < PAGES) { s->pages |= f << 8; ask(s, cur, f); return; }
  s->state = s->n ? 2 : 3; gen++; fprintf(stderr, "[eprating] %s: %d episodes rated\n", s->id, s->n);
}
static void ask(Show *s, const char *after, int f) {
  char q[900], a[260] = ""; if (after) snprintf(a, sizeof a, ", after: \"%s\"", after);
  snprintf(q, sizeof q, "{ title(id: \"%s\") { episodes { episodes(first: 250%s) { edges { node { ratingsSummary { aggregateRating } series { %s } } } pageInfo { hasNextPage endCursor } } } } }", s->id, a,
    f == 0 ? "episodeNumber { seasonNumber episodeNumber }" : "displayableEpisodeNumber { displayableSeason { text } episodeNumber { text } }");
  cJSON *b = cJSON_CreateObject(); cJSON_AddStringToObject(b, "query", q); char *js = cJSON_PrintUnformatted(b); cJSON_Delete(b); if (!js) return;
  net_post_hdr_t(IMDB_GQL, js, "application/json", "x-imdb-client-name: imdb-web-next\nUser-Agent: Mozilla/5.0", 12000, on_page, (void *)(uintptr_t)s->tok); free(js);
}

float eprating_get(const char *show, int season, int episode) {
  if (!show || strncmp(show, "tt", 2)) return -1;
  char id[24]; snprintf(id, sizeof id, "%s", show); char *c = strchr(id, ':'); if (c) *c = 0;
  Show *s = NULL; for (int i = 0; i < SHOWS; i++) if (S[i].state && !strcmp(S[i].id, id)) s = &S[i];
  if (!s) { s = &S[nextSlot]; nextSlot = (nextSlot + 1) % SHOWS; free(s->r); memset(s, 0, sizeof *s); snprintf(s->id, sizeof s->id, "%s", id); s->state = 1; s->tok = ++tokN; s->pages = (form > 0 ? 1 : 0) << 8; ask(s, NULL, form > 0 ? 1 : 0); return -1; }
  if (s->state != 2) return -1;
  for (int i = 0; i < s->n; i++) if (s->r[i].s == season && s->r[i].e == episode) return s->r[i].r / 10.0f;
  return -1;
}
