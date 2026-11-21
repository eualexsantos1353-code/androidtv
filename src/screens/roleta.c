/* Roulette: can't decide what to watch? A full page with a reel of posters that spins and stops on one title.
   Two modes. Surprise: a random page of the installed catalogs (Discover's, with the type and the genre chosen).
   Personalized: the connected Trakt account's recommendations and watchlist (the watchlist weighs more). Watched titles
   never come out, nor the last ones drawn. The winner is chosen before the reel turns: what stops in the middle is
   what was drawn. OK on the winner opens its page. Opened from Settings > About, or the remote's yellow key on a page. */
#include "screens/roleta.h"
#include "screens/sheet.h"
#include "core/app.h"
#include "core/keys.h"
#include "core/i18n.h"
#include "core/json.h"
#include "core/net.h"
#include "core/hit.h"
#include "core/store.h"
#include "core/stremio.h"
#include "core/scrobble.h"
#include "core/traktrows.h"
#include "ui/gfx.h"
#include "ui/text.h"
#include "ui/widgets.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REEL 22          /* the reel's length, and where the winner sits on it */
#define WIN_AT 18
#define MAXPOOL 480
#define NOT_AGAIN 30     /* how many of the last titles drawn can't come out again */
#define HIST_MAX 40
#define PITCH (CARD_W + CARD_GAP)
#define REEL_Y 344.0f
#define SPIN_SEC 4.8f

typedef struct { Item it; float w; } Cand;
enum { PH_SETUP, PH_LOAD, PH_SPIN, PH_RESULT };
enum { MODE_SURPRISE, MODE_PERSONAL };
typedef struct { unsigned gen; float w; } Req;

static struct {
  int open, phase, mode, eff, kind, gi, ngen, zone, fi, pending, retried, filled, win, pick, pi;
  unsigned gen; double openAt, outAt, t0, resAt, loadAt; float scroll;
  char genres[48][48], note[200];
  Cand *pool; int npool;
  Item reel[REEL];
  Nudge nudge;
} R;
Screen roletaScreen;

static unsigned rs;
static unsigned rnd_u(void) { if (!rs) rs = (unsigned)(app_now() * 1000.0) | 1u; rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
static int rnd(int n) { return n > 0 ? (int)(rnd_u() % (unsigned)n) : 0; }
static float rndf(void) { return (float)(rnd_u() % 100000u) / 100000.0f; }

static const char *kind_type(int k) { return k == 1 ? "movie" : k == 2 ? "series" : ""; }
static const char *genre_name(void) { return R.gi > 0 && R.gi <= R.ngen ? R.genres[R.gi - 1] : ""; }
static const char *const GEN_LIST[] = { "Action", "Adventure", "Animation", "Comedy", "Crime", "Documentary", "Drama", "Family", "Fantasy", "History", "Horror", "Mystery", "Romance", "Sci-Fi", "Thriller", "War", "Western" };
static int genre_ok(const char *g) { /* only the genres some installed catalog really offers (not years, languages and the like) */
  CatalogDef d[1]; int n = 0; if (R.kind != 2) n += sb_disc_defs_f(d, 1, "movie", g) > 0; if (R.kind != 1) n += sb_disc_defs_f(d, 1, "series", g) > 0; return n > 0; }
static void genres_load(const char *want) {
  R.ngen = 0; for (int i = 0; i < (int)(sizeof GEN_LIST / sizeof *GEN_LIST) && R.ngen < 48; i++) if (genre_ok(GEN_LIST[i])) snprintf(R.genres[R.ngen++], 48, "%s", GEN_LIST[i]);
  R.gi = 0; for (int i = 0; want && *want && i < R.ngen; i++) if (!strcmp(R.genres[i], want)) R.gi = i + 1;
}
static void prefs_save(void) {
  store_set("roletaMode", R.mode == MODE_PERSONAL ? "1" : "0"); store_set("roletaKind", R.kind == 1 ? "1" : R.kind == 2 ? "2" : "0"); store_set("roletaGenre", genre_name());
}

/* ---- what was drawn lately (kept: the TV's store, newest first) ---- */
static int hist_has(const char *id, int upto) {
  const char *p = store_get("roletaHist", ""); size_t L = strlen(id); int n = 0;
  while (*p && n < upto) { const char *e = strchr(p, ','); size_t l = e ? (size_t)(e - p) : strlen(p); if (l == L && !strncmp(p, id, L)) return 1; n++; if (!e) break; p = e + 1; }
  return 0;
}
static void hist_add(const char *id) {
  char nh[HIST_MAX * 20 + 32]; snprintf(nh, sizeof nh, "%s", id); size_t o = strlen(nh), L = strlen(id); int n = 1; const char *p = store_get("roletaHist", "");
  while (*p && n < HIST_MAX) { const char *e = strchr(p, ','); size_t l = e ? (size_t)(e - p) : strlen(p);
    if (!(l == L && !strncmp(p, id, l)) && l > 0 && o + l + 2 < sizeof nh) { nh[o++] = ','; memcpy(nh + o, p, l); o += l; nh[o] = 0; n++; }
    if (!e) break; p = e + 1; }
  store_set("roletaHist", nh);
}

/* ---- the pool ---- */
static void add_cand(const Item *it, float w) {
  if (!R.pool || R.npool >= MAXPOOL || !it->id[0] || !it->poster[0] || !it->name[0]) return;
  if (strcmp(it->type, "movie") && strcmp(it->type, "series")) return;
  for (int i = 0; i < R.npool; i++) if (!strcmp(R.pool[i].it.id, it->id)) { R.pool[i].w += w * 0.7f; return; } /* (in both lists: it weighs more) */
  R.pool[R.npool].it = *it; R.pool[R.npool].w = w; R.npool++;
}
static void finish(void);
static void done_one(void) { if (R.pending > 0) R.pending--; if (R.pending == 0 && R.phase == PH_LOAD) finish(); }
static Req *req_new(float w) { Req *r = malloc(sizeof *r); if (r) { r->gen = R.gen; r->w = w; } return r; }

static void on_page(ItemList *l, int more, void *ud) {
  (void)more; Req *r = ud; int ok = r && r->gen == R.gen && R.open; float w = r ? r->w : 1; free(r);
  if (!ok) { if (l) itemlist_free(l); return; }
  fprintf(stderr, "[roleta] page answered: %d titles\n", l ? l->n : -1);
  if (l) { for (int i = 0; i < l->n; i++) add_cand(&l->items[i], w); itemlist_free(l); }
  done_one();
}
static void on_trakt(int st, const unsigned char *b, size_t len, void *ud) {
  (void)len; Req *r = ud; int ok = r && r->gen == R.gen && R.open; float w = r ? r->w : 1; free(r); if (!ok) return;
  cJSON *j = st == 200 ? net_json(b) : NULL; const cJSON *m;
  if (j) cJSON_ArrayForEach(m, js_arr(j, "metas")) {
    Item it; memset(&it, 0, sizeof it); const char *id = js_str(m, "id", ""); if (!sb_is_imdb(id)) continue;
    snprintf(it.id, sizeof it.id, "%s", id); snprintf(it.type, sizeof it.type, "%s", js_str(m, "type", "movie")); snprintf(it.name, sizeof it.name, "%s", js_str(m, "name", ""));
    snprintf(it.metaLine, sizeof it.metaLine, "%s", js_str(m, "releaseInfo", "")); snprintf(it.poster, sizeof it.poster, "https://images.metahub.space/poster/medium/%s/img", id);
    add_cand(&it, w); }
  net_json_free(j); done_one();
}
static void start_surprise(int first, float w) {
  for (int t = 0; t < 2; t++) {
    if ((t == 0 && R.kind == 2) || (t == 1 && R.kind == 1)) continue;
    CatalogDef d[8]; int n = sb_disc_defs_f(d, 8, t == 0 ? "movie" : "series", genre_name()); fprintf(stderr, "[roleta] %s genre '%s': %d catalogs%s%s\n", t == 0 ? "movie" : "series", genre_name(), n, n > 0 ? ", first: " : "", n > 0 ? d[0].title : ""); if (n <= 0) continue;
    int skip = (!first && d[0].skip) ? rnd(4) * 100 : 0; Req *r = req_new(w); if (!r) continue; fprintf(stderr, "[roleta] asks %s skip %d (pages %d, extra '%s')\n", d[0].path, skip, d[0].skip, d[0].extra);
    R.pending++; sb_catalog_page(&d[0], skip, on_page, r);
  }
}
static void start_personal(void) {
  static const char *const LIST[2] = { "r", "w" }; static const float W[2] = { 1.0f, 2.2f }; /* recommendations; the watchlist weighs more */
  for (int t = 0; t < 2; t++) {
    if ((t == 0 && R.kind == 2) || (t == 1 && R.kind == 1)) continue;
    for (int l = 0; l < 2; l++) { char url[32]; snprintf(url, sizeof url, "trakt:%s/%s", LIST[l], t == 0 ? "movie" : "series"); Req *r = req_new(W[l]); if (!r) continue; R.pending++; traktrows_fetch(url, on_trakt, r); }
  }
}

/* ---- the draw (the winner is chosen here; the reel is made to stop on it) ---- */
static void finish(void) {
  if (R.eff == MODE_PERSONAL && R.npool < 6 && !R.filled) { R.filled = 1; snprintf(R.note, sizeof R.note, "%s", T("Few titles match your profile; popular ones filled in.")); R.pending = 1; start_surprise(0, 0.5f); done_one(); return; }
  if (R.npool < 6 && !R.retried) { R.retried = 1; R.pending = 1; start_surprise(1, 0.5f); done_one(); return; }
  static int left[MAXPOOL], fresh[MAXPOOL]; int nl = 0, nf = 0;
  for (int i = 0; i < R.npool; i++) if (!lib_item_watched(&R.pool[i].it)) left[nl++] = i;
  fprintf(stderr, "[roleta] pool %d, not watched %d (mode %d)\n", R.npool, nl, R.eff);
  if (!nl) { R.phase = PH_SETUP; snprintf(R.note, sizeof R.note, "%s", T("Nothing matches these filters")); return; }
  for (int i = 0; i < nl; i++) if (!hist_has(R.pool[left[i]].it.id, NOT_AGAIN)) fresh[nf++] = left[i];
  if (!nf) for (int i = 0; i < nl; i++) if (!hist_has(R.pool[left[i]].it.id, 3)) fresh[nf++] = left[i]; /* (never the last ones) */
  if (!nf) for (int i = 0; i < nl; i++) fresh[nf++] = left[i];
  float tot = 0; for (int i = 0; i < nf; i++) tot += fmaxf(0.05f, R.pool[fresh[i]].w);
  float at = rndf() * tot; int win = fresh[nf - 1]; for (int i = 0; i < nf; i++) { at -= fmaxf(0.05f, R.pool[fresh[i]].w); if (at <= 0) { win = fresh[i]; break; } }
  static int others[MAXPOOL]; int no = 0; for (int i = 0; i < nl; i++) if (left[i] != win) others[no++] = left[i];
  for (int i = no - 1; i > 0; i--) { int j = rnd(i + 1), t = others[i]; others[i] = others[j]; others[j] = t; }
  for (int i = 0; i < REEL; i++) R.reel[i] = R.pool[i == WIN_AT || no == 0 ? win : others[i % no]].it;
  R.reel[WIN_AT] = R.pool[win].it; R.win = WIN_AT; hist_add(R.pool[win].it.id);
  R.phase = PH_SPIN; R.t0 = app_now(); R.scroll = 0; R.zone = 0; R.fi = 3;
}
static void spin(void) {
  if (R.phase == PH_LOAD || R.phase == PH_SPIN) return;
  if (!R.pool) R.pool = calloc(MAXPOOL, sizeof(Cand)); if (!R.pool) return;
  R.gen++; R.phase = PH_LOAD; R.npool = 0; R.retried = R.filled = 0; R.note[0] = 0; R.loadAt = app_now(); R.zone = 0; R.fi = 3; R.pending = 1;
  R.eff = R.mode;
  if (R.mode == MODE_PERSONAL && scrobble_state() != TRAKT_LINKED) { R.eff = MODE_SURPRISE; snprintf(R.note, sizeof R.note, "%s", T("Connect Trakt in Settings for personalized spins; this one was a surprise.")); }
  if (R.eff == MODE_PERSONAL) start_personal(); else start_surprise(0, 1);
  done_one();
}

/* ---- open / close ---- */
void roleta_open(void) {
  if (R.open) return;
  R.mode = !strcmp(store_get("roletaMode", "0"), "1") ? MODE_PERSONAL : MODE_SURPRISE;
  { const char *k = store_get("roletaKind", "0"); R.kind = !strcmp(k, "1") ? 1 : !strcmp(k, "2") ? 2 : 0; }
  genres_load(store_get("roletaGenre", ""));
  R.open = 1; R.pick = 0; R.openAt = app_now(); R.phase = PH_SETUP; R.zone = 0; R.fi = 3; R.note[0] = 0; R.gen++; R.scroll = 0;
  app_overlay = &roletaScreen;
}
static void close_it(void) { if (!R.open) return; R.open = 0; R.pick = 0; R.outAt = app_now(); R.gen++; prefs_save(); }
int roleta_covers(void) { return R.open && app_now() - R.openAt > 0.3; }

static void open_winner(void) {
  Item it = R.reel[R.win]; R.open = 0; R.gen++; prefs_save(); if (app_overlay == &roletaScreen) app_overlay = NULL;
  app_open_item(&it);
}
static void press(void) {
  if (R.zone == 1) { if (R.fi == 0) open_winner(); else spin(); return; }
  if (R.fi == 0) { R.mode = R.mode == MODE_SURPRISE ? MODE_PERSONAL : MODE_SURPRISE; prefs_save(); }
  else if (R.fi == 1) { R.kind = (R.kind + 1) % 3; genres_load(NULL); prefs_save(); }
  else if (R.fi == 2) { if (R.ngen > 0 && R.mode != MODE_PERSONAL) { R.pick = 1; R.pi = R.gi; } } /* (the genres: a page of their own) */
  else spin();
}
static int key(int k) {
  if (!R.open) { if (app_overlay == &roletaScreen) app_overlay = NULL; app_key(app_mirror_key(k), 1, 0); return 1; }
  if (R.pick) { int n = R.ngen + 1, cols = 4;
    if (k == K_BACK) R.pick = 0;
    else if (k == K_LEFT) { if (R.pi > 0) R.pi--; } else if (k == K_RIGHT) { if (R.pi < n - 1) R.pi++; }
    else if (k == K_UP) { if (R.pi >= cols) R.pi -= cols; } else if (k == K_DOWN) { if (R.pi + cols < n) R.pi += cols; else if (R.pi < n - 1 && R.pi / cols < (n - 1) / cols) R.pi = n - 1; }
    else if (k == K_OK) { R.gi = R.pi; R.pick = 0; prefs_save(); }
    return 1; }
  if (k == K_BACK || k == K_YELLOW) { close_it(); return 1; }
  int z0 = R.zone, f0 = R.fi; int busy = R.phase == PH_LOAD || R.phase == PH_SPIN;
  if (k == K_LEFT) { if (R.fi > 0) R.fi--; }
  else if (k == K_RIGHT) { int mx = R.zone == 0 ? 3 : 1; if (R.fi < mx) R.fi++; }
  else if (k == K_UP) { if (R.zone == 1) { R.zone = 0; R.fi = 3; } }
  else if (k == K_DOWN) { if (R.zone == 0 && R.phase == PH_RESULT) { R.zone = 1; R.fi = 0; } }
  else if (k == K_OK) { if (!busy) press(); return 1; }
  else return 1;
  if (R.zone == z0 && R.fi == f0) nudge_start(&R.nudge, k);
  return 1;
}
static void tick(float dt) {
  nudge_tick(&R.nudge, dt);
  if (!R.open) { if (app_now() - R.outAt >= 0.25 && app_overlay == &roletaScreen) app_overlay = NULL; return; }
  (void)dt;
  if (R.phase == PH_LOAD && app_now() - R.loadAt > 25) { R.gen++; R.phase = PH_SETUP; snprintf(R.note, sizeof R.note, "%s", T("Couldn’t reach the catalogs. Try again.")); }
  if (R.phase == PH_SPIN) {
    float x = (float)((app_now() - R.t0) / SPIN_SEC); if (x > 1) x = 1; R.scroll = WIN_AT * PITCH * (1 - powf(1 - x, 4));
    if (x >= 1) { R.phase = PH_RESULT; R.resAt = app_now(); R.zone = 1; R.fi = 0; }
  }
}

/* ---- drawing ---- */
static void on_focus(int a, int b) { (void)b; if (R.phase == PH_LOAD || R.phase == PH_SPIN) return; R.zone = a >= 10 ? 1 : 0; R.fi = a % 10; }
static void on_act(int a, int b) { (void)b; if (R.phase == PH_LOAD || R.phase == PH_SPIN) return; R.zone = a >= 10 ? 1 : 0; R.fi = a % 10; press(); }
static void button(float x, float y, float w, float h, int id, const char *label, const char *value, int primary, int dim) {
  int foc = R.open && ((id >= 10 ? 1 : 0) == R.zone) && (id % 10) == R.fi; float s = foc ? 1.04f : 1;
  gfx_scale_push(s, s, x + w / 2, y + h / 2);
  if (primary) { gfx_shadow(x, y + 8, w, h, h / 2, 24, col_alpha(ACC | 0xff000000u, foc ? 0.5f : 0.25f)); gfx_rrect_v(x, y, w, h, h / 2, ACC2 | 0xff000000u, ACC | 0xff000000u); }
  else { gfx_rrect(x, y, w, h, h / 2, WHITE_A(foc ? 40 : 22)); gfx_ring(x, y, w, h, h / 2, foc ? 2.5f : 1, foc ? (ACC | 0xff000000u) : WHITE_A(36)); }
  uint32_t fg = primary ? ACC_FG : WHITE_A(dim ? 110 : 255);
  if (value) { text_draw(label, x + w / 2, y + 14, 20, 600, primary ? ACC_FG : WHITE_A(dim ? 70 : 140), 0.5f, w - 40, ALIGN_CENTER); text_draw(value, x + w / 2, y + h - 14 - text_line_height(30), 30, 700, fg, 0, w - 40, ALIGN_CENTER); }
  else text_draw(label, x + w / 2, y + (h - text_line_height(32)) / 2.0f, 32, 800, fg, 0, w - 30, ALIGN_CENTER);
  gfx_scale_pop(); hit_add(x, y, w, h, on_focus, on_act, id, 0);
}
static void pk_focus(int a, int b) { (void)b; R.pi = a; }
static void pk_act(int a, int b) { (void)b; R.gi = a; R.pick = 0; prefs_save(); }
static void picker_draw(void) {
  float cx = SB_W / 2, pw = 290, ph = 68, gx = 22, gy = 18; int n = R.ngen + 1, cols = 4, rows = (n + cols - 1) / cols;
  float W = cols * pw + (cols - 1) * gx + 120, H = rows * ph + (rows - 1) * gy + 170, x0 = cx - W / 2, y0 = (SB_H - H) / 2;
  gfx_rect(0, 0, SB_W, SB_H, BLACK_A(160)); hit_shield(0, 0, SB_W, SB_H);
  gfx_shadow(x0, y0 + 30, W, H, 32, 80, BLACK_A(150)); gfx_rrect(x0, y0, W, H, 32, RGB(30, 26, 46)); gfx_ring(x0, y0, W, H, 32, 1, WHITE_A(30));
  text_draw(T("Genre"), cx, y0 + 36, 36, 800, WHITE_A(255), 0, 0, ALIGN_CENTER);
  for (int i = 0; i < n; i++) {
    float x = x0 + 60 + (i % cols) * (pw + gx), y = y0 + 110 + (i / cols) * (ph + gy); int foc = i == R.pi, sel = i == R.gi;
    gfx_rrect(x, y, pw, ph, ph / 2, WHITE_A(foc ? 46 : 20)); if (foc) gfx_ring(x, y, pw, ph, ph / 2, 2.5f, ACC | 0xff000000u); else if (sel) gfx_ring(x, y, pw, ph, ph / 2, 1.5f, col_alpha(ACC | 0xff000000u, 0.6f));
    text_draw(i == 0 ? T("All") : T(R.genres[i - 1]), x + pw / 2, y + (ph - text_line_height(28)) / 2.0f, 28, foc ? 800 : 600, WHITE_A(foc ? 255 : 205), 0, pw - 30, ALIGN_CENTER);
    hit_add(x, y, pw, ph, pk_focus, pk_act, i, 0);
  }
}
static void draw(void) {
  float op;
  if (R.open) { float k = ease_quint((float)((app_now() - R.openAt) / 0.35)); op = k; }
  else { float t = (float)((app_now() - R.outAt) / 0.2); op = 1 - (t < 0 ? 0 : t > 1 ? 1 : t); }
  if (op <= 0.001f) return;
  if (R.open) hit_shield(0, 0, SB_W, SB_H);
  gfx_opacity_push(op);
  gfx_rrect_v(0, 0, SB_W, SB_H, 0, RGB(26, 21, 40), PAGE);
  float cx = SB_W / 2;
  text_draw(T("Roulette"), cx, 56, 60, 800, WHITE_A(255), -1.0f, SB_W - 200, ALIGN_CENTER);
  text_draw(T("Can't decide? Spin the roulette"), cx, 56 + text_line_height(60) + 4, 26, 500, WHITE_A(170), 0, SB_W - 200, ALIGN_CENTER);
  /* the filters and the button */
  gfx_offset_push(nudge_x(&R.nudge), nudge_y(&R.nudge));
  float bw = 360, bh = 92, gap = 24, bx = (SB_W - (3 * bw + 3 * gap + 300)) / 2, by = 196;
  button(bx, by, bw, bh, 0, T("Mode"), T(R.mode == MODE_PERSONAL ? "Personalized" : "Surprise"), 0, 0);
  button(bx + bw + gap, by, bw, bh, 1, T("Type"), R.kind == 1 ? T("Movies") : R.kind == 2 ? T("Series") : T("All"), 0, 0);
  button(bx + 2 * (bw + gap), by, bw, bh, 2, T("Genre"), genre_name()[0] ? T(genre_name()) : T("All"), 0, R.mode == MODE_PERSONAL);
  button(bx + 3 * (bw + gap), by, 300, bh, 3, T(R.phase == PH_LOAD || R.phase == PH_SPIN ? "Spinning…" : R.phase == PH_RESULT ? "Spin again" : "Spin"), NULL, 1, 0);
  gfx_offset_pop();
  /* the reel */
  float t = (float)app_now();
  if (R.phase == PH_SPIN || R.phase == PH_RESULT) {
    for (int i = 0; i < REEL; i++) {
      float x = cx - CARD_W / 2.0f + i * PITCH - R.scroll; if (x > SB_W || x + CARD_W < 0) continue;
      int win = i == R.win && R.phase == PH_RESULT; float k = win ? ease_quint((float)((app_now() - R.resAt) / 0.4)) : 0;
      if (win) { gfx_scale_push(1 + 0.07f * k, 1 + 0.07f * k, x + CARD_W / 2.0f, REEL_Y + CARD_H / 2.0f); gfx_shadow(x, REEL_Y + 18, CARD_W, CARD_H, 22, 50, col_alpha(ACC | 0xff000000u, 0.45f * k)); }
      card_draw(&R.reel[i], x, REEL_Y, 0, 0, 1);
      if (win) { gfx_ring(x, REEL_Y, CARD_W, CARD_H, CARD_R, 3, col_alpha(ACC | 0xff000000u, k)); gfx_scale_pop(); }
    }
  } else {
    float pulse = R.phase == PH_LOAD ? 0.5f + 0.5f * sinf(t * 5) : 0; /* the empty reel: waiting; breathing while it asks */
    for (int k = -3; k <= 3; k++) { float x = cx - CARD_W / 2.0f + k * PITCH, a = (k == 0 ? 26 : 16) + 10 * pulse; if (abs(k) == 3) a *= 0.5f;
      gfx_rrect(x, REEL_Y, CARD_W, CARD_H, CARD_R, WHITE_A((int)a)); if (k == 0) text_draw("?", x + CARD_W / 2.0f, REEL_Y + CARD_H / 2.0f - text_line_height(120) / 2.0f, 120, 800, WHITE_A(60), 0, 0, ALIGN_CENTER); }
  }
  /* the edges of the reel fade into the page, and a mark shows where it stops */
  gfx_rrect_h(0, REEL_Y - 20, 380, CARD_H + 40, 0, RGB(14, 12, 20), col_alpha(RGB(14, 12, 20), 0)); gfx_rrect_h(SB_W - 380, REEL_Y - 20, 380, CARD_H + 40, 0, col_alpha(RGB(14, 12, 20), 0), RGB(14, 12, 20));
  if (R.phase != PH_RESULT) { gfx_rrect(cx - 3, REEL_Y - 30, 6, 18, 3, ACC | 0xff000000u); gfx_rrect(cx - 3, REEL_Y + CARD_H + 12, 6, 18, 3, ACC | 0xff000000u); }
  /* under it: what came out, or what is going on */
  float ty = REEL_Y + CARD_H + 44;
  if (R.phase == PH_RESULT) {
    const Item *w = &R.reel[R.win]; text_draw(w->name, cx, ty, 44, 800, WHITE_A(255), -0.6f, SB_W - 300, ALIGN_CENTER);
    float yy = ty + text_line_height(44) + 6; char ml[160]; snprintf(ml, sizeof ml, "%s%s%s", T(!strcmp(w->type, "series") ? "Series" : "Movie"), w->metaLine[0] ? "   ·   " : "", w->metaLine);
    text_draw(ml, cx, yy, 24, 500, WHITE_A(180), 0.2f, SB_W - 300, ALIGN_CENTER);
    if (R.note[0]) text_draw(R.note, cx, yy + text_line_height(24) + 4, 20, 400, WHITE_A(120), 0, SB_W - 300, ALIGN_CENTER);
    float ow = 300, oh = 76, og = 24, ox = (SB_W - (2 * ow + og)) / 2, oy = SB_H - 56 - oh;
    button(ox, oy, ow, oh, 10, T("Open title"), NULL, 1, 0); button(ox + ow + og, oy, ow, oh, 11, T("Spin again"), NULL, 0, 0);
  } else {
    const char *m = R.note[0] ? R.note : R.phase == PH_LOAD ? T("Picking one…") : R.phase == PH_SPIN ? "" : T("Set your filters and spin");
    if (*m) text_draw(m, cx, ty + 20, 28, 500, WHITE_A(R.note[0] ? 190 : 150), 0, SB_W - 300, ALIGN_CENTER);
  }
  if (R.pick && R.open) picker_draw();
  gfx_opacity_pop();
}
Screen roletaScreen = { NULL, NULL, key, tick, draw, NULL, NULL, NULL };
