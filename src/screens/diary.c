#include "screens/diary.h"
#include "core/app.h"
#include "core/keys.h"
#include "core/i18n.h"
#include "core/store.h"
#include "core/json.h"
#include "ui/gfx.h"
#include "ui/text.h"
#include "ui/img.h"
#include "ui/sound.h"
#include "ui/widgets.h"
#include "ui/icon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static struct { int open, closing, stars; Item it; char poster[512]; Tween a, s; double at; } D;
static Screen scr;
static void close_(void) { if (!D.open || D.closing) return; D.closing = 1; tween_to(&D.a, 0, 0.35f, EASE_IN_OUT); if (app_overlay == &scr) app_overlay = NULL; }
static void save(void) {
  cJSON *o = cJSON_CreateObject(); cJSON_AddStringToObject(o, "id", D.it.id); cJSON_AddStringToObject(o, "type", D.it.type); cJSON_AddStringToObject(o, "name", D.it.name); cJSON_AddStringToObject(o, "poster", D.it.poster);
  char day[16]; time_t t = time(NULL); struct tm tm; localtime_r(&t, &tm); strftime(day, sizeof day, "%Y-%m-%d", &tm); cJSON_AddStringToObject(o, "day", day); cJSON_AddNumberToObject(o, "stars", D.stars);
  cJSON *n = cJSON_CreateArray(); cJSON_AddItemToArray(n, o); const cJSON *e; int c = 1;
  cJSON_ArrayForEach(e, store_node("diary")) { if (c >= 300 || (!strcmp(js_str(e, "id", ""), D.it.id) && !strcmp(js_str(e, "day", ""), day))) continue; cJSON_AddItemToArray(n, cJSON_Duplicate(e, 1)); c++; }
  store_set_node("diary", n); store_flush(); fprintf(stderr, "[diary] %s: %d stars\n", D.it.id, D.stars);
  char m[160]; snprintf(m, sizeof m, "%s", T("Saved in your diary")); toast(m, 2000, NULL); }
static int key(int k) {
  if (!D.open || D.closing) return 0;
  if (k == K_BACK) { close_(); return 1; }
  if (k == K_LEFT && D.stars > 1) { D.stars--; tween_to(&D.s, (float)D.stars, 0.2f, EASE_OUT); sound_play(SND_FOCUS); }
  else if (k == K_RIGHT && D.stars < 5) { D.stars++; tween_to(&D.s, (float)D.stars, 0.2f, EASE_OUT); sound_play(SND_FOCUS); }
  else if (k == K_OK) { sound_play(SND_TOGGLE); save(); close_(); }
  return 1; }
static void tick(float dt) { tween_tick(&D.a, dt); tween_tick(&D.s, dt); if (D.closing && D.a.v < 0.01f) { D.open = 0; D.closing = 0; } if (D.open && !D.closing && app_now() - D.at > 60) close_(); }
static void draw(void) {
  if (!D.open) return; float a = D.a.v; if (a <= 0.001f) return; gfx_opacity_push(a);
  gfx_rect(0, 0, SB_W, SB_H, BLACK_A(170));
  float w = 1000, h = 460, x = SB_W / 2 - w / 2, y = SB_H / 2 - h / 2 + 20 * (1 - a);
  gfx_shadow(x, y + 30, w, h, 36, 80, BLACK_A(160)); gfx_rrect(x, y, w, h, 36, RGBA(20, 18, 30, 245)); gfx_ring(x, y, w, h, 36, 2, WHITE_A(30));
  { float asp = 0; GLuint t = D.poster[0] ? img_get(D.poster, 260, 390, &asp) : 0; if (t) gfx_image(t, x + 40, y + 35, 260, 390, 20, WHITE_A(255), 0, 0, 1, 1); else gfx_rrect(x + 40, y + 35, 260, 390, 20, WHITE_A(20)); }
  float tx = x + 340; text_draw(T("You finished"), tx, y + 60, 28, 600, WHITE_A(180), 0, 0, ALIGN_LEFT);
  text_draw_wrapped(D.it.name, tx, y + 100, w - 380, 2, 48, 800, WHITE_A(255), -0.5f, ALIGN_LEFT, 54);
  text_draw(T("How many stars?"), tx, y + 230, 30, 700, WHITE_A(220), 0, 0, ALIGN_LEFT);
  for (int i = 0; i < 5; i++) { float sx = tx + i * 96, lit = D.s.v - i; if (lit > 1) lit = 1; if (lit < 0) lit = 0;
    icon_draw("av_star", sx, y + 280, 76, WHITE_A(50)); if (lit > 0) { gfx_opacity_push(lit); icon_draw("av_star", sx, y + 280, 76, RGB(0xf5, 0xb0, 0x3b)); gfx_opacity_pop(); } }
  key_hint(tx, y + 405, K_LEFT, T("Stars"), WHITE_A(180), 0); key_hint(tx + 190, y + 405, K_OK, T("Save"), WHITE_A(220), 0); key_hint(tx + 360, y + 405, K_BACK, T("Skip"), WHITE_A(180), 0);
  gfx_opacity_pop(); }
static Screen scr = { NULL, NULL, key, tick, draw, NULL, NULL, NULL, NULL, NULL };
void diary_ask(const Item *it) {
  if (!strcmp(store_get("diaryAsk", "true"), "false") || !it->id[0]) return;
  D.it = *it; sb_poster_of(it, D.poster, sizeof D.poster); D.stars = 4; tween_set(&D.s, 4); D.open = 1; D.closing = 0; D.at = app_now(); tween_set(&D.a, 0); tween_to(&D.a, 1, 0.4f, EASE_OUT);
  app_overlay_keep(&scr); app_overlay = &scr; }
int diary_items(Item *out, int max) { int n = 0; const cJSON *e; cJSON_ArrayForEach(e, store_node("diary")) { if (n >= max) break; Item *it = &out[n++]; memset(it, 0, sizeof *it);
    snprintf(it->id, sizeof it->id, "%s", js_str(e, "id", "")); snprintf(it->type, sizeof it->type, "%s", js_str(e, "type", "movie")); snprintf(it->poster, sizeof it->poster, "%s", js_str(e, "poster", ""));
    int st = (int)js_num(e, "stars", 0); char s[64] = ""; for (int i = 0; i < 5; i++) strcat(s, i < st ? "\xe2\x98\x85" : "\xe2\x98\x86"); snprintf(it->name, sizeof it->name, "%s  %s", s, js_str(e, "name", "")); }
  return n; }
int diary_stats(int year, int *n, double *avg) { char yp[8]; snprintf(yp, sizeof yp, "%d-", year); int c = 0; double s = 0; const cJSON *e;
  cJSON_ArrayForEach(e, store_node("diary")) if (!strncmp(js_str(e, "day", ""), yp, 5)) { c++; s += js_num(e, "stars", 0); } *n = c; *avg = c ? s / c : 0; return c; }
