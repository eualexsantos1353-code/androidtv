/* Similar: a grid of titles of the same genre. Also a person's films and series (OK on a face on a title page),
   the most notable first, from Wikidata (cast, director, writer, with their IMDb ids; ranked by how many wikis
   cover each). */
#include "screens/similar.h"
#include "core/keys.h"
#include "core/i18n.h"
#include "core/json.h"
#include "ui/gfx.h"
#include "ui/text.h"
#include "ui/widgets.h"
#include "ui/img.h"
#include "screens/sheet.h"
#include <stdio.h>
#include <string.h>
#include "core/hit.h"
#include "core/net.h"
#include "core/coll.h"
#include "core/tmdb.h"
#include "core/traktsync.h"
#include "core/debrid.h"
#include <ctype.h>
#include "core/addons.h"
#include "core/notify.h"
#include "core/store.h"
#include "ui/sound.h"
#include "ui/icon.h"
#include "core/app.h"
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#define GX (G.person ? 708.0f : 62.0f)
#define GTOP (G.person ? 214.0f : 224.0f)
#define COLS (G.person ? 4 : 6) /* a person: the films on the right, their face and story on the left */
static struct { DetailParams p; ItemList items; int idx; char title[300], sub[200]; int loading, empty, person, coll, cloud, cal, bioF, bioOpen, pendTmdb; float bioScroll; unsigned cloudGen; double pendAt; unsigned collVer; char from[16], ckey[24]; double loadT0; struct { float x, y; int shown; Timer1 pop; } ring; Tween gridY; Timer1 fade; Nudge nudge; unsigned gen; } G;
#define MAXPT 48
typedef struct { char id[16], type[8], name[128]; int yr; } PT;
typedef struct { char who[16]; int n; PT t[MAXPT]; } PersonMem;
static PersonMem pmem[8]; static int pmemN;
/* ---- the person's profile (TMDB, NuvioTV's cast page): photo, what they do, born where and when, the story ---- */
static struct { char who[16], name[128], dept[64], born[64], died[64], place[160], photo[300], bio[6000]; int gender, tid, state; unsigned gen; } PI; /* state: 0 none, 1 asking, 2 known */
static void on_pi_en(int st, const unsigned char *b, size_t len, void *ud) { (void)len; if ((unsigned)(uintptr_t)ud != PI.gen) return; cJSON *j = st == 200 ? net_json(b) : NULL;
  if (!PI.bio[0]) snprintf(PI.bio, sizeof PI.bio, "%s", js_str(j, "biography", "")); net_json_free(j); }
static void on_pi(int st, const unsigned char *b, size_t len, void *ud) {
  (void)len; if ((unsigned)(uintptr_t)ud != PI.gen) return; cJSON *j = st == 200 ? net_json(b) : NULL; PI.state = 2;
  if (!j) return;
  snprintf(PI.name, sizeof PI.name, "%s", js_str(j, "name", "")); snprintf(PI.born, sizeof PI.born, "%s", js_str(j, "birthday", "")); snprintf(PI.died, sizeof PI.died, "%s", js_str(j, "deathday", ""));
  snprintf(PI.place, sizeof PI.place, "%s", js_str(j, "place_of_birth", "")); snprintf(PI.dept, sizeof PI.dept, "%s", js_str(j, "known_for_department", "")); PI.gender = (int)js_num(j, "gender", 0);
  const char *pp = js_str(j, "profile_path", ""); if (*pp) snprintf(PI.photo, sizeof PI.photo, "https://image.tmdb.org/t/p/h632%s", pp);
  snprintf(PI.bio, sizeof PI.bio, "%s", js_str(j, "biography", "")); net_json_free(j);
  if (!PI.bio[0]) { char p[64]; snprintf(p, sizeof p, "/person/%d", PI.tid); tmdb_request(p, on_pi_en, (void *)(uintptr_t)PI.gen); } /* none in the app's language: the English one */
}
static void on_pi_find(int st, const unsigned char *b, size_t len, void *ud) {
  (void)len; if ((unsigned)(uintptr_t)ud != PI.gen) return; cJSON *j = st == 200 ? net_json(b) : NULL;
  PI.tid = (int)js_num(cJSON_GetArrayItem(js_arr(j, "person_results"), 0), "id", 0); net_json_free(j);
  if (!PI.tid) { PI.state = 2; return; }
  char p[96], lang[16]; snprintf(lang, sizeof lang, "%s", i18n_lang()); snprintf(p, sizeof p, "/person/%d?language=%s", PI.tid, lang); if (!tmdb_request(p, on_pi, (void *)(uintptr_t)PI.gen)) PI.state = 2;
}
static void pi_load(const char *nm) {
  if (PI.state && !strcmp(PI.who, nm)) return; memset(&PI, 0, sizeof PI); snprintf(PI.who, sizeof PI.who, "%s", nm); PI.gen = G.gen; PI.state = 1;
  char p[80]; snprintf(p, sizeof p, "/find/%s?external_source=imdb_id", nm); if (!tmdb_request(p, on_pi_find, (void *)(uintptr_t)PI.gen)) PI.state = 2;
}
static const char *dept_label(void) {
  const char *d = PI.dept; int f = PI.gender == 1;
  if (!strcmp(d, "Acting")) return T(f ? "Actress" : "Actor"); if (!strcmp(d, "Directing")) return T(f ? "Director (f)" : "Director");
  if (!strcmp(d, "Writing")) return T("Writer"); if (!strcmp(d, "Production")) return T(f ? "Producer (f)" : "Producer"); if (!strcmp(d, "Sound")) return T("Music");
  return ""; }
static void date_txt(const char *ymd, char *out, size_t cap) { /* "1970-10-08" -> "8 out 1970" */
  int y = 0, m = 0, d = 0; out[0] = 0; if (sscanf(ymd, "%d-%d-%d", &y, &m, &d) != 3 || m < 1 || m > 12) return;
  static const char *MO[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
  snprintf(out, cap, "%d %s %d", d, T(MO[m - 1]), y); }
static int age_of(const char *born, const char *died) { int y, m, d, y2, m2, d2; if (sscanf(born, "%d-%d-%d", &y, &m, &d) != 3) return -1;
  if (!died || sscanf(died, "%d-%d-%d", &y2, &m2, &d2) != 3) { time_t t = time(NULL); struct tm tm; localtime_r(&t, &tm); y2 = tm.tm_year + 1900; m2 = tm.tm_mon + 1; d2 = tm.tm_mday; }
  return y2 - y - ((m2 < m || (m2 == m && d2 < d)) ? 1 : 0); }
/* a person's films in tabs: All, Films, Series, and With you (the ones in your Library or watched) */
static Item *PALL; static int nPALL, ptab, ptabF;
static const char *PTAB[4] = { "All", "Movies", "Series", "With you" };
static int ptab_pass(const Item *it, int t) { if (t == 1) return strcmp(it->type, "series") != 0; if (t == 2) return !strcmp(it->type, "series"); if (t == 3) return lib_get(it->id) != NULL; return 1; }
static void ptab_apply(void) {
  itemlist_clear(&G.items); G.items.items = calloc((size_t)(nPALL ? nPALL : 1), sizeof(Item)); G.items.n = 0;
  for (int i = 0; i < nPALL; i++) if (ptab_pass(&PALL[i], ptab)) G.items.items[G.items.n++] = PALL[i];
  G.idx = 0; G.ring.shown = 0; tween_set(&G.gridY, 0); timer_start(&G.fade, 0.25f); }
static void person_show(const PersonMem *m) {
  itemlist_clear(&G.items); G.items.items = calloc(MAXPT, sizeof(Item)); G.items.n = 0;
  for (int i = 0; i < m->n; i++) { Item *it = &G.items.items[G.items.n++]; snprintf(it->id, sizeof it->id, "%s", m->t[i].id); snprintf(it->type, sizeof it->type, "%s", m->t[i].type); snprintf(it->name, sizeof it->name, "%s", m->t[i].name);
    snprintf(it->poster, sizeof it->poster, "https://images.metahub.space/poster/small/%s/img", m->t[i].id);
    if (m->t[i].yr > 0) snprintf(it->metaLine, sizeof it->metaLine, "%d", m->t[i].yr); else it->metaLine[0] = 0; }
  if (m->n) { char bu[200]; snprintf(bu, sizeof bu, "https://images.metahub.space/background/medium/%s/img", m->t[0].id); bg_set(bu, 0, NULL); } /* behind the page: the best known one's scene */
  free(PALL); nPALL = G.items.n; PALL = malloc(sizeof(Item) * (size_t)(nPALL ? nPALL : 1)); if (PALL) memcpy(PALL, G.items.items, sizeof(Item) * (size_t)nPALL); else nPALL = 0; ptab = ptabF = 0;
  G.loading = 0; G.empty = G.items.n == 0; timer_start(&G.fade, 0.3f); G.idx = 0; G.ring.shown = 0;
}
static void on_person(int st, const unsigned char *body, size_t len, void *ud) {
  (void)len; if ((unsigned)(uintptr_t)ud != G.gen || SCR != SCR_SIM) return;
  cJSON *j = st == 200 ? net_json(body) : NULL; const cJSON *b = js_arr(js_obj(j, "results"), "bindings");
  if (!b) { net_json_free(j); G.loading = 0; G.empty = 1; return; }
  PersonMem *m = &pmem[pmemN++ % 8]; memset(m, 0, sizeof *m); snprintf(m->who, sizeof m->who, "%s", G.p.it.id); const cJSON *r;
  cJSON_ArrayForEach(r, b) { if (m->n >= MAXPT) break; const char *tt = js_str(js_obj(r, "imdb"), "value", ""), *nm = js_str(js_obj(r, "lab"), "value", ""); if (!*tt || !*nm) continue;
    PT *t = &m->t[m->n++]; snprintf(t->id, sizeof t->id, "%s", tt); snprintf(t->name, sizeof t->name, "%s", nm); t->yr = atoi(js_str(js_obj(r, "yr"), "value", "0")); snprintf(t->type, sizeof t->type, "%s", !strcmp(js_str(js_obj(r, "s"), "value", ""), "1") ? "series" : "movie"); }
  net_json_free(j); person_show(m);
}
static void person_load(void) {
  for (int i = 0; i < 8; i++) if (pmem[i].who[0] && !strcmp(pmem[i].who, G.p.it.id)) { person_show(&pmem[i]); return; } /* kept in memory */
  const char *id = G.p.it.id; if (strncmp(id, "nm", 2)) { G.loading = 0; G.empty = 1; return; } for (const char *q = id + 2; *q; q++) if (*q < '0' || *q > '9') { G.loading = 0; G.empty = 1; return; }
  static const char *FILM = "wd:Q11424 wd:Q24869 wd:Q506240 wd:Q93204 wd:Q202866 wd:Q29168811 wd:Q17123180", *SER = "wd:Q5398426 wd:Q1259759 wd:Q526877 wd:Q581714";
  char q[1600]; snprintf(q, sizeof q, "SELECT ?imdb (SAMPLE(?l) AS ?lab) (MIN(?yy) AS ?yr) (MAX(?ser) AS ?s) (MAX(?sl) AS ?pop) WHERE { ?p wdt:P345 \"%s\" . "
    "{ ?f wdt:P161 ?p } UNION { ?f wdt:P57 ?p } UNION { ?f wdt:P58 ?p } ?f wdt:P345 ?imdb . FILTER(STRSTARTS(?imdb, \"tt\")) "
    "{ ?f wdt:P31 ?t . VALUES ?t { %s } BIND(0 AS ?ser) } UNION { ?f wdt:P31 ?t . VALUES ?t { %s } BIND(1 AS ?ser) } "
    "?f wikibase:sitelinks ?sl . OPTIONAL { ?f wdt:P577 ?d . BIND(YEAR(?d) AS ?yy) } OPTIONAL { ?f rdfs:label ?l . FILTER(LANG(?l) = \"en\") } } GROUP BY ?imdb ORDER BY DESC(?pop) LIMIT 60", id, FILM, SER);
  char u[5200] = "https://query.wikidata.org/sparql?format=json&query="; size_t o = strlen(u);
  for (const char *s = q; *s && o + 4 < sizeof u; s++) { unsigned char c = *s; if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '!' || c == '~' || c == '*' || c == '\'' || c == '(' || c == ')') u[o++] = c; else o += snprintf(u + o, sizeof u - o, "%%%02X", c); }
  u[o] = 0; net_get(u, on_person, (void *)(uintptr_t)G.gen);
}
static void on_cat(ItemList *l, int cached, void *ud) {
  (void)cached; if ((unsigned)(uintptr_t)ud != G.gen || SCR != SCR_SIM) { itemlist_free(l); return; }
  itemlist_clear(&G.items); G.items.items = calloc(30, sizeof(Item)); G.items.n = 0;
  for (int i = 0; i < l->n && G.items.n < 30; i++) { if (!strcmp(l->items[i].id, G.p.it.id)) continue; G.items.items[G.items.n++] = l->items[i]; }
  itemlist_free(l); G.loading = 0; G.empty = G.items.n == 0; timer_start(&G.fade, 0.3f); G.idx = 0; G.ring.shown = 0;
}
static void on_meta(cJSON *m, void *ud) {
  if ((unsigned)(uintptr_t)ud != G.gen || SCR != SCR_SIM) return;
  const char *nm = js_str(m, "name", G.p.it.name); if (*nm) snprintf(G.title, sizeof G.title, "%s", T1("Similar to {name}", "name", nm));
  const cJSON *gs = js_arr(m, "_genresEn"); if (!gs || !cJSON_GetArraySize(gs)) gs = js_arr(m, "genres"); /* Cinemeta's English genre, also when the page shows it translated */
  const char *g = gs && cJSON_GetArraySize(gs) && cJSON_IsString(cJSON_GetArrayItem(gs, 0)) ? cJSON_GetArrayItem(gs, 0)->valuestring : "";
  for (const char *q = g; *q; q++) if ((unsigned char)*q >= 0x80) { g = ""; break; } /* a translated name: the top titles instead */
  char u[512], ge[128] = ""; if (*g) { const char *s = g; char *d = ge; while (*s && d < ge + 120) { if ((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '-') *d++ = *s; else { snprintf(d, 4, "%%%02X", (unsigned char)*s); d += 3; } s++; } *d = 0; }
  if (ge[0]) snprintf(u, sizeof u, CINEMETA "/catalog/%s/top/genre=%s.json", G.p.it.type, ge); else snprintf(u, sizeof u, CINEMETA "/catalog/%s/top.json", G.p.it.type);
  sb_catalog_url(u, G.p.it.type, 31, on_cat, (void *)(uintptr_t)G.gen);
}
/* a collection (from a film's page or the Library's Collections): its films in the order they came out, the one it
   was opened from marked, the focus on the first not watched yet, over the collection's own backdrop */
static void coll_show(void) {
  if (!G.coll || G.collVer == coll_version()) return; G.collVer = coll_version(); int was = !G.loading; /* (shown already: its films' ids filled in place, the focus kept) */
  const Coll *c = coll_get(G.ckey); if (!c) { if (app_now() - G.loadT0 > 30) { G.loading = 0; G.empty = 1; } return; }
  itemlist_clear(&G.items); G.items.items = calloc((size_t)(c->n ? c->n : 1), sizeof(Item)); G.items.n = 0; int first = -1;
  for (int i = 0; i < c->n; i++) { Item *it = &G.items.items[G.items.n++]; if (c->part[i].id[0]) snprintf(it->id, sizeof it->id, "%s", c->part[i].id); else snprintf(it->id, sizeof it->id, "tmdb:%d", c->part[i].tmdb); /* (its IMDb id on its way) */ snprintf(it->type, sizeof it->type, "%s", c->part[i].type[0] ? c->part[i].type : "movie"); snprintf(it->name, sizeof it->name, "%s", c->part[i].name); snprintf(it->poster, sizeof it->poster, "%s", c->part[i].poster);
    if (c->part[i].year) snprintf(it->metaLine, sizeof it->metaLine, "%d", c->part[i].year); if (first < 0 && !lib_item_watched(it)) first = i; }
  if (c->name[0]) snprintf(G.title, sizeof G.title, "%s", c->name);
  { char n[16]; snprintf(n, sizeof n, "%d", c->n); int y0 = c->n ? c->part[0].year : 0, y1 = c->n ? c->part[c->n - 1].year : 0; char yr[24] = ""; if (y0 && y1 && y1 != y0) snprintf(yr, sizeof yr, "  ·  %d\xe2\x80\x93%d", y0, y1); else if (y0) snprintf(yr, sizeof yr, "  ·  %d", y0);
    int nf = 0; for (int i = 0; i < c->n; i++) if (strcmp(c->part[i].type, "series")) nf++; /* (films alone: "8 films"; with shows: "6 titles") */
    snprintf(G.sub, sizeof G.sub, "%s%s", nf == c->n ? T1("{n} films", "n", n) : T1("{n} titles", "n", n), yr); }
  if (was) { if (G.idx >= G.items.n) G.idx = G.items.n ? G.items.n - 1 : 0; return; }
  G.loading = 0; G.empty = G.items.n == 0; timer_start(&G.fade, 0.3f); G.idx = first >= 0 ? first : 0; G.ring.shown = 0;
  if (c->backdrop[0]) bg_set(c->backdrop, 0, c->poster);
}
/* ---- the debrid cloud (Settings > Keys > My cloud): its video files, each played straight from it ---- */
static void cloud_name(const char *fn, char *out, size_t cap) { /* "The.Movie.2019.1080p.mkv" -> "The Movie 2019 1080p" */
  snprintf(out, cap, "%s", fn); char *d = strrchr(out, '.'); if (d && strlen(d) <= 5) *d = 0; for (char *q = out; *q; q++) if (*q == '.' || *q == '_') *q = ' '; }
/* a cloud file named as a title ("Show.S02E05.1080p", "Film.2019.2160p"): found on Cinemeta, so its tile has the
   poster and the title, and it plays as that title (its place kept, resumed, Trakt told) */
typedef struct { char q[160], type[16], id[32], vid[64], poster[300], name[200]; int se, ep, state; } CMeta; /* state 1 asking, 2 known */
static CMeta *CMx; static int nCMx; static unsigned cmGen;
static void cloud_parse(const char *fn, CMeta *m) {
  char b[256]; snprintf(b, sizeof b, "%s", fn); char *d = strrchr(b, '.'); if (d && strlen(d) <= 5) *d = 0; for (char *q = b; *q; q++) if (*q == '.' || *q == '_' || *q == '[' || *q == ']' || *q == '(' || *q == ')') *q = ' ';
  m->se = m->ep = 0; char out[160] = ""; size_t o = 0; char *save = NULL; int yearSeen = 0;
  for (char *t = strtok_r(b, " ", &save); t; t = strtok_r(NULL, " ", &save)) { int s2, e2; char lw[64]; size_t k = 0; for (; t[k] && k < sizeof lw - 1; k++) lw[k] = (char)tolower((unsigned char)t[k]); lw[k] = 0;
    if ((sscanf(lw, "s%de%d", &s2, &e2) == 2 || sscanf(lw, "%dx%d", &s2, &e2) == 2) && s2 > 0 && s2 < 100) { m->se = s2; m->ep = e2; break; }
    if (strlen(lw) == 4 && (!strncmp(lw, "19", 2) || !strncmp(lw, "20", 2)) && isdigit((unsigned char)lw[2]) && isdigit((unsigned char)lw[3]) && o) { yearSeen = 1; break; }
    if (strstr(lw, "1080p") || strstr(lw, "720p") || strstr(lw, "2160p") || !strcmp(lw, "4k") || strstr(lw, "webrip") || strstr(lw, "bluray") || strstr(lw, "web-dl") || strstr(lw, "x264") || strstr(lw, "x265") || strstr(lw, "hevc") || !strcmp(lw, "dual") || !strcmp(lw, "multi")) break;
    if (o + strlen(t) + 2 < sizeof out) o += (size_t)snprintf(out + o, sizeof out - o, "%s%s", o ? " " : "", t); }
  (void)yearSeen; snprintf(m->q, sizeof m->q, "%s", out); snprintf(m->type, sizeof m->type, "%s", m->se ? "series" : "movie"); }
static void on_cm(int st, const unsigned char *b, size_t len, void *ud) {
  (void)len; unsigned u = (unsigned)(uintptr_t)ud; int i = (int)(u & 0xfff); if ((u >> 12) != (cmGen & 0xfffff) || i >= nCMx) return; CMeta *m = &CMx[i]; cJSON *j = st == 200 ? net_json(b) : NULL;
  const cJSON *r = cJSON_GetArrayItem(js_arr(j, "metas"), 0); const char *id = js_str(r, "id", js_str(r, "imdb_id", ""));
  if (!strncmp(id, "tt", 2)) { snprintf(m->id, sizeof m->id, "%s", id); snprintf(m->poster, sizeof m->poster, "%s", js_str(r, "poster", "")); snprintf(m->name, sizeof m->name, "%s", js_str(r, "name", ""));
    if (m->se) snprintf(m->vid, sizeof m->vid, "%s:%d:%d", id, m->se, m->ep); else snprintf(m->vid, sizeof m->vid, "%s", id); }
  m->state = 2; net_json_free(j); }
static void cloud_match(const DebFile *f, int n) {
  free(CMx); nCMx = n < 4095 ? n : 4095; CMx = calloc((size_t)(nCMx ? nCMx : 1), sizeof *CMx); cmGen++;
  for (int i = 0; i < nCMx; i++) { cloud_parse(f[i].name, &CMx[i]); if (!CMx[i].q[0] || i >= 60) continue; char enc[400], u[600]; addon_urlenc(CMx[i].q, enc, sizeof enc);
    snprintf(u, sizeof u, "https://v3-cinemeta.strem.io/catalog/%s/top/search=%s.json", CMx[i].type, enc); CMx[i].state = 1; net_get(u, on_cm, (void *)(uintptr_t)(((cmGen & 0xfffff) << 12) | (unsigned)i)); } }
static const CMeta *cloud_meta(const Item *it) { int i = atoi(it->id + 4); return !strncmp(it->id, "deb:", 4) && i >= 0 && i < nCMx && CMx[i].id[0] ? &CMx[i] : NULL; }
static void cloud_poll(void) {
  if (!G.cloud) return; const DebFile *f; int n = deb_list(&f); if (n < 0) return; if (!G.loading && G.cloudGen == deb_generation()) return;
  G.cloudGen = deb_generation(); itemlist_clear(&G.items); G.items.items = calloc((size_t)(n ? n : 1), sizeof(Item)); G.items.n = 0;
  for (int i = 0; i < n; i++) { Item *it = &G.items.items[G.items.n++]; snprintf(it->id, sizeof it->id, "deb:%d", i); snprintf(it->type, sizeof it->type, "other"); cloud_name(f[i].name, it->name, sizeof it->name);
    if (f[i].size > 0) { double gb = f[i].size / 1073741824.0; if (gb >= 1) snprintf(it->metaLine, sizeof it->metaLine, "%.1f GB", gb); else snprintf(it->metaLine, sizeof it->metaLine, "%.0f MB", f[i].size / 1048576.0); } }
  cloud_match(f, n);
  { char nb[16]; snprintf(nb, sizeof nb, "%d", n); snprintf(G.sub, sizeof G.sub, "%s", T1("{n} videos", "n", nb)); }
  G.loading = 0; G.empty = n == 0; timer_start(&G.fade, 0.3f); G.idx = 0; G.ring.shown = 0; }
static void cloud_tile(const Item *it, float x, float y, int f) {
  float w = CARD_W, h = CARD_H; const CMeta *m = cloud_meta(it);
  if (m && m->poster[0]) { float asp; GLuint t = img_get(m->poster, (int)w, (int)h, &asp); /* the title's poster, its name and episode, how far it was watched */
    if (t) gfx_image(t, x, y, w, h, CARD_R, WHITE_A(255), 0, 0, 1, 1); else gfx_rrect_v(x, y, w, h, CARD_R, RGB(34, 32, 44), RGB(18, 17, 24));
    gfx_rrect_v(x, y + h * 0.55f, w, h * 0.45f, CARD_R, BLACK_A(0), BLACK_A(220));
    char ln[64] = ""; if (m->se) snprintf(ln, sizeof ln, "T%d:E%d", m->se, m->ep); text_draw(m->name, x + 16, y + h - (ln[0] ? 86 : 58), 23, 800, WHITE_A(250), 0, w - 32, ALIGN_LEFT);
    if (ln[0]) text_draw(ln, x + 16, y + h - 56, 20, 700, WHITE_A(200), 0, w - 32, ALIGN_LEFT);
    if (it->metaLine[0]) text_draw(it->metaLine, x + w - 16, y + 14, 18, 700, WHITE_A(220), 0, 0, ALIGN_RIGHT);
    { Item li; memset(&li, 0, sizeof li); snprintf(li.id, sizeof li.id, "%s", m->id); snprintf(li.type, sizeof li.type, "%s", m->type); snprintf(li.videoId, sizeof li.videoId, "%s", m->vid); CwInfo ci; memset(&ci, 0, sizeof ci);
      if (lib_cw_info(&li, &ci) && ci.pct > 0.01f && (!m->se || !strcmp(ci.vid, m->vid))) { float pw = w - 32; gfx_rrect(x + 16, y + h - 24, pw, 6, 3, WHITE_A(60)); gfx_rrect(x + 16, y + h - 24, pw * (ci.pct > 1 ? 1 : ci.pct), 6, 3, ACC2 | 0xff000000u); } }
    gfx_ring(x, y, w, h, CARD_R, f ? 3 : 1.2f, f ? (ACC2 | 0xff000000u) : WHITE_A(26)); return; }
  gfx_rrect_v(x, y, w, h, CARD_R, RGB(34, 32, 44), RGB(18, 17, 24)); gfx_ring(x, y, w, h, CARD_R, f ? 3 : 1.2f, f ? (ACC2 | 0xff000000u) : WHITE_A(26));
  icon_draw("p_film", x + w / 2 - 34, y + 70, 68, WHITE_A(120));
  text_draw_wrapped(it->name, x + 18, y + 180, w - 36, 5, 22, 700, WHITE_A(235), 0, ALIGN_LEFT, 28);
  if (it->metaLine[0]) text_draw(it->metaLine, x + 18, y + h - 44, 20, 600, WHITE_A(150), 0, w - 36, ALIGN_LEFT); }

/* ---- the Calendar: a month of days on the left, the focused day's episodes on the right (the series in the
   Library, from the addons' calendar catalogs, core/notify). Arrows move the day (across months too), OK or Right
   into the day's list, OK there opens the series */
static struct { int zone, idx, got, flt, fidx; long day; double pollAt; Tween listY; } C; /* day: yyyymmdd focused; zone 1: the list, 2: the filters */
static long day_of(time_t t) { struct tm tm; localtime_r(&t, &tm); return (tm.tm_year + 1900) * 10000L + (tm.tm_mon + 1) * 100 + tm.tm_mday; }
static time_t time_of(long d) { struct tm tm; memset(&tm, 0, sizeof tm); tm.tm_year = (int)(d / 10000) - 1900; tm.tm_mon = (int)(d / 100 % 100) - 1; tm.tm_mday = (int)(d % 100); tm.tm_hour = 12; tm.tm_isdst = -1; return mktime(&tm); }
static long day_add(long d, int n) { return day_of(time_of(d) + (time_t)n * 86400); }
static int wday_of(long d) { time_t t = time_of(d); struct tm tm; localtime_r(&t, &tm); return tm.tm_wday; }

/* the filters (as the ASTRAL TV Agenda's): the followed series' episodes, then from TMDB for the month shown: series
   premiering, series coming back with a new season, films coming out */
static int cmp_cal(const void *a, const void *b);
static const char *FLT[4] = { "Upcoming episodes", "Premieres of the week", "New seasons", "Movies" };
typedef struct { ItemList l; int ym, state, pend; unsigned tok; } FData; /* ym: the month asked (yyyymm); state 1 asking, 2 known */
static FData FD[4]; static unsigned fTok;
static ItemList *cl(void) { return C.flt ? &FD[C.flt].l : &G.items; }
static void fd_add(FData *d, int tv, int tid, const char *name, const char *poster, const char *date, const char *line) {
  struct tm tm; memset(&tm, 0, sizeof tm); if (sscanf(date, "%d-%d-%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday) != 3) return; tm.tm_year -= 1900; tm.tm_mon--; tm.tm_hour = 12; tm.tm_isdst = -1;
  char id[40]; snprintf(id, sizeof id, "tm:%c:%d", tv ? 't' : 'm', tid); for (int i = 0; i < d->l.n; i++) if (!strcmp(d->l.items[i].id, id) && !strcmp(d->l.items[i].metaLine, line)) return;
  Item *m = realloc(d->l.items, (size_t)(d->l.n + 1) * sizeof(Item)); if (!m) return; d->l.items = m; Item *it = &m[d->l.n++]; memset(it, 0, sizeof *it);
  snprintf(it->id, sizeof it->id, "%s", id); snprintf(it->type, sizeof it->type, "%s", tv ? "series" : "movie"); snprintf(it->name, sizeof it->name, "%s", name);
  if (*poster) snprintf(it->poster, sizeof it->poster, "https://image.tmdb.org/t/p/w342%s", poster); it->airAt = (double)mktime(&tm); it->ep = -1; snprintf(it->metaLine, sizeof it->metaLine, "%s", line); }
static void fd_done(FData *d) { if (--d->pend <= 0) { d->state = 2; qsort(d->l.items, (size_t)d->l.n, sizeof(Item), cmp_cal); } }
static void on_disc(int st, const unsigned char *b, size_t len, void *ud) { /* premieres and films: one page of discover */
  (void)len; unsigned u = (unsigned)(uintptr_t)ud; int f = (int)(u & 3); FData *d = &FD[f]; if ((u >> 2) != d->tok) return; cJSON *j = st == 200 ? net_json(b) : NULL; const cJSON *r;
  cJSON_ArrayForEach(r, js_arr(j, "results")) { int tv = f == 1; if (tv) fd_add(d, 1, (int)js_num(r, "id", 0), js_str(r, "name", ""), js_str(r, "poster_path", ""), js_str(r, "first_air_date", ""), T("Series premiere"));
    else fd_add(d, 0, (int)js_num(r, "id", 0), js_str(r, "title", ""), js_str(r, "poster_path", ""), js_str(r, "release_date", ""), T("Film")); }
  net_json_free(j); fd_done(d); }
static void on_show(int st, const unsigned char *b, size_t len, void *ud) { /* new seasons: a show's next or last episode, when it is a season's first */
  (void)len; unsigned u = (unsigned)(uintptr_t)ud; FData *d = &FD[2]; if ((u >> 2) != d->tok) return; cJSON *j = st == 200 ? net_json(b) : NULL;
  const char *ks[2] = { "next_episode_to_air", "last_episode_to_air" };
  for (int k = 0; k < 2; k++) { const cJSON *e = js_obj(j, ks[k]); int sn = (int)js_num(e, "season_number", 0); if (!e || js_num(e, "episode_number", 0) != 1 || sn < 2) continue;
    const char *ad = js_str(e, "air_date", ""); int y, m; if (sscanf(ad, "%d-%d", &y, &m) != 2 || y * 100 + m != d->ym) continue;
    char ln[64], sn2[8]; snprintf(sn2, sizeof sn2, "%d", sn); snprintf(ln, sizeof ln, "%s", T1("Season {n}", "n", sn2)); fd_add(d, 1, (int)js_num(j, "id", 0), js_str(j, "name", ""), js_str(j, "poster_path", ""), ad, ln); }
  net_json_free(j); fd_done(d); }
static void on_airing(int st, const unsigned char *b, size_t len, void *ud) { /* new seasons: the shows airing that month, then each one's details */
  (void)len; unsigned u = (unsigned)(uintptr_t)ud; FData *d = &FD[2]; if ((u >> 2) != d->tok) return; cJSON *j = st == 200 ? net_json(b) : NULL; const cJSON *r;
  cJSON_ArrayForEach(r, js_arr(j, "results")) { char p[80]; snprintf(p, sizeof p, "/tv/%d?language=%s", (int)js_num(r, "id", 0), i18n_lang()); d->pend++; if (!tmdb_request(p, on_show, (void *)(uintptr_t)(d->tok << 2 | 2))) d->pend--; }
  net_json_free(j); fd_done(d); }
static void fd_ask(int f) {
  if (!f) return; int ym = (int)(C.day / 100); FData *d = &FD[f]; if (d->ym == ym && d->state) return;
  itemlist_clear(&d->l); d->ym = ym; d->state = 1; d->pend = 0; d->tok = ++fTok & 0x3fffffff;
  if (!tmdb_has_key()) { d->state = 2; return; }
  int y = ym / 100, m = ym % 100, last = m == 2 ? (y % 4 ? 28 : 29) : (m == 4 || m == 6 || m == 9 || m == 11) ? 30 : 31; char a[16], z[16]; snprintf(a, sizeof a, "%04d-%02d-01", y, m); snprintf(z, sizeof z, "%04d-%02d-%02d", y, m, last);
  for (int pg = 1; pg <= (f == 2 ? 2 : 3); pg++) { char p[400];
    if (f == 1) snprintf(p, sizeof p, "/discover/tv?language=%s&sort_by=popularity.desc&first_air_date.gte=%s&first_air_date.lte=%s&with_type=2|4&page=%d", i18n_lang(), a, z, pg);
    else if (f == 2) snprintf(p, sizeof p, "/discover/tv?language=%s&sort_by=popularity.desc&air_date.gte=%s&air_date.lte=%s&page=%d", i18n_lang(), a, z, pg);
    else snprintf(p, sizeof p, "/discover/movie?language=%s&region=BR&sort_by=popularity.desc&primary_release_date.gte=%s&primary_release_date.lte=%s&with_release_type=2|3|4&page=%d", i18n_lang(), a, z, pg);
    d->pend++; if (!tmdb_request(p, f == 2 ? on_airing : on_disc, (void *)(uintptr_t)(d->tok << 2 | (unsigned)f))) d->pend--; }
  if (d->pend <= 0) d->state = 2; }
/* a TMDB title picked: its IMDb id first, then its page */
static void on_ext(int st, const unsigned char *b, size_t len, void *ud) {
  (void)len; Item *it = (Item *)ud; cJSON *j = st == 200 ? net_json(b) : NULL; const char *im = js_str(j, "imdb_id", "");
  if (*im && SCR == SCR_SIM && G.cal) { DetailParams p; memset(&p, 0, sizeof p); p.it = *it; snprintf(p.it.id, sizeof p.it.id, "%s", im); p.it.metaLine[0] = 0; app_go(SCR_DETAIL, &p, 0); }
  else if (!*im) toast(T("Nothing found."), 1600, NULL);
  net_json_free(j); free(it); }
static int cal_open_tm(const Item *it) {
  if (strncmp(it->id, "tm:", 3)) return 0; Item *c = malloc(sizeof *c); if (!c) return 1; *c = *it; char p[64]; snprintf(p, sizeof p, "/%s/%s/external_ids", it->id[3] == 't' ? "tv" : "movie", it->id + 5);
  if (!tmdb_request(p, on_ext, c)) free(c); return 1; }
static int day_eps(long d, int *first) { ItemList *L = cl(); int n = 0; *first = -1; for (int i = 0; i < L->n; i++) if (day_of((time_t)L->items[i].airAt) == d) { if (*first < 0) *first = i; n++; } return n; }
/* besides what the addons' calendar catalogs said: each followed series' own episode list (its metadata, asked once
   as the Calendar opens), so a library the catalogs don't cover still fills the month */
static Item *CX; static int nCX, capCX, cxPend, cxAsked; static unsigned cxGen;
static int cmp_cal(const void *a, const void *b) { double x = ((const Item *)a)->airAt, y = ((const Item *)b)->airAt; if (x != y) return x < y ? -1 : 1; return strcmp(((const Item *)a)->name, ((const Item *)b)->name); }
static void cal_load(void) {
  itemlist_clear(&G.items); int r = notify_calendar(&G.items); if (r < 0) r = 0;
  if (nCX) { Item *m = realloc(G.items.items, (size_t)(G.items.n + nCX) * sizeof(Item)); if (m) { G.items.items = m;
      for (int i = 0; i < nCX; i++) { int dup = 0; for (int k = 0; k < G.items.n; k++) if (!strcmp(G.items.items[k].videoId, CX[i].videoId)) { dup = 1; break; } if (!dup) G.items.items[G.items.n++] = CX[i]; }
      qsort(G.items.items, (size_t)G.items.n, sizeof(Item), cmp_cal); } }
  C.got = 1; G.loading = !G.items.n && (cxPend > 0 || notify_busy()) && app_now() - G.loadT0 < 20; /* (20 s at most: then what came) */ G.empty = !G.items.n && !G.loading; C.pollAt = app_now() + 1; }
static void on_cal_meta(cJSON *m, void *ud) {
  if ((unsigned)(uintptr_t)ud != cxGen) return; cxPend--; if (!m) return;
  time_t now = time(NULL); const cJSON *v; const char *sid = js_str(m, "id", "");
  cJSON_ArrayForEach(v, js_arr(m, "videos")) { const char *rel = js_str(v, "released", js_str(v, "firstAired", "")); struct tm tm; memset(&tm, 0, sizeof tm); int hh = 0, mi = 0;
    if (sscanf(rel, "%d-%d-%dT%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &hh, &mi) < 3) continue; tm.tm_year -= 1900; tm.tm_mon--; tm.tm_hour = hh; tm.tm_min = mi; time_t r = timegm(&tm);
    int se = (int)js_num(v, "season", 0), en = (int)js_num(v, "episode", js_num(v, "number", 0)); if (se <= 0 || r < now - 35 * 86400 || r > now + 92 * 86400) continue;
    if (nCX == capCX) { capCX = capCX ? capCX * 2 : 64; Item *n2 = realloc(CX, (size_t)capCX * sizeof(Item)); if (!n2) return; CX = n2; }
    Item *it = &CX[nCX++]; memset(it, 0, sizeof *it); sb_fill_item(it, m, "series"); snprintf(it->id, sizeof it->id, "%s", sid); snprintf(it->type, sizeof it->type, "series");
    snprintf(it->videoId, sizeof it->videoId, "%s", js_str(v, "id", "")); it->airAt = (double)r; it->ep = en; snprintf(it->metaLine, sizeof it->metaLine, "T%d:E%d", se, en); snprintf(it->epName, sizeof it->epName, "%s", js_str(v, "name", js_str(v, "title", ""))); }
  cal_load(); }
static void cal_more(void) {
  nCX = 0; cxGen++; cxPend = 0; cJSON *lib = store_node("lib"); const cJSON *it; int k = 0; if (!cJSON_IsObject(lib)) return;
  time_t now = time(NULL);
  cJSON_ArrayForEach(it, lib) { if (k >= 80) break; if (strcmp(js_str(it, "type", ""), "series")) continue; const char *id = js_str(it, "_id", ""); if (!*id) continue;
    if (js_bool(it, "removed", 0) || js_bool(it, "temp", 0)) { /* not in the Library: a series watched lately (Continue Watching) still counts */
      const char *lw = js_str(js_obj(it, "state"), "lastWatched", ""); struct tm tm; memset(&tm, 0, sizeof tm); if (sscanf(lw, "%d-%d-%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday) != 3) continue;
      tm.tm_year -= 1900; tm.tm_mon--; if (now - timegm(&tm) > 200 * 86400) continue; }
    k++; cxPend++; sb_meta("series", id, on_cal_meta, (void *)(uintptr_t)cxGen); }
  { static char tids[150][24]; int nt = tsync_shows(tids, 150), kt = 0; /* etapa-80: the shows Trakt follows (its calendar, its watchlist), those not counted above */
    for (int i = 0; i < nt && kt < 100; i++) { const cJSON *l = lib_get(tids[i]); if (l && !js_bool(l, "removed", 0) && !js_bool(l, "temp", 0)) continue; kt++; k++; cxPend++; sb_meta("series", tids[i], on_cal_meta, (void *)(uintptr_t)cxGen); }
    if (kt) fprintf(stderr, "[calendar] %d more from Trakt\n", kt); }
  cxAsked = k; fprintf(stderr, "[calendar] asking the episode lists of %d series\n", k); }
static void cal_enter(void) { tsync_shows_refresh(); memset(&C, 0, sizeof C); G.loadT0 = app_now(); C.day = day_of(time(NULL)); snprintf(G.title, sizeof G.title, "%s", T("Calendar")); cal_more(); cal_load(); }
static void cal_tick(float dt) { tween_tick(&C.listY, dt); { static double seen; double at = js_num(store_node("traktShows"), "at", 0); if (at != seen) { int first = seen == 0 && !C.got; seen = at; if (!first && at) { cal_more(); } } } /* (Trakt's shows came: asked too) */ if ((!C.got || G.loading || notify_busy()) && app_now() > C.pollAt) cal_load(); }
static const Item *cal_focused(void) { if (C.zone != 1) return NULL; int f; int n = day_eps(C.day, &f); return n && C.idx < n ? &cl()->items[f + C.idx] : NULL; }
static int cal_key(int k) {
  int f, n = day_eps(C.day, &f); long d0 = C.day;
  if (C.zone == 2) { /* the filters: each applies as the focus lands on it */
    if (k == K_LEFT || k == K_RIGHT) { int t = C.fidx + (k == K_RIGHT ? 1 : -1); if (t < 0 || t > 3) { nudge_start(&G.nudge, k); return 1; } C.fidx = C.flt = t; fd_ask(t); sound_focus(); }
    else if (k == K_DOWN || k == K_OK) { C.zone = 0; sound_focus(); }
    else if (k == K_UP) nudge_start(&G.nudge, k);
    else if (k == K_BACK) return 0;
    return 1; }
  if (C.zone == 1) {
    if (k == K_UP) { if (C.idx > 0) C.idx--; else nudge_start(&G.nudge, k); }
    else if (k == K_DOWN) { if (C.idx < n - 1) C.idx++; else nudge_start(&G.nudge, k); }
    else if (k == K_LEFT || k == K_BACK) { C.zone = 0; }
    tween_to(&C.listY, C.idx > 3 ? -(C.idx - 3) * 150.0f : 0, 0.24f, EASE_OUT); sound_focus(); return 1; }
  if (k == K_LEFT) C.day = day_add(C.day, -1); else if (k == K_RIGHT) { if (wday_of(C.day) == 6 && n) { C.zone = 1; C.idx = 0; tween_set(&C.listY, 0); sound_focus(); return 1; } C.day = day_add(C.day, 1); }
  else if (k == K_UP) { long up = day_add(C.day, -7); if (up / 100 != C.day / 100) { C.zone = 2; C.fidx = C.flt; sound_focus(); return 1; } C.day = up; } /* (the month's first week: up to the filters) */
  else if (k == K_DOWN) C.day = day_add(C.day, 7);
  else if (k == K_OK) { if (n) { C.zone = 1; C.idx = 0; tween_set(&C.listY, 0); sound_focus(); } else nudge_start(&G.nudge, k); return 1; }
  else if (k == K_BACK) return 0; else return 1;
  { int span = C.flt ? 366 : 92; long lo = day_add(day_of(time(NULL)), -span), hi = day_add(day_of(time(NULL)), span); if (C.day < lo || C.day > hi) { C.day = d0; nudge_start(&G.nudge, k); return 1; } }
  if (C.day / 100 != d0 / 100) fd_ask(C.flt); /* (another month: the filter's titles for it) */
  if (C.day != d0) sound_focus(); return 1; }
static void cal_draw(void) {
  static const char *M[12] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
  static const char *W[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" }, *WS[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
  uint32_t ac = ACC2 | 0xff000000u; long today = day_of(time(NULL)); int y = (int)(C.day / 10000), m = (int)(C.day / 100 % 100);
  text_draw(G.title, 80, 118, 52, 800, WHITE_A(255), -0.8f, 0, ALIGN_LEFT);
  { char my[64]; snprintf(my, sizeof my, "%s %d", T(M[m - 1]), y); text_draw(my, 80 + 7 * 116 - 8, 134, 36, 700, WHITE_A(200), 0, 0, ALIGN_RIGHT); }
  /* the filters */
  { float px = 80, py = 176, ph = 52; for (int i = 0; i < 4; i++) { const char *lb = T(FLT[i]); float w = text_width(lb, 22, 700, 0) + 40; int sel = C.flt == i, fo = C.zone == 2 && C.fidx == i;
      if (fo) gfx_rrect(px, py, w, ph, ph / 2, WHITE_A(250)); else if (sel) gfx_rrect(px, py, w, ph, ph / 2, WHITE_A(52)); else gfx_ring(px, py, w, ph, ph / 2, 1.5f, WHITE_A(70));
      text_draw(lb, px + 20, py + (ph - text_line_height(22)) / 2, 22, 700, fo ? RGB(16, 16, 20) : WHITE_A(sel ? 255 : 190), 0, 0, ALIGN_LEFT);
      if (sel && !fo) gfx_rrect(px + w / 2 - 14, py + ph + 6, 28, 4, 2, ac); px += w + 12; } }
  /* the month */
  float gx = 80, gy = 306, cw = 108, chh = 96, gap = 8; long first = (long)(y * 10000L + m * 100 + 1); int w0 = wday_of(first);
  for (int c = 0; c < 7; c++) text_draw(T(WS[c]), gx + c * (cw + gap) + cw / 2, gy - 44, 24, 700, WHITE_A(c == 0 || c == 6 ? 130 : 170), 0, 0, ALIGN_CENTER);
  long d = day_add(first, -w0);
  for (int r = 0; r < 6; r++) for (int c = 0; c < 7; c++, d = day_add(d, 1)) {
    float x = gx + c * (cw + gap), yy = gy + r * (chh + gap); int inM = (int)(d / 100 % 100) == m, fo = d == C.day, ff; int ne = day_eps(d, &ff);
    if (!inM && r == 5 && c == 0) break;
    if (fo) gfx_rrect(x, yy, cw, chh, 18, C.zone == 0 ? WHITE_A(250) : WHITE_A(60)); else gfx_rrect(x, yy, cw, chh, 18, WHITE_A(inM ? 16 : 6));
    if (d == today && !fo) gfx_ring(x, yy, cw, chh, 18, 3, ac);
    char dn[8]; snprintf(dn, sizeof dn, "%d", (int)(d % 100)); uint32_t fg = fo && C.zone == 0 ? RGB(16, 16, 20) : WHITE_A(inM ? 235 : 90);
    text_draw(dn, x + 14, yy + 10, 30, 700, fg, 0, 0, ALIGN_LEFT);
    if (ne) { /* the first poster, small, and how many */
      const Item *it = &cl()->items[ff]; float aspect; GLuint t = it->poster[0] ? img_get(it->poster, 60, 90, &aspect) : 0;
      if (t) gfx_image(t, x + cw - 48, yy + chh - 66, 38, 56, 6, WHITE_A(inM ? 255 : 120), 0, 0, 1, 1); else gfx_rrect(x + cw - 48, yy + chh - 66, 38, 56, 6, col_alpha(ac, 0.6f));
      { if (ne > 1) { char nb[8]; snprintf(nb, sizeof nb, "%d", ne); float bw = text_width(nb, 20, 800, 0) + 18; uint32_t dc = fo && C.zone == 0 ? ac : col_alpha(ac, inM ? 0.9f : 0.45f); gfx_rrect(x + 12, yy + chh - 36, bw, 26, 13, dc); text_draw(nb, x + 12 + bw / 2, yy + chh - 36 + (26 - text_line_height(20)) / 2, 20, 800, WHITE_A(inM ? 255 : 150), 0, 0, ALIGN_CENTER); } } /* etapa-81: how many that day, one small number (no row of dots) */ } /* a dot for each title that day (four at most) */
    }
  /* the day */
  float rx = 960, rw = SB_W - 80 - rx, ry = 150;
  gfx_rrect(rx - 30, ry - 30, rw + 60, SB_H - ry - 30, 30, BLACK_A(90)); gfx_ring(rx - 30, ry - 30, rw + 60, SB_H - ry - 30, 30, 1.2f, WHITE_A(22));
  { char dn[8], hd[160]; snprintf(dn, sizeof dn, "%d", (int)(C.day % 100)); const char *ds = T2("{weekday}, {month} {day}", "month", T(M[m - 1]), "day", dn); const char *wp = strstr(ds, "{weekday}");
    if (wp) snprintf(hd, sizeof hd, "%.*s%s%s", (int)(wp - ds), ds, T(W[wday_of(C.day)]), wp + 9); else snprintf(hd, sizeof hd, "%s", ds);
    if (C.day == today) { char h2[200]; snprintf(h2, sizeof h2, "%s  \xc2\xb7  %s", T("Today"), hd); snprintf(hd, sizeof hd, "%s", h2); }
    text_draw(hd, rx, ry, 40, 800, WHITE_A(255), -0.4f, rw, ALIGN_LEFT); }
  int f, n = day_eps(C.day, &f); float ly = ry + 80, lh = SB_H - 60 - ly;
  if (C.flt) { if (FD[C.flt].state == 1 && !n) text_draw(T("Getting the calendar…"), rx, ly + 20, 30, 600, WHITE_A(160), 0, rw, ALIGN_LEFT); else if (!tmdb_has_key()) text_draw_wrapped(T("Add a TMDB key under Keys"), rx, ly + 20, rw, 3, 30, 600, WHITE_A(160), 0, ALIGN_LEFT, 40); else if (!n) text_draw(T("Nothing on this day"), rx, ly + 20, 30, 600, WHITE_A(150), 0, rw, ALIGN_LEFT); }
  else if (G.loading && !G.items.n) text_draw(T("Getting the calendar…"), rx, ly + 20, 30, 600, WHITE_A(160), 0, rw, ALIGN_LEFT);
  else if (!G.items.n) { char cn[16]; snprintf(cn, sizeof cn, "%d", cxAsked); text_draw_wrapped(cxAsked ? T1("None of your {n} series has an episode from a month ago to three months ahead", "n", cn) : T("Add series to your Library to see their episodes here"), rx, ly + 20, rw, 3, 30, 600, WHITE_A(160), 0, ALIGN_LEFT, 40); }
  else if (!n) text_draw(T("No episodes on this day"), rx, ly + 20, 30, 600, WHITE_A(150), 0, rw, ALIGN_LEFT);
  gfx_clip_push(rx - 24, ly - 6, rw + 48, lh + 12);
  for (int i = 0; i < n; i++) { const Item *it = &cl()->items[f + i]; float yy = ly + i * 150 + C.listY.v; if (yy + 150 < ly || yy > ly + lh) continue; int fo = C.zone == 1 && i == C.idx;
    if (fo) { gfx_rrect(rx - 16, yy, rw + 32, 138, 20, WHITE_A(28)); gfx_ring(rx - 16, yy, rw + 32, 138, 20, 3, ac); }
    float aspect; GLuint t = it->poster[0] ? img_get(it->poster, 120, 180, &aspect) : 0; if (t) gfx_image(t, rx, yy + 9, 80, 120, 10, WHITE_A(255), 0, 0, 1, 1); else gfx_rrect(rx, yy + 9, 80, 120, 10, WHITE_A(30));
    text_draw(it->name, rx + 104, yy + 16, 34, 800, WHITE_A(255), -0.3f, rw - 120, ALIGN_LEFT);
    char ln[360]; if (it->epName[0]) snprintf(ln, sizeof ln, "%s  \xc2\xb7  %s", it->metaLine, it->epName); else snprintf(ln, sizeof ln, "%s", it->metaLine);
    text_draw(ln, rx + 104, yy + 62, 28, 600, WHITE_A(200), 0, rw - 120, ALIGN_LEFT);
    { char tm2[32]; time_t at = (time_t)it->airAt; struct tm lt; localtime_r(&at, &lt); snprintf(tm2, sizeof tm2, "%02d:%02d", lt.tm_hour, lt.tm_min);
      const char *st = at <= time(NULL) ? T("Out") : T("Coming"); char l3[96]; if (C.flt) snprintf(l3, sizeof l3, "%s", st); else snprintf(l3, sizeof l3, "%s  \xc2\xb7  %s", st, tm2); text_draw(l3, rx + 104, yy + 100, 24, 600, at <= time(NULL) ? col_alpha(ac, 1) : WHITE_A(150), 0, rw - 120, ALIGN_LEFT); } }
  gfx_clip_pop();
  { float hy = SB_H - 52, hx = 80; hx += key_hint(hx, hy, K_OK, T(C.zone ? "Open" : "Episodes"), WHITE_A(170), 0) + 30; key_hint(hx, hy, K_BACK, T("Back"), WHITE_A(170), 0); }
}
static void enter(const void *pp) {
  if (pp) G.p = *(const DetailParams *)pp; G.gen++; itemlist_clear(&G.items); G.idx = 0; G.loading = 1; G.loadT0 = app_now(); G.empty = 0; G.ring.shown = 0; tween_set(&G.gridY, 0);
  G.person = !strcmp(G.p.it.type, "person"); G.coll = !strcmp(G.p.it.type, "collection"); G.cloud = !strcmp(G.p.it.type, "cloud"); G.cal = !strcmp(G.p.it.type, "calendar"); G.bioF = G.bioOpen = 0; G.sub[0] = 0;
  if (G.cal) { cal_enter(); return; }
  if (G.cloud) { snprintf(G.title, sizeof G.title, "%s \xc2\xb7 %s", T("My cloud"), deb_service_name()); deb_refresh(); G.cloudGen = 0; cloud_poll(); return; }
  if (G.coll) { snprintf(G.ckey, sizeof G.ckey, "%s", G.p.it.id + 5); G.loadT0 = app_now(); G.collVer = 0; snprintf(G.from, sizeof G.from, "%s", G.p.it.videoId); snprintf(G.title, sizeof G.title, "%s", G.p.it.name[0] ? G.p.it.name : T("Collection")); coll_show(); return; }

  if (G.person) { snprintf(G.title, sizeof G.title, "%s", G.p.it.name); person_load(); pi_load(G.p.it.id); return; }
  snprintf(G.title, sizeof G.title, "%s", T("Similar"));
  sb_meta(G.p.it.type, G.p.it.id, on_meta, (void *)(uintptr_t)G.gen);
}
static const Item *focused(void) { if (G.cal) return cal_focused(); if (G.bioF || G.bioOpen || ptabF) return NULL; /* (on the biography: OK is its own, not the film's) */ return G.idx < G.items.n ? &G.items.items[G.idx] : NULL; }
static void open_it(const Item *it) { if (G.cal && cal_open_tm(it)) return; if (!strncmp(it->id, "deb:", 4)) { const CMeta *m = cloud_meta(it); if (m) deb_play_as(atoi(it->id + 4), m->type, m->id, m->vid); else deb_play(atoi(it->id + 4)); return; } if (!strncmp(it->id, "tmdb:", 5)) { G.pendTmdb = atoi(it->id + 5); G.pendAt = app_now(); return; } /* (its IMDb id on its way: opened when it comes, see tick) */
  DetailParams p; memset(&p, 0, sizeof p); p.it = *it; app_go(SCR_DETAIL, &p, 0); }
static int key(int k) {
  if (G.cal) return cal_key(k);
  if (G.bioOpen) { if (k == K_DOWN) G.bioScroll += 120; else if (k == K_UP) { G.bioScroll -= 120; if (G.bioScroll < 0) G.bioScroll = 0; } else if (k == K_BACK || k == K_OK) G.bioOpen = 0; return 1; } /* the whole story, read */
  if (G.bioF) { if (k == K_RIGHT || k == K_DOWN) G.bioF = 0; else if (k == K_OK) { G.bioOpen = 1; G.bioScroll = 0; } else if (k == K_UP) { G.bioF = 0; topnav_focus(); } else if (k == K_BACK) return 0; return 1; }
  if (G.person && ptabF) { /* the tabs over the films: each applies as the focus lands on it */
    if (k == K_LEFT || k == K_RIGHT) { int t = ptab + (k == K_RIGHT ? 1 : -1); if (t < 0) { if (PI.bio[0]) { ptabF = 0; G.bioF = 1; } else nudge_start(&G.nudge, k); return 1; } if (t > 3) { nudge_start(&G.nudge, k); return 1; } ptab = t; ptab_apply(); }
    else if (k == K_DOWN || k == K_OK) { if (G.items.n) ptabF = 0; else nudge_start(&G.nudge, k); }
    else if (k == K_UP) { ptabF = 0; topnav_focus(); }
    else if (k == K_BACK) return 0;
    return 1; }
  if (G.person && PI.bio[0] && k == K_LEFT && G.idx % COLS == 0) { G.bioF = 1; return 1; } /* the first column: left onto the biography */
  if (G.person && !G.items.n && nPALL) { if (k == K_UP) ptabF = 1; return k != K_BACK; } /* (a tab with nothing in it: back up to the tabs) */
  if (!G.items.n) { if (k == K_UP) topnav_focus(); return k != K_BACK; } int i0 = G.idx;
  if (k == K_RIGHT) G.idx = G.idx + 1 < G.items.n ? G.idx + 1 : G.idx;
  else if (k == K_LEFT) { if (G.idx > 0) G.idx--; }
  else if (k == K_DOWN) G.idx = G.idx + COLS < G.items.n ? G.idx + COLS : (G.items.n - 1 > G.idx && G.idx / COLS < (G.items.n - 1) / COLS ? G.items.n - 1 : G.idx);
  else if (k == K_UP) { if (G.idx - COLS >= 0) G.idx -= COLS; else if (G.person && nPALL) { ptabF = 1; return 1; } else { topnav_focus(); return 1; } } /* (a person: up from the first row onto the biography, then up again to the tabs) */
  else if (k == K_OK) { open_it(&G.items.items[G.idx]); return 1; }
  else if (k == K_BACK) return 0;
  if (G.idx == i0) nudge_start(&G.nudge, k);
  return 1;
}
#define ROWH (CARD_H + (G.person ? 62.0f : 26.0f)) /* a person: the year and kind under each */
/* the text art of the titles around the focus, for their pages (drawn there at 520x175): ready when one opens */
static void logos_ahead(void) { static double at0; double now = app_now(); if (now - at0 < 0.5) return; at0 = now;
  for (int i = G.idx - COLS; i <= G.idx + COLS; i++) { if (i < 0 || i >= G.items.n) continue; char lu[512]; sb_logo_of(&G.items.items[i], lu, sizeof lu); if (lu[0]) img_prefetch_pri(lu, 520, 175); } }
static void tick(float dt) {
  if (G.cal) { cal_tick(dt); nudge_tick(&G.nudge, dt); return; }
  coll_show(); cloud_poll(); logos_ahead();
  if (G.pendTmdb) { if (app_now() - G.pendAt > 15) G.pendTmdb = 0; else for (int i = 0; i < G.items.n; i++) { const Coll *c = coll_get(G.ckey); if (!c) break; for (int k = 0; k < c->n; k++) if (c->part[k].tmdb == G.pendTmdb && c->part[k].id[0]) { Item it = G.items.items[i]; for (int q = 0; q < G.items.n; q++) if (!strcmp(G.items.items[q].id, c->part[k].id)) it = G.items.items[q]; G.pendTmdb = 0; open_it(&it); return; } break; } } /* (a film picked before its id came) */
  timer_tick(&G.ring.pop, dt); tween_tick(&G.gridY, dt); timer_tick(&G.fade, dt); nudge_tick(&G.nudge, dt);
  int row = G.idx / COLS; float y = G.person ? row * ROWH : row > 1 ? (row - 1) * ROWH : 0; /* (a person: the focused row at the top, its year line in view) */ tween_to(&G.gridY, -y, 0.3f, EASE_OUT);
  /* the ring jumps to the card and lands with a small pop (never glides across the others) */
  if (G.items.n) { int rtl = app_rtl(); float cx = GX + (G.idx % COLS) * (CARD_W + CARD_GAP), cy = 16 + row * ROWH; if (rtl) cx = SB_W - cx - CARD_W;
    if (!G.ring.shown || G.ring.x != cx || G.ring.y != cy) { static double last; int pop = !G.ring.shown || focus_pops(&last); G.ring.x = cx; G.ring.y = cy; G.ring.shown = 1; if (pop) timer_start(&G.ring.pop, 0.2f); else G.ring.pop.on = 0; } } /* held: it jumps, lit */
}
static int origin(float *x, float *y) { if (G.cal || topNav.active || G.bioF || ptabF || G.idx >= G.items.n) return 0; int row = G.idx / COLS; float cx = GX + (G.idx % COLS) * (CARD_W + CARD_GAP); if (app_rtl()) cx = SB_W - cx - CARD_W; *x = cx + CARD_W / 2; *y = GTOP + G.gridY.v + 16 + row * ROWH + CARD_H / 2; return 1; }
static void sim_focus(int i, int b) { (void)b; int was = topNav.active; topNav.active = 0; if (was || G.idx != i) G.idx = i; }
static void person_panel(void) {
  float x = 62, y = 124, ph = 236;
  /* the face, rounded */
  if (PI.photo[0]) { GLuint t = img_get_shaped(PI.photo, (int)ph, (int)ph, 34, 2); gfx_shadow(x, y + 10, ph, ph, 34, 30, BLACK_A(120)); if (t) gfx_image(t, x, y, ph, ph, 0, WHITE_A((int)(255 * gfx_tex_fade(t))), 0, 0, 1, 1); else gfx_rrect(x, y, ph, ph, 34, WHITE_A(14)); gfx_ring(x, y, ph, ph, 34, 1.5f, WHITE_A(36)); }
  else { gfx_rrect(x, y, ph, ph, 34, WHITE_A(14)); char ini[8] = { G.title[0], 0 }; text_draw(ini, x + ph / 2, y + ph / 2 - text_line_height(80) / 2, 80, 800, WHITE_A(120), 0, 0, ALIGN_CENTER); }
  float nx = x + ph + 32, nw = 640 - (nx - x); const char *nm = PI.name[0] ? PI.name : G.title;
  int nl = text_draw_wrapped(nm, nx, y + 8, nw, 2, 54, 800, WHITE_A(255), -1.0f, ALIGN_LEFT, 58); float dy = y + 8 + nl * 58 + 8;
  const char *dl = dept_label(); if (*dl) text_draw(dl, nx, dy, 26, 500, WHITE_A(170), 0, nw, ALIGN_LEFT);
  /* the facts card */
  float cy = y + ph + 34, cw = 600, rowH = 52; int nr = 0; char v[3][200]; const char *ic[3], *lb[3];
  if (PI.born[0]) { char d[64]; date_txt(PI.born, d, sizeof d); int a = age_of(PI.born, PI.died[0] ? PI.died : NULL); char an[16]; snprintf(an, sizeof an, "%d", a);
    if (PI.died[0]) { char dd[64]; date_txt(PI.died, dd, sizeof dd); snprintf(v[nr], 200, "%s \xe2\x80\x93 %s (%s)", d, dd, T1("{n} years", "n", an)); } else if (a >= 0) snprintf(v[nr], 200, "%s (%s)", d, T1("{n} years", "n", an)); else snprintf(v[nr], 200, "%s", d);
    ic[nr] = "p_cake"; lb[nr] = T("Born"); nr++; }
  if (PI.place[0]) { snprintf(v[nr], 200, "%s", PI.place); ic[nr] = "d_country"; lb[nr] = T("Birthplace"); nr++; }
  if (nPALL) { int y0 = 0; for (int i = 0; i < nPALL; i++) { int yy = atoi(PALL[i].metaLine); if (yy > 0 && (!y0 || yy < y0)) y0 = yy; } char n[16], ys[16]; snprintf(n, sizeof n, "%d", nPALL); snprintf(ys, sizeof ys, "%d", y0);
    if (y0) snprintf(v[nr], 200, "%s  \xc2\xb7  %s", T1("{n} titles", "n", n), T1("since {y}", "y", ys)); else snprintf(v[nr], 200, "%s", T1("{n} titles", "n", n)); ic[nr] = "p_film"; lb[nr] = T("Credits"); nr++; }
  if (nr) { float ch = 28 + nr * rowH + 12; gfx_rrect(x, cy, cw, ch, 26, BLACK_A(110)); gfx_ring(x, cy, cw, ch, 26, 1.2f, WHITE_A(26));
    for (int i = 0; i < nr; i++) { float ry = cy + 20 + i * rowH, lh = text_line_height(24);
      icon_draw(ic[i], x + 28, ry + lh / 2 - 13, 26, WHITE_A(150)); text_draw(lb[i], x + 68, ry, 24, 500, WHITE_A(170), 0, 0, ALIGN_LEFT);
      text_draw(v[i], x + 230, ry, 24, 600, WHITE_A(235), 0, cw - 250, ALIGN_LEFT); }
    cy += ch + 26; }
  /* the story */
  if (PI.bio[0] || PI.state == 1) { float bh = SB_H - 40 - cy; if (bh > 160) { gfx_rrect(x, cy, cw, bh, 26, BLACK_A(110)); gfx_ring(x, cy, cw, bh, 26, 1.2f, WHITE_A(26));
      if (G.bioF) { gfx_rrect(x, cy, cw, bh, 26, WHITE_A(20)); gfx_ring(x, cy, cw, bh, 26, 4, ACC2 | 0xff000000u); }
      text_draw(T("Biography"), x + 30, cy + 24, 30, 700, WHITE_A(255), 0, 0, ALIGN_LEFT);
      if (PI.bio[0]) key_hint(x + cw - 30 - key_hint(-9999, 0, G.bioF ? K_OK : K_LEFT, T("Read more"), 0, 1), cy + 28, G.bioF ? K_OK : K_LEFT, T("Read more"), WHITE_A(G.bioF ? 230 : 130), 1);
      int lines = (int)((bh - 24 - 50 - 28) / 38); if (lines > 0 && PI.bio[0]) text_draw_wrapped(PI.bio, x + 30, cy + 24 + 54, cw - 60, lines, 25, 400, WHITE_A(214), 0, ALIGN_LEFT, 38);
      else if (lines > 0) text_draw("\xe2\x80\xa6", x + 30, cy + 24 + 54, 25, 400, WHITE_A(150), 0, 0, ALIGN_LEFT); } }
}
static void draw(void) {
  if (G.cal) { gfx_offset_push(nudge_x(&G.nudge), nudge_y(&G.nudge)); cal_draw(); gfx_offset_pop(); return; }
  int rtl = app_rtl();
  if (G.person) { person_panel(); float tw = text_draw(T("Filmography"), GX, 128, 40, 800, WHITE_A(255), -0.6f, 0, ALIGN_LEFT);
    if (nPALL) { float px = GX + tw + 28, py = 128, ph = 46; uint32_t ac2 = ACC2 | 0xff000000u; /* the tabs */
      for (int t = 0; t < 4; t++) { int cnt = 0; for (int i = 0; i < nPALL; i++) if (ptab_pass(&PALL[i], t)) cnt++; char lb[64]; snprintf(lb, sizeof lb, "%s  %d", T(PTAB[t]), cnt);
        float w = text_width(lb, 20, 700, 0) + 32; int sel = ptab == t, fo = sel && ptabF;
        if (fo) gfx_rrect(px, py, w, ph, ph / 2, WHITE_A(250)); else if (sel) gfx_rrect(px, py, w, ph, ph / 2, WHITE_A(50)); else gfx_ring(px, py, w, ph, ph / 2, 1.5f, WHITE_A(60));
        text_draw(lb, px + 16, py + (ph - text_line_height(20)) / 2, 20, 700, fo ? RGB(16, 16, 20) : WHITE_A(sel ? 255 : 185), 0, 0, ALIGN_LEFT); if (sel && !fo) gfx_rrect(px + w / 2 - 12, py + ph + 5, 24, 4, 2, ac2); px += w + 10; } }
    if (G.person && !G.items.n && nPALL && !G.loading) text_draw(T(ptab == 3 ? "You haven't watched anything with this person yet" : "Nothing here yet"), GX, GTOP + 40, 26, 600, WHITE_A(170), 0, 0, ALIGN_LEFT); }
  else text_draw(G.title, rtl ? SB_W - 48 : 48, 120, 44, 800, WHITE_A(255), -0.8f, SB_W - 96, rtl ? ALIGN_RIGHT : ALIGN_LEFT);
  if (G.sub[0]) text_draw(G.sub, rtl ? SB_W - 48 : 48, 120 + 58, 24, 600, WHITE_A(180), 0, SB_W - 96, rtl ? ALIGN_RIGHT : ALIGN_LEFT); /* (a collection: how many films, which years) */
  float top = GTOP; gfx_clip_push(G.person ? GX - 30 : 34, top - 28, G.person ? SB_W - GX + 30 : SB_W - 34, SB_H - top + 28); /* (from 28 above the grid: the first line's focused card grows 9% and has its outline; cut at the grid's top, its top edge was lost) */
  float fa = G.fade.on ? 1 - powf(1 - timer_x(&G.fade), 2) : 1; gfx_opacity_push(fa);
  gfx_offset_push(nudge_x(&G.nudge), nudge_y(&G.nudge) + top + G.gridY.v);
  if (G.loading) { /* loading: a thin line under the title, a short accent stroke sweeping along it */
    double t = app_now() - G.loadT0; float in = (float)((t - 0.15) / 0.3); in = in < 0 ? 0 : in > 1 ? 1 : in;
    if (in > 0) { float W = 360, x0 = rtl ? SB_W - 48 - 40 - W : 48 + 40, ph = (float)fmod(t, 1.4) / 1.4f, e = ph * ph * (3 - 2 * ph), sw = W * 0.32f, sx = x0 - sw + (W + sw) * e;
      gfx_opacity_push(in); gfx_rrect(x0, 52, W, 3, 1.5f, WHITE_A(22));
      float l = sx < x0 ? x0 : sx, r = sx + sw > x0 + W ? x0 + W : sx + sw; if (r - l > 1) gfx_rrect(l, 52, r - l, 3, 1.5f, ACC); gfx_opacity_pop(); } } /* the stroke enters and leaves at the line's ends */
  if (G.empty && !G.loading) text_draw(T("Nothing found."), rtl ? SB_W - 48 - 40 : 48 + 40, 40, 27, 400, WHITE_A(191), 0, 0,
 rtl ? ALIGN_RIGHT : ALIGN_LEFT);
  /* the cards; the focused one last, over its neighbours as it grows */
  int fi = !topNav.active && !G.bioF && !G.bioOpen && !ptabF && G.idx < G.items.n ? G.idx : -1; float hp = app_hold_progress();
  for (int pass = 0; pass < 2; pass++) for (int i = 0; i < G.items.n; i++) { if ((i == fi) != pass) continue;
    int row = i / COLS; float y = 16 + row * ROWH; if (y + G.gridY.v > SB_H - top || y + CARD_H + G.gridY.v < 0) continue;
    float x = GX + (i % COLS) * (CARD_W + CARD_GAP); if (rtl) x = SB_W - x - CARD_W; unsigned long long lk = lift_key(&G, i);
    lift_begin(lk, pass, pass ? hp : 0, x, y, CARD_W, CARD_H, CARD_R);
    if (G.cloud) cloud_tile(&G.items.items[i], x, y, i == fi); else
    card_draw(&G.items.items[i], x, y, 0, 1, abs(row - G.idx / COLS) <= 2);
    if (G.person) { const Item *ci = &G.items.items[i]; char ln[64]; const char *kd = T(strcmp(ci->type, "series") ? "Movie" : "A series");
      if (ci->metaLine[0]) snprintf(ln, sizeof ln, "%s  \xc2\xb7  %s", ci->metaLine, kd); else snprintf(ln, sizeof ln, "%s", kd);
      text_draw(ln, x + 4, y + CARD_H + 14, 22, 500, WHITE_A(160), 0, CARD_W - 8, ALIGN_LEFT); }
    if (G.coll && G.from[0] && !strcmp(G.items.items[i].id, G.from)) { /* the one it was opened from: a quiet glass tag in its corner (the app's pill: dark glass, a fine edge, an accent dot) */
      const char *tf = T(strcmp(G.items.items[i].type, "series") ? "This film" : "This show"); float fs = 18, tw = text_width(tf, fs, 700, 0.2f), h = 34, w = 14 + 8 + 9 + tw + 14, px = rtl ? x + CARD_W - 12 - w : x + 12, py = y + 12;
      gfx_shadow(px, py + 4, w, h, h / 2, 14, BLACK_A(90)); gfx_rrect(px, py, w, h, h / 2, BLACK_A(165)); gfx_ring(px, py, w, h, h / 2, 1.2f, WHITE_A(44));
      gfx_circle(px + 14 + 4, py + h / 2, 4, ACC2 | 0xff000000u); text_draw(tf, px + 14 + 8 + 9, py + (h - text_line_height(fs)) / 2, fs, 700, WHITE_A(240), 0.2f, 0, ALIGN_LEFT); }
    if (pass && hp > 0) hold_trace_draw(x, y, CARD_W, CARD_H, CARD_R, ease_apply(EASE_IN_OUT, hp));
    lift_end(lk, x, y, CARD_W, CARD_H, CARD_R); hit_add(x, y, CARD_W, CARD_H, sim_focus, NULL, i, 0); }
  gfx_offset_pop(); gfx_opacity_pop(); gfx_clip_pop();
  if (G.bioOpen && PI.bio[0]) { /* the whole biography over the page */
    gfx_rect(0, 0, SB_W, SB_H, BLACK_A(200)); float bw = 1240, bh = 880, bx = (SB_W - bw) / 2, by = (SB_H - bh) / 2;
    gfx_rrect_v(bx, by, bw, bh, 30, RGB(30, 28, 38), RGB(16, 15, 21)); gfx_ring(bx, by, bw, bh, 30, 1.2f, WHITE_A(30));
    text_draw(PI.name[0] ? PI.name : G.title, bx + 50, by + 40, 44, 800, WHITE_A(255), -0.5f, bw - 100, ALIGN_LEFT);
    float ty = by + 120, th = bh - 120 - 80; gfx_clip_push(bx + 40, ty, bw - 80, th);
    int nl = text_draw_wrapped(PI.bio, bx + 50, ty - G.bioScroll, bw - 100, 200, 28, 400, WHITE_A(225), 0, ALIGN_LEFT, 42); gfx_clip_pop();
    float maxS = nl * 42 - th; if (maxS < 0) maxS = 0; if (G.bioScroll > maxS) G.bioScroll = maxS;
    key_hint(bx + 50, by + bh - 60, K_DOWN, T("Scroll"), WHITE_A(170), 1); key_hint(bx + bw - 50 - key_hint(-9999, 0, K_BACK, T("Close"), 0, 1), by + bh - 60, K_BACK, T("Close"), WHITE_A(170), 1); }
}
Screen simScreen = { enter, NULL, key, tick, draw, focused, open_it, NULL, origin };

const char *sim_kind(void) { return G.p.it.type; }
