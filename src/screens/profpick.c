#include "screens/profpick.h"
#include "core/app.h"
#include "core/keys.h"
#include "core/i18n.h"
#include "core/store.h"
#include "core/organize.h"
#include "core/stremio.h"
#include "ui/gfx.h"
#include "ui/text.h"
#include "ui/orbs.h"
#include "ui/sound.h"
#include "ui/widgets.h"
#include "ui/icon.h"
#include "screens/splash.h"
#include "screens/board.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* zone 0: the avatars (and "Add profile"); zone 1: the Manage button; edit: the profile's own rows */
enum { E_AV, E_NAME, E_KID, E_DEL, E_DONE, E_N };
static struct { int open, closing, atStart, idx, zone, manage, edit, erow, typing; char name[64]; Tween a, f; Orbs orbs; } P;
static Screen scr;
int profpick_on(void) { return P.open && !P.closing; }
static int nslots(void) { int n = prof_count(); return n + (n < PROF_N ? 1 : 0); }
static void close_(void) { if (!P.open || P.closing) return; P.closing = 1; tween_to(&P.a, 0, 0.4f, EASE_IN_OUT); if (app_overlay == &scr) app_overlay = NULL; SDL_StopTextInput(); }
static void go_in(int i) {
  int was = prof_cur(); if (i == was && sb_signed_in()) { close_(); return; }
  int ok = prof_switch(i); sound_play(SND_START); close_();
  board_invalidate(); topNav.idx = NAV_HOME;
  if (ok) { sb_sync_addons(NULL, NULL); sb_pull_library(NULL, NULL); app_go(SCR_HOME, NULL, 1); char m[160]; snprintf(m, sizeof m, "%s", T1("Hello, {p}", "p", prof_name(i))); toast(m, 2200, NULL); }
  else { char m[160]; snprintf(m, sizeof m, "%s", T1("Sign in with {p}\xe2\x80\x99s Stremio account", "p", prof_name(i))); app_go(SCR_LOGIN, NULL, 1); toast(m, 4000, NULL); } /* its own account: its sign-in (Back there comes back here) */
}
static void edit_open(int i) { P.edit = 1; P.idx = i; P.erow = E_NAME; snprintf(P.name, sizeof P.name, "%s", prof_name(i)); sound_play(SND_TAB); }
static void edit_close(void) { if (P.typing) { P.typing = 0; SDL_StopTextInput(); } if (P.name[0]) prof_set_name(P.idx, P.name); P.edit = 0; board_invalidate(); }
static int key(int k) {
  if (!P.open || P.closing) return 0;
  if (P.edit) {
    if (P.typing) { if (k == K_BACK) { size_t n = strlen(P.name); if (n) { n--; while (n && (P.name[n] & 0xc0) == 0x80) n--; P.name[n] = 0; } else { P.typing = 0; SDL_StopTextInput(); } } else if (k == K_OK || k == K_DOWN || k == K_UP) { P.typing = 0; SDL_StopTextInput(); prof_set_name(P.idx, P.name); } return 1; }
    if (k == K_BACK) { edit_close(); return 1; }
    if (k == K_UP && P.erow > 0) { P.erow--; if (P.erow == E_DEL && P.idx == 0) P.erow--; if (P.erow == E_KID && P.idx == 0) P.erow--; sound_play(SND_FOCUS); }
    else if (k == K_DOWN && P.erow < E_N - 1) { P.erow++; if (P.erow == E_KID && P.idx == 0) P.erow++; if (P.erow == E_DEL && P.idx == 0) P.erow++; sound_play(SND_FOCUS); }
    else if (P.erow == E_AV && (k == K_LEFT || k == K_RIGHT)) { prof_set_avatar(P.idx, prof_avatar(P.idx) + (k == K_RIGHT ? 1 : -1)); sound_play(SND_TOGGLE); }
    else if (k == K_OK) {
      if (P.erow == E_AV) prof_set_avatar(P.idx, prof_avatar(P.idx) + 1);
      else if (P.erow == E_NAME) { P.typing = 1; P.name[0] = 0; SDL_StartTextInput(); }
      else if (P.erow == E_KID) prof_set_kid(P.idx, !prof_kid(P.idx));
      else if (P.erow == E_DEL) { prof_remove(P.idx); P.edit = 0; P.idx = 0; board_invalidate(); }
      else edit_close();
      sound_play(SND_TOGGLE); }
    return 1; }
  if (k == K_BACK) { if (P.manage) { P.manage = 0; return 1; } if (!P.atStart || sb_signed_in()) close_(); return 1; }
  if (P.zone == 0) {
    if (k == K_LEFT && P.idx > 0) { P.idx--; sound_play(SND_FOCUS); }
    else if (k == K_RIGHT && P.idx < nslots() - 1) { P.idx++; sound_play(SND_FOCUS); }
    else if (k == K_DOWN) { P.zone = 1; sound_play(SND_FOCUS); }
    else if (k == K_OK) { if (P.idx >= prof_count()) { int n = prof_add(); if (n >= 0) edit_open(n); } else if (P.manage) edit_open(P.idx); else go_in(P.idx); } }
  else { if (k == K_UP) { P.zone = 0; sound_play(SND_FOCUS); } else if (k == K_OK) { P.manage = !P.manage; P.zone = 0; sound_play(SND_TAB); } }
  return 1; }
static void text(const char *u) { if (!P.typing) return; size_t n = strlen(P.name), m = strlen(u); if (n + m < 40) { memcpy(P.name + n, u, m + 1); } }
static void tick(float dt) { tween_tick(&P.a, dt); tween_tick(&P.f, dt); orbs_step(&P.orbs, dt, 0, 1, 1); if (P.closing && P.a.v < 0.01f) { P.open = 0; P.closing = 0; } }
static void row(float y, const char *lb, const char *val, int foc) {
  float x = SB_W / 2 - 420, w = 840; if (foc) gfx_rrect(x, y, w, 76, 38, WHITE_A(235)); else gfx_rrect(x, y, w, 76, 38, WHITE_A(22));
  uint32_t c = foc ? RGB(20, 18, 30) : WHITE_A(230); text_draw(lb, x + 36, y + 38 - text_line_height(30) / 2, 30, 700, c, 0, 0, ALIGN_LEFT);
  if (val) text_draw(val, x + w - 36, y + 38 - text_line_height(28) / 2, 28, 600, foc ? RGB(60, 50, 90) : WHITE_A(180), 0, 0, ALIGN_RIGHT); }
static void draw(void) {
  if (!P.open) return; float a = P.a.v; if (a <= 0.001f) return; gfx_opacity_push(a);
  gfx_rect(0, 0, SB_W, SB_H, PAGE); orbs_draw(&P.orbs, 1);
  lumen_mark_draw(SB_W / 2 - 150, 120, 34, 3, 0); text_draw("LUMEN", SB_W / 2 - 100, 120 - text_line_height(40) / 2, 40, 800, WHITE_A(240), 10, 0, ALIGN_LEFT);
  if (P.edit) {
    text_draw(T("Edit profile"), SB_W / 2, 210, 60, 800, WHITE_A(255), -1, 0, ALIGN_CENTER);
    prof_avatar_draw(prof_avatar(P.idx), SB_W / 2, 400, 110, P.name, 1);
    if (P.erow == E_AV) { gfx_ring(SB_W / 2 - 122, 278, 244, 244, 122, 6, WHITE_A(240)); text_draw("\xe2\x97\x80", SB_W / 2 - 170, 380, 40, 700, WHITE_A(220), 0, 0, ALIGN_CENTER); text_draw("\xe2\x96\xb6", SB_W / 2 + 170, 380, 40, 700, WHITE_A(220), 0, 0, ALIGN_CENTER); }
    float y = 560; char nm[80]; snprintf(nm, sizeof nm, "%s%s", P.name, P.typing ? "|" : "");
    row(y, T("Name"), nm, P.erow == E_NAME); y += 92;
    if (P.idx > 0) { row(y, T("Kids profile"), T(prof_kid(P.idx) ? "On" : "Off"), P.erow == E_KID); y += 92; row(y, T("Remove profile"), NULL, P.erow == E_DEL); y += 92; }
    row(y, T("Done"), NULL, P.erow == E_DONE);
    if (P.typing) text_draw(T("Type the name with the keyboard, OK when done"), SB_W / 2, SB_H - 90, 26, 500, WHITE_A(180), 0, 0, ALIGN_CENTER);
    gfx_opacity_pop(); return; }
  text_draw(T(P.manage ? "Manage profiles" : "Who's watching?"), SB_W / 2, 250, 72, 800, WHITE_A(255), -1.5f, 0, ALIGN_CENTER);
  text_draw(T(P.manage ? "Choose a profile to edit it" : "Each profile with its own Stremio account"), SB_W / 2, 350, 30, 500, WHITE_A(180), 0, 0, ALIGN_CENTER);
  int n = prof_count(), ns = nslots(); float R = 100, gap = 90, tw = ns * 2 * R + (ns - 1) * gap, x0 = SB_W / 2 - tw / 2 + R, cy = 560;
  for (int i = 0; i < ns; i++) { float cx = x0 + i * (2 * R + gap); int f = P.zone == 0 && P.idx == i; float s = f ? 1.08f : 1;
    gfx_scale_push(s, s, cx, cy);
    if (i < n) { prof_avatar_draw(prof_avatar(i), cx, cy, R, prof_name(i), f ? 1 : 0.8f); if (P.manage) { gfx_circle(cx, cy, R, BLACK_A(90)); icon_draw("set_prof", cx - 30, cy - 30, 60, WHITE_A(230)); }
      if (i == prof_cur() && sb_signed_in()) { gfx_circle(cx + R * 0.72f, cy + R * 0.72f, 16, RGB(0xf5, 0xb0, 0x3b)); gfx_ring(cx + R * 0.72f - 16, cy + R * 0.72f - 16, 32, 32, 16, 4, PAGE); } }
    else { gfx_circle(cx, cy, R, WHITE_A(f ? 60 : 30)); text_draw("+", cx, cy - text_line_height(90) / 2, 90, 300, WHITE_A(230), 0, 0, ALIGN_CENTER); }
    if (f) gfx_ring(cx - R - 10, cy - R - 10, 2 * R + 20, 2 * R + 20, R + 10, 6, WHITE_A(245));
    gfx_scale_pop();
    const char *nm = i < n ? prof_name(i) : T("Add profile"); text_draw(nm, cx, cy + R + 34, 32, f ? 800 : 600, WHITE_A(f ? 255 : 200), 0, 2 * R + gap - 10, ALIGN_CENTER);
    if (i < n && prof_kid(i)) text_draw(T("KIDS"), cx, cy + R + 80, 20, 800, RGB(0xf5, 0xb0, 0x3b), 2, 0, ALIGN_CENTER); }
  { const char *b = T(P.manage ? "Done" : "Manage profiles"); float bw = text_width(b, 28, 700, 0) + 80, bx = SB_W / 2 - bw / 2, by = 830; int f = P.zone == 1;
    gfx_rrect(bx, by, bw, 72, 36, f ? WHITE_A(235) : WHITE_A(26)); text_draw(b, SB_W / 2, by + 36 - text_line_height(28) / 2, 28, 700, f ? RGB(20, 18, 30) : WHITE_A(230), 0, 0, ALIGN_CENTER); }
  gfx_opacity_pop();
}
static Screen scr = { NULL, NULL, key, tick, draw, NULL, NULL, text, NULL, NULL };
void profpick_open(int atStart) {
  P.open = 1; P.closing = 0; P.atStart = atStart; P.idx = prof_cur(); P.zone = 0; P.manage = 0; P.edit = 0; P.typing = 0;
  tween_set(&P.a, atStart ? 1 : 0); tween_to(&P.a, 1, 0.4f, EASE_OUT); orbs_place(&P.orbs, 0, 1.4f); app_overlay_keep(&scr); app_overlay = &scr; }
void profpick_tick_start(void) {
  static int done; if (done || !app_splash_done || SCR != SCR_HOME) return; done = 1;
  if (prof_count() > 1 && strcmp(store_get("profAsk", "true"), "false")) profpick_open(1); }
