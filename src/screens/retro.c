#include "screens/retro.h"
#include "core/app.h"
#include "core/keys.h"
#include "core/i18n.h"
#include "core/json.h"
#include "core/stremio.h"
#include "core/store.h"
#include "ui/gfx.h"
#include "ui/text.h"
#include "ui/img.h"
#include "ui/orbs.h"
#include "ui/sound.h"
#include "ui/widgets.h"
#include "ui/season.h"
#include "screens/splash.h"
#include "screens/diary.h"
#include "core/traktsync.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdint.h>

#define NS 7
#define SLIDE_S 7.0
typedef struct { char name[160], poster[512], bg[512], id[64]; double h; } Top;
static struct { int open, closing, idx, year, movies, series, eps, mRated, mPlays, mDiary; double hours, month[12]; Top tm, ts; char genre[64], actor[96]; int gn, an;
  double at, slideAt; Tween a, k; Orbs orbs; } R;
static const char *MON[12] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };

typedef struct { char s[96]; int n; } Cnt;
static void cnt_add(Cnt *c, int *nc, int cap, const char *s, int w) { if (!s || !*s) return; for (int i = 0; i < *nc; i++) if (!strcmp(c[i].s, s)) { c[i].n += w; return; } if (*nc < cap) { snprintf(c[*nc].s, sizeof c[*nc].s, "%s", s); c[(*nc)++].n = w; } }
static void compute(void) {
  time_t t = time(NULL); struct tm tm; localtime_r(&t, &tm); R.year = tm.tm_year + 1900 - (tm.tm_mon == 0 ? 1 : 0);
  R.movies = R.series = R.eps = 0; R.mRated = R.mPlays = R.mDiary = 0; R.hours = 0; memset(R.month, 0, sizeof R.month); memset(&R.tm, 0, sizeof R.tm); memset(&R.ts, 0, sizeof R.ts); R.genre[0] = R.actor[0] = 0;
  char yp[8]; snprintf(yp, sizeof yp, "%d-", R.year);
  static Cnt gen[64], act[256]; int ng = 0, na = 0;
  const cJSON *it; cJSON_ArrayForEach(it, lib_all()) {
    const char *ty = js_str(it, "type", ""); if (strcmp(ty, "movie") && strcmp(ty, "series")) continue;
    const cJSON *st = js_obj(it, "state"); const char *lw = js_str(st, "lastWatched", ""); if (strncmp(lw, yp, 5)) continue;
    double ms = js_num(st, "overallTimeWatched", 0); if (ms <= 0) ms = js_num(st, "timeWatched", 0); double h = ms / 3.6e6; if (h <= 0.05) continue;
    int mo = atoi(lw + 5) - 1; if (mo >= 0 && mo < 12) R.month[mo] += h; R.hours += h;
    int sr = !strcmp(ty, "series"); if (sr) R.series++; else R.movies++;
    Top *tp = sr ? &R.ts : &R.tm; if (h > tp->h) { tp->h = h; snprintf(tp->name, sizeof tp->name, "%s", js_str(it, "name", "")); snprintf(tp->poster, sizeof tp->poster, "%s", js_str(it, "poster", "")); snprintf(tp->id, sizeof tp->id, "%s", js_str(it, "_id", ""));
      Item x; memset(&x, 0, sizeof x); snprintf(x.id, sizeof x.id, "%s", tp->id); snprintf(x.type, sizeof x.type, "%s", ty); snprintf(x.background, sizeof x.background, "%s", js_str(it, "background", "")); snprintf(x.poster, sizeof x.poster, "%s", tp->poster); sb_bg_mid(&x, tp->bg, sizeof tp->bg); }
    const cJSON *m = sb_meta_cached(ty, js_str(it, "_id", "")); int w = (int)(h + 1); const cJSON *g;
    cJSON_ArrayForEach(g, js_arr(m, "genres")) if (cJSON_IsString(g)) cnt_add(gen, &ng, 64, g->valuestring, w);
    { const cJSON *l; cJSON_ArrayForEach(l, js_arr(m, "links")) { const char *c = js_str(l, "category", ""); if (!strcmp(c, "Genres")) cnt_add(gen, &ng, 64, js_str(l, "name", ""), w); else if (!strcmp(c, "Cast")) cnt_add(act, &na, 256, js_str(l, "name", ""), 1); } }
    cJSON_ArrayForEach(g, js_arr(m, "cast")) if (cJSON_IsString(g)) cnt_add(act, &na, 256, g->valuestring, 1);
  }
  { const cJSON *lg = store_node("watchLog"); for (int i = 0; i < 12; i++) { char mk[12]; snprintf(mk, sizeof mk, "%d-%02d", R.year, i + 1); double sec = js_num(lg, mk, 0); if (sec > 600) R.month[i] = sec / 3600.0; } } /* etapa-74: the months as they were watched (from now on) */
  int bg = -1, ba = -1; for (int i = 0; i < ng; i++) if (bg < 0 || gen[i].n > gen[bg].n) bg = i; for (int i = 0; i < na; i++) if (ba < 0 || act[i].n > act[ba].n) ba = i;
  if (bg >= 0) snprintf(R.genre, sizeof R.genre, "%s", gen[bg].s); if (ba >= 0 && act[ba].n >= 2) { snprintf(R.actor, sizeof R.actor, "%s", act[ba].s); R.an = act[ba].n; }
  { const TsyncYear *y = tsync_year(); if (y && y->ok && y->year == R.year) { /* etapa-80: Trakt's history knows the whole year, every day: it leads */
      R.hours = y->hours; R.movies = y->movies; R.series = y->series; for (int i = 0; i < 12; i++) if (y->month[i] > R.month[i]) R.month[i] = y->month[i];
      if (y->showName[0]) { snprintf(R.ts.name, sizeof R.ts.name, "%s", y->showName); snprintf(R.ts.id, sizeof R.ts.id, "%s", y->showId); R.ts.h = y->showH; snprintf(R.ts.poster, sizeof R.ts.poster, "https://images.metahub.space/poster/medium/%s/img", y->showId); snprintf(R.ts.bg, sizeof R.ts.bg, "https://images.metahub.space/background/medium/%s/img", y->showId); }
      R.mRated = y->movieRated; R.mPlays = y->moviePlays;
      if (y->movieName[0]) { snprintf(R.tm.name, sizeof R.tm.name, "%s", y->movieName); snprintf(R.tm.id, sizeof R.tm.id, "%s", y->movieId); R.tm.h = y->movieH; snprintf(R.tm.poster, sizeof R.tm.poster, "https://images.metahub.space/poster/medium/%s/img", y->movieId); snprintf(R.tm.bg, sizeof R.tm.bg, "https://images.metahub.space/background/medium/%s/img", y->movieId); }
      if (y->genre[0]) snprintf(R.genre, sizeof R.genre, "%s", y->genre); } }
  if (!R.mRated) { const cJSON *e, *best = NULL; char yp[8]; snprintf(yp, sizeof yp, "%d-", R.year); /* (no Trakt rating: the diary's best of the year) */
    cJSON_ArrayForEach(e, store_node("diary")) if (!strncmp(js_str(e, "day", ""), yp, 5) && !strcmp(js_str(e, "type", "movie"), "movie") && (!best || js_num(e, "stars", 0) > js_num(best, "stars", 0))) best = e;
    if (best && js_num(best, "stars", 0) >= 4) { snprintf(R.tm.name, sizeof R.tm.name, "%s", js_str(best, "name", "")); snprintf(R.tm.id, sizeof R.tm.id, "%s", js_str(best, "id", "")); snprintf(R.tm.poster, sizeof R.tm.poster, "%s", js_str(best, "poster", ""));
      snprintf(R.tm.bg, sizeof R.tm.bg, "https://images.metahub.space/background/medium/%s/img", R.tm.id); R.mDiary = (int)js_num(best, "stars", 0); } }
  fprintf(stderr, "[retro] %d: %.0f h, %d films, %d series\n", R.year, R.hours, R.movies, R.series);
}
/* etapa-74: genres and faces need each title's page: the ones not seen yet are asked, the slides follow as they come */
static int pend; static unsigned mgen;
static void on_meta(cJSON *m, void *ud) { (void)m; if ((unsigned)(uintptr_t)ud != mgen) return; if (--pend <= 0 && R.open) compute(); }
static void ask_metas(void) {
  typedef struct { const char *ty, *id; double h; } T_; static T_ t[400]; int n = 0; const cJSON *it; char yp[8]; snprintf(yp, sizeof yp, "%d-", R.year);
  cJSON_ArrayForEach(it, lib_all()) { const char *ty = js_str(it, "type", ""); if (strcmp(ty, "movie") && strcmp(ty, "series")) continue; const cJSON *st = js_obj(it, "state");
    if (strncmp(js_str(st, "lastWatched", ""), yp, 5) || n >= 400) continue; double ms = js_num(st, "overallTimeWatched", 0); if (ms <= 0) continue; t[n].ty = ty; t[n].id = js_str(it, "_id", ""); t[n].h = ms; n++; }
  for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) if (t[j].h > t[i].h) { T_ x = t[i]; t[i] = t[j]; t[j] = x; }
  mgen++; pend = 0; for (int i = 0; i < n && i < 30; i++) if (!sb_meta_cached(t[i].ty, t[i].id)) { pend++; sb_meta(t[i].ty, t[i].id, on_meta, (void *)(uintptr_t)mgen); }
}
static Screen scr;
static void close_(void) { if (!R.open) return; R.closing = 1; tween_to(&R.a, 0, 0.4f, EASE_IN_OUT); if (app_overlay == &scr) app_overlay = NULL; }
static void go(int d) { int n = R.idx + d; if (n < 0) n = 0; if (n >= NS) { close_(); return; } R.idx = n; R.slideAt = app_now(); tween_set(&R.k, 0); tween_to(&R.k, 1, 0.7f, EASE_OUT); sound_play(SND_TAB); }
static int key(int k) { if (!R.open || R.closing) return 0; if (k == K_BACK) { close_(); return 1; } if (k == K_RIGHT || k == K_OK || k == K_DOWN) go(1); else if (k == K_LEFT || k == K_UP) go(-1); return 1; }
static void tick(float dt) { { static double seen; double at = js_num(store_node("traktYear"), "at", 0); if (at != seen) { seen = at; if (R.open && at) compute(); } } tween_tick(&R.a, dt); tween_tick(&R.k, dt); if (R.closing && R.a.v < 0.01f) { R.open = 0; R.closing = 0; }
  if (R.open && !R.closing && app_now() - R.slideAt > SLIDE_S) go(1); orbs_step(&R.orbs, dt, 0, 1, 1); }
static void big(const char *s, float y, int px, uint32_t c) { text_draw(s, SB_W / 2, y, px, 800, c, -2, SB_W - 200, ALIGN_CENTER); }
static void poster(const char *url, float cx, float y, float h, float k) { if (!url[0]) return; float asp = 0; GLuint t = img_get(url, (int)(h * 2 / 3), (int)h, &asp); if (!t) return; float w = h * 2 / 3, s = 0.9f + 0.1f * k;
  gfx_scale_push(s, s, cx, y + h / 2); gfx_shadow(cx - w / 2, y + 30, w, h, 26, 90, BLACK_A(170)); gfx_image(t, cx - w / 2, y, w, h, 22, WHITE_A(255), 0, 0, 1, 1); gfx_scale_pop(); }
static void draw(void) {
  if (!R.open) return; float a = R.a.v, k = R.k.v; if (a <= 0.001f) return; gfx_opacity_push(a);
  gfx_rect(0, 0, SB_W, SB_H, PAGE); orbs_draw(&R.orbs, 0.9f);
  const Top *bgT = R.idx == 2 ? &R.ts : R.idx == 3 ? &R.tm : NULL;
  if (bgT && bgT->bg[0]) { float asp = 0; GLuint t = img_get(bgT->bg, 960, 540, &asp); if (t) { gfx_opacity_push(0.35f * k); gfx_image(t, 0, 0, SB_W, SB_H, 0, WHITE_A(255), 0, 0, 1, 1); gfx_opacity_pop(); gfx_rect(0, 0, SB_W, SB_H, col_alpha(PAGE | 0xff000000u, 0.55f)); } }
  /* the progress bars on top, as stories */
  { float W = SB_W - 240, g = 10, bw = (W - g * (NS - 1)) / NS, p = (float)((app_now() - R.slideAt) / SLIDE_S); if (p > 1) p = 1;
    for (int i = 0; i < NS; i++) { float x = 120 + i * (bw + g); gfx_rrect(x, 60, bw, 6, 3, WHITE_A(60)); float f = i < R.idx ? 1 : i == R.idx ? p : 0; if (f > 0) gfx_rrect(x, 60, bw * f, 6, 3, WHITE_A(230)); } }
  float up = 40 * (1 - k); gfx_opacity_push(k); gfx_offset_push(0, up);
  char b[200], n[32]; uint32_t acc = ACC2 | 0xff000000u;
  switch (R.idx) {
    case 0: lumen_mark_draw(SB_W / 2, 360, 120, (float)(app_now() - R.slideAt), 0); snprintf(b, sizeof b, "%s %d", T("Your year"), R.year); big(b, 560, 96, WHITE_A(255)); text_draw(T("Your year in the Lumen, in a few slides"), SB_W / 2, 700, 34, 500, WHITE_A(200), 0, 0, ALIGN_CENTER); break;
    case 1: { double sh = R.hours * fmin(1, (app_now() - R.slideAt) / 2.0); snprintf(n, sizeof n, "%.0f", sh); text_draw(T("You watched"), SB_W / 2, 250, 40, 600, WHITE_A(210), 0, 0, ALIGN_CENTER); big(n, 330, 220, acc);
      text_draw(T("hours of films and series"), SB_W / 2, 600, 44, 700, WHITE_A(240), 0, 0, ALIGN_CENTER);
      char m[16], s2[16]; snprintf(m, sizeof m, "%d", R.movies); snprintf(s2, sizeof s2, "%d", R.series); snprintf(b, sizeof b, "%s  \xc2\xb7  %s", T1("{n} films", "n", m), T1("{n} series", "n", s2)); text_draw(b, SB_W / 2, 690, 34, 500, WHITE_A(200), 0, 0, ALIGN_CENTER);
      { int dn; double da; if (diary_stats(R.year, &dn, &da) > 0) { char d1[16], d2[16]; snprintf(d1, sizeof d1, "%d", dn); snprintf(d2, sizeof d2, "%.1f", da); text_draw(T2("{n} films in your diary, {a} stars on average", "n", d1, "a", d2), SB_W / 2, 820, 28, 600, RGB(0xf5, 0xb0, 0x3b), 0, 0, ALIGN_CENTER); } } /* etapa-79 */
      { double days = R.hours / 24; if (days >= 1) { char d[16]; snprintf(d, sizeof d, "%.1f", days); text_draw(T1("That is {n} days in a row in front of the TV", "n", d), SB_W / 2, 760, 28, 500, WHITE_A(160), 0, 0, ALIGN_CENTER); } } break; }
    case 2: case 3: { const Top *tp = R.idx == 2 ? &R.ts : &R.tm; text_draw(T(R.idx == 2 ? "Your series of the year" : R.mRated || R.mDiary || !R.mPlays ? "Your film of the year" : "The film you saw the most"), SB_W / 2, 140, 40, 700, acc, 1, 0, ALIGN_CENTER);
      if (!tp->name[0]) { big(T("None this year"), 460, 64, WHITE_A(220)); break; } poster(tp->poster, SB_W / 2, 220, 560, k); big(tp->name, 810, 56, WHITE_A(255));
      if (R.idx == 3 && R.mRated) { snprintf(n, sizeof n, "%d", R.mRated); text_draw(T1("{n}/10 on Trakt", "n", n), SB_W / 2, 890, 32, 600, RGB(0xf5, 0xb0, 0x3b), 0, 0, ALIGN_CENTER); }
      else if (R.idx == 3 && R.mDiary) { char st[32] = ""; for (int q = 0; q < 5; q++) strcat(st, q < R.mDiary ? "\xe2\x98\x85" : "\xe2\x98\x86"); text_draw(st, SB_W / 2, 890, 36, 600, RGB(0xf5, 0xb0, 0x3b), 2, 0, ALIGN_CENTER); }
      else if (R.idx == 3 && R.mPlays > 1) { snprintf(n, sizeof n, "%d", R.mPlays); text_draw(T1("Seen {n} times", "n", n), SB_W / 2, 890, 32, 500, WHITE_A(200), 0, 0, ALIGN_CENTER); }
      else { snprintf(n, sizeof n, "%.0f", tp->h < 1 ? 1 : tp->h); text_draw(T1("{n} hours together", "n", n), SB_W / 2, 890, 32, 500, WHITE_A(200), 0, 0, ALIGN_CENTER); }
      if (R.idx == 3 && !R.mRated && !R.mDiary) text_draw(T("Give your films stars (Diary or Trakt) to choose your film of the year"), SB_W / 2, 950, 24, 500, WHITE_A(150), 0, 0, ALIGN_CENTER);
      break; }
    case 4: text_draw(T("Your genre"), SB_W / 2, 230, 40, 600, WHITE_A(200), 0, 0, ALIGN_CENTER); if (R.genre[0]) big(T(R.genre), 300, 130, acc); else text_draw(T(pend > 0 ? "Finding out\xe2\x80\xa6" : "Not enough yet"), SB_W / 2, 330, 48, 700, WHITE_A(200), 0, 0, ALIGN_CENTER);
      if (R.actor[0]) { text_draw(T("The face you saw the most"), SB_W / 2, 560, 36, 600, WHITE_A(200), 0, 0, ALIGN_CENTER); big(R.actor, 620, 80, WHITE_A(255)); char c[16]; snprintf(c, sizeof c, "%d", R.an); text_draw(T1("in {n} of your titles", "n", c), SB_W / 2, 730, 30, 500, WHITE_A(170), 0, 0, ALIGN_CENTER); } break;
    case 5: { int bm = 0; double mx = 0; for (int i = 0; i < 12; i++) if (R.month[i] > mx) { mx = R.month[i]; bm = i; }
      text_draw(T("Your marathon month"), SB_W / 2, 150, 40, 600, WHITE_A(200), 0, 0, ALIGN_CENTER); if (mx > 0) big(T(MON[bm]), 220, 110, acc); else text_draw(T("Not enough yet"), SB_W / 2, 250, 48, 700, WHITE_A(200), 0, 0, ALIGN_CENTER);
      float bx = 260, bwid = (SB_W - 520) / 12.f, base = 900, hmax = 420;
      for (int i = 0; i < 12; i++) { float f = mx > 0 ? (float)(R.month[i] / mx) : 0, hh = hmax * f * k; gfx_rrect(bx + i * bwid + 10, base - hh, bwid - 20, hh < 8 ? 8 : hh, 10, i == bm ? acc : WHITE_A(70));
        if (R.month[i] >= 1 && k > 0.9f) { char hv[16]; snprintf(hv, sizeof hv, "%.0fh", R.month[i]); text_draw(hv, bx + i * bwid + bwid / 2, base - hh - 34, 22, 700, WHITE_A(200), 0, 0, ALIGN_CENTER); }
        char ml[8]; snprintf(ml, sizeof ml, "%.3s", T(MON[i])); text_draw(ml, bx + i * bwid + bwid / 2, base + 16, 22, 600, WHITE_A(170), 0, 0, ALIGN_CENTER); } break; }
    default: lumen_mark_draw(SB_W / 2, 380, 110, 3, 1); big(T("Thanks for watching with the Lumen"), 600, 64, WHITE_A(255)); snprintf(b, sizeof b, "%s %d", T("See you in"), R.year + 1); text_draw(b, SB_W / 2, 700, 36, 500, acc, 0, 0, ALIGN_CENTER); break;
  }
  gfx_offset_pop(); gfx_opacity_pop();
  key_hint(SB_W / 2 - 160, SB_H - 80, K_OK, T("Next"), WHITE_A(200), 0); key_hint(SB_W / 2 + 40, SB_H - 80, K_BACK, T("Close"), WHITE_A(200), 0);
  gfx_opacity_pop();
}
static Screen scr = { NULL, NULL, key, tick, draw, NULL, NULL, NULL, NULL, NULL };
void retro_open(void) {
  compute(); ask_metas(); tsync_retro_refresh(R.year); R.open = 1; R.closing = 0; R.idx = 0; R.slideAt = app_now(); tween_set(&R.a, 0); tween_to(&R.a, 1, 0.5f, EASE_OUT); tween_set(&R.k, 0); tween_to(&R.k, 1, 0.8f, EASE_OUT);
  orbs_place(&R.orbs, 0, 1.6f); app_overlay_keep(&scr); app_overlay = &scr; sound_play(SND_TAB);
}
