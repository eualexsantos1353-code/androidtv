#include "screens/mural.h"
#include "core/app.h"
#include "core/keys.h"
#include "core/i18n.h"
#include "ui/gfx.h"
#include "ui/text.h"
#include "ui/img.h"
#include "ui/sound.h"
#include "ui/widgets.h"
#include "screens/detail.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static struct { int open, closing, n, idx; Item *it; char (*poster)[512]; Tween a, pos; double holdAt; } M;
static Screen scr;
static void close_(void) { if (!M.open || M.closing) return; M.closing = 1; tween_to(&M.a, 0, 0.35f, EASE_IN_OUT); if (app_overlay == &scr) app_overlay = NULL; }
static void move(int d) { int n = M.idx + d; if (n < 0 || n >= M.n) return; M.idx = n; tween_to(&M.pos, (float)n, 0.45f, EASE_CSS_EASEOUT); sound_play(SND_FOCUS); }
static int key(int k) {
  if (!M.open || M.closing) return 0;
  if (k == K_BACK) { close_(); return 1; }
  if (k == K_RIGHT) move(1); else if (k == K_LEFT) move(-1); else if (k == K_DOWN) move(5); else if (k == K_UP) move(-5);
  else if (k == K_OK && M.n) { DetailParams p; memset(&p, 0, sizeof p); p.it = M.it[M.idx]; close_(); app_go(SCR_DETAIL, &p, 0); }
  return 1; }
static void tick(float dt) { tween_tick(&M.a, dt); tween_tick(&M.pos, dt); if (M.closing && M.a.v < 0.01f) { M.open = 0; M.closing = 0; } }
static void one(int i, float rel) {
  float ar = fabsf(rel); if (ar > 6.5f) return;
  float H = 560, W = H * 2 / 3, sc = ar < 1 ? 1 - 0.3f * ar : 0.7f - 0.07f * (ar - 1), sq = ar < 1 ? 1 - 0.35f * ar : 0.65f; /* sq: turned away (narrower) */
  float off = ar < 1 ? rel * W * 0.75f : (rel > 0 ? 1 : -1) * (W * 0.75f + (ar - 1) * W * 0.36f);
  float cx = SB_W / 2 + off, cy = 520, w = W * sc * sq, h = H * sc, x = cx - w / 2, y = cy - h / 2;
  float asp = 0; GLuint t = img_get(M.poster[i], (int)W, (int)H, &asp); float dim = ar < 1 ? 1 - 0.45f * ar : 0.55f - 0.06f * (ar - 1);
  if (!t) { gfx_rrect(x, y, w, h, 16, RGBA(40, 36, 56, 255)); text_draw_wrapped(M.it[i].name, x + 16, y + 24, w - 32, 4, 26, 700, WHITE_A(220), 0, ALIGN_LEFT, 32); return; }
  gfx_shadow(x, y + 30, w, h, 18, 60, BLACK_A((int)(160 * dim)));
  gfx_image(t, x, y, w, h, 16, WHITE_A(255), 0, 0, 1, 1); if (dim < 1) gfx_rrect(x, y, w, h, 16, BLACK_A((int)(255 * (1 - dim))));
  /* its reflection on the floor: upside down, fading */
  gfx_opacity_push(0.22f * dim); gfx_image(t, x, y + h + 10, w, h * 0.45f, 16, WHITE_A(255), 0, 1, 1, 0.55f); gfx_opacity_pop();
  gfx_rrect_v(x - 2, y + h + 8, w + 4, h * 0.45f + 4, 0, col_alpha(PAGE | 0xff000000u, 0.35f), PAGE | 0xff000000u);
  if (ar < 0.5f) gfx_ring(x - 4, y - 4, w + 8, h + 8, 20, 4, col_alpha(ACC | 0xff000000u, 1 - 2 * ar));
}
static void draw(void) {
  if (!M.open) return; float a = M.a.v; if (a <= 0.001f) return; gfx_opacity_push(a);
  gfx_rect(0, 0, SB_W, SB_H, PAGE); gfx_rrect_v(0, 0, SB_W, SB_H * 0.6f, 0, col_alpha(ACC_D | 0xff000000u, 0.35f), col_alpha(ACC_D | 0xff000000u, 0));
  text_draw(T("Your wall"), 100, 70, 40, 800, WHITE_A(240), -0.5f, 0, ALIGN_LEFT);
  { char c[32]; snprintf(c, sizeof c, "%d / %d", M.idx + 1, M.n); text_draw(c, SB_W - 100, 80, 28, 600, WHITE_A(170), 0, 0, ALIGN_RIGHT); }
  float p = M.pos.v; int c = (int)floorf(p + 0.5f);
  for (int d = 6; d >= 1; d--) { if (c - d >= 0) one(c - d, (float)(c - d) - p); if (c + d < M.n) one(c + d, (float)(c + d) - p); }
  if (c >= 0 && c < M.n) one(c, (float)c - p);
  if (M.n) text_draw(M.it[M.idx].name, SB_W / 2, 880, 40, 800, WHITE_A(250), -0.5f, SB_W - 300, ALIGN_CENTER);
  key_hint(SB_W / 2 - 220, SB_H - 60, K_LEFT, T("Walk"), WHITE_A(180), 0); key_hint(SB_W / 2 + 20, SB_H - 60, K_OK, T("Open"), WHITE_A(200), 0);
  gfx_opacity_pop();
}
static Screen scr = { NULL, NULL, key, tick, draw, NULL, NULL, NULL, NULL, NULL };
void mural_open(const Item *items, int n, int at) {
  if (n <= 0) return; free(M.it); free(M.poster); M.it = malloc((size_t)n * sizeof *M.it); M.poster = malloc((size_t)n * sizeof *M.poster); if (!M.it || !M.poster) return;
  memcpy(M.it, items, (size_t)n * sizeof *items); M.n = n; for (int i = 0; i < n; i++) sb_poster_of(&M.it[i], M.poster[i], sizeof M.poster[i]);
  M.idx = at < 0 ? 0 : at >= n ? n - 1 : at; tween_set(&M.pos, (float)M.idx); M.open = 1; M.closing = 0; tween_set(&M.a, 0); tween_to(&M.a, 1, 0.4f, EASE_OUT);
  app_overlay_keep(&scr); app_overlay = &scr; sound_play(SND_TAB);
}
