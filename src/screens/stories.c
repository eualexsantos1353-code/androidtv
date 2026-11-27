#include "screens/stories.h"
#include "core/app.h"
#include "core/keys.h"
#include "core/i18n.h"
#include "core/json.h"
#include "core/store.h"
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
#include <time.h>

#define MAXS 24
#define DUR 6.5
static struct { int open, closing, n, idx; Item it[MAXS]; char bg[MAXS][512], logo[MAXS][512]; double at; Tween a, k; } S;
int stories_seen(const char *id) { const cJSON *s = store_node("storiesSeen"), *e; cJSON_ArrayForEach(e, s) if (cJSON_IsString(e) && !strcmp(e->valuestring, id)) return 1; return 0; }
static void mark_seen(const char *id) { if (!id[0] || stories_seen(id)) return; cJSON *o = store_node("storiesSeen"), *n = cJSON_CreateArray(); cJSON_AddItemToArray(n, cJSON_CreateString(id)); int c = 1; const cJSON *e;
  cJSON_ArrayForEach(e, o) if (c < 200 && cJSON_IsString(e)) { cJSON_AddItemToArray(n, cJSON_CreateString(e->valuestring)); c++; } store_set_node("storiesSeen", n); }
static Screen scr;
static void close_(void) { if (!S.open || S.closing) return; S.closing = 1; tween_to(&S.a, 0, 0.35f, EASE_IN_OUT); if (app_overlay == &scr) app_overlay = NULL; }
static void go(int i) { if (i < 0) i = 0; if (i >= S.n) { close_(); return; } S.idx = i; S.at = app_now(); tween_set(&S.k, 0); tween_to(&S.k, 1, 0.6f, EASE_OUT); mark_seen(S.it[i].id); }
static int key(int k) {
  if (!S.open || S.closing) return 0;
  if (k == K_BACK) { close_(); return 1; }
  if (k == K_RIGHT) go(S.idx + 1); else if (k == K_LEFT) go(S.idx - 1);
  else if (k == K_OK) { DetailParams p; memset(&p, 0, sizeof p); p.it = S.it[S.idx]; close_(); app_go(SCR_DETAIL, &p, 0); }
  return 1; }
static void tick(float dt) { tween_tick(&S.a, dt); tween_tick(&S.k, dt); if (S.closing && S.a.v < 0.01f) { S.open = 0; S.closing = 0; } if (S.open && !S.closing && app_now() - S.at > DUR) go(S.idx + 1); }
static void draw(void) {
  if (!S.open) return; float a = S.a.v; if (a <= 0.001f) return; const Item *it = &S.it[S.idx]; float t = (float)(app_now() - S.at), k = S.k.v;
  gfx_opacity_push(a); gfx_rect(0, 0, SB_W, SB_H, RGB(0, 0, 0));
  { float asp = 0; GLuint tx = img_get(S.bg[S.idx], 1280, 720, &asp); if (tx) { float z = 1.04f + 0.06f * (t / (float)DUR); gfx_opacity_push(k); gfx_scale_push(z, z, SB_W * 0.6f, SB_H * 0.4f); gfx_image(tx, 0, 0, SB_W, SB_H, 0, WHITE_A(255), 0, 0, 1, 1); gfx_scale_pop(); gfx_opacity_pop(); } }
  gfx_rrect_v(0, SB_H * 0.4f, SB_W, SB_H * 0.6f, 0, BLACK_A(0), BLACK_A(230)); gfx_rrect_v(0, 0, SB_W, 200, 0, BLACK_A(150), BLACK_A(0));
  { float W = SB_W - 200, g = 8, bw = (W - g * (S.n - 1)) / S.n, p = t / (float)DUR; if (p > 1) p = 1;
    for (int i = 0; i < S.n; i++) { float x = 100 + i * (bw + g); gfx_rrect(x, 48, bw, 6, 3, WHITE_A(70)); float f = i < S.idx ? 1 : i == S.idx ? p : 0; if (f > 0) gfx_rrect(x, 48, bw * f, 6, 3, WHITE_A(240)); } }
  float up = 30 * (1 - k); gfx_opacity_push(k); gfx_offset_push(0, up);
  int newEp = it->videoId[0] != 0; const char *tag = newEp ? T("New episode") : T("New release");
  { float tw = text_width(tag, 24, 800, 1.5f) + 36; gfx_rrect(100, SB_H - 470, tw, 46, 23, newEp ? (ACC | 0xff000000u) : RGB(0xef, 0x44, 0x44)); text_draw(tag, 118, SB_H - 470 + (46 - text_line_height(24)) / 2, 24, 800, WHITE_A(255), 1.5f, 0, ALIGN_LEFT); }
  { float asp = 0; GLuint lt = S.logo[S.idx][0] ? img_get(S.logo[S.idx], 560, 200, &asp) : 0; if (lt && asp > 0) { float lw = 560, lh = lw / asp; if (lh > 190) { lh = 190; lw = lh * asp; } gfx_image(lt, 100, SB_H - 400 + (190 - lh), lw, lh, 0, WHITE_A(255), 0, 0, 1, 1); }
    else text_draw_wrapped(it->name, 100, SB_H - 390, SB_W - 400, 2, 76, 800, WHITE_A(255), -1, ALIGN_LEFT, 84); }
  { char se[64] = ""; const char *c = strrchr(it->videoId, ':');
    if (c) { char s1[8], e1[8]; const char *p = c - 1; while (p > it->videoId && *p != ':') p--; snprintf(s1, sizeof s1, "%d", atoi(p + 1)); snprintf(e1, sizeof e1, "%d", atoi(c + 1)); snprintf(se, sizeof se, "%s", T2("Season {s} \xc2\xb7 Episode {e}", "s", s1, "e", e1)); text_draw(se, 100, SB_H - 190, 32, 700, WHITE_A(240), 0, 0, ALIGN_LEFT); }
    else if (it->description[0]) text_draw_wrapped(it->description, 100, SB_H - 200, SB_W - 600, 2, 28, 400, WHITE_A(215), 0, ALIGN_LEFT, 38); }
  gfx_offset_pop(); gfx_opacity_pop();
  key_hint(100, SB_H - 70, K_OK, T("Open"), WHITE_A(220), 0); key_hint(300, SB_H - 70, K_LEFT, T("Previous / next"), WHITE_A(180), 0);
  gfx_opacity_pop();
}
static Screen scr = { NULL, NULL, key, tick, draw, NULL, NULL, NULL, NULL, NULL };
void stories_open(const Item *items, int n, int at) {
  if (n <= 0) return; if (n > MAXS) n = MAXS; S.n = n; memcpy(S.it, items, (size_t)n * sizeof *items);
  for (int i = 0; i < n; i++) { sb_bg_mid(&S.it[i], S.bg[i], sizeof S.bg[i]); snprintf(S.logo[i], sizeof S.logo[i], "%s", S.it[i].logo); }
  S.open = 1; S.closing = 0; tween_set(&S.a, 0); tween_to(&S.a, 1, 0.4f, EASE_OUT); app_overlay_keep(&scr); app_overlay = &scr; sound_play(SND_TAB); go(at);
}
