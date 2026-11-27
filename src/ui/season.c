#include "ui/season.h"
#include "ui/gfx.h"
#include "ui/icon.h"
#include "core/app.h"
#include "core/store.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* Easter (Anonymous Gregorian), for Carnival: Easter minus 47 days is its Tuesday */
static void easter(int y, int *m, int *d) { int a = y % 19, b = y / 100, c = y % 100, dd = b / 4, e = b % 4, f = (b + 8) / 25, g = (b - f + 1) / 3, h = (19 * a + b - dd - g + 15) % 30, i = c / 4, k = c % 4, l = (32 + 2 * e + 2 * i - h - k) % 7, mm = (a + 11 * h + 22 * l) / 451;
  *m = (h + l - 7 * mm + 114) / 31; *d = ((h + l - 7 * mm + 114) % 31) + 1; }
int season_now(void) {
  const char *f = getenv("SB_SEASON"); if (f && *f) return !strcmp(f, "xmas") ? SEASON_XMAS : !strcmp(f, "newyear") ? SEASON_NEWYEAR : !strcmp(f, "carnival") ? SEASON_CARNIVAL : !strcmp(f, "halloween") ? SEASON_HALLOWEEN : SEASON_NONE;
  if (!strcmp(store_get("seasonal", "true"), "false")) return SEASON_NONE;
  time_t t = time(NULL); struct tm tm; localtime_r(&t, &tm); int mo = tm.tm_mon + 1, d = tm.tm_mday;
  if ((mo == 12 && d == 31 && tm.tm_hour >= 22) || (mo == 1 && d == 1 && tm.tm_hour < 3)) return SEASON_NEWYEAR;
  if (mo == 12 || (mo == 1 && d <= 6)) return SEASON_XMAS;
  if (mo == 10 && d >= 25) return SEASON_HALLOWEEN;
  { int em, ed; easter(tm.tm_year + 1900, &em, &ed); struct tm e = { 0 }; e.tm_year = tm.tm_year; e.tm_mon = em - 1; e.tm_mday = ed - 47; e.tm_hour = 12; time_t tue = mktime(&e);
    double dd = difftime(t, tue) / 86400.0; if (dd >= -3.5 && dd <= 1.5) return SEASON_CARNIVAL; } /* Saturday to Ash Wednesday */
  return SEASON_NONE;
}

#define NP 90
typedef struct { float x, y, vx, vy, r, ph, life; uint32_t c; } P;
static P p[NP]; static int kind = -1; static double nextBurst;
static float frand(void) { return (float)rand() / (float)RAND_MAX; }
static const uint32_t CONF[6] = { 0xff3b82f6u, 0xffef4444u, 0xfff59e0bu, 0xff22c55eu, 0xffd946efu, 0xff06b6d4u };
static uint32_t rgb_hex(uint32_t h) { return RGB((h >> 16) & 255, (h >> 8) & 255, h & 255); }
static void seed(P *q, int k, int top) {
  memset(q, 0, sizeof *q); q->x = frand() * SB_W; q->y = top ? -20 - frand() * 80 : frand() * SB_H; q->ph = frand() * 6.28f;
  if (k == SEASON_XMAS) { q->r = 2 + frand() * 4; q->vy = 25 + q->r * 9; q->vx = -8 + frand() * 16; q->c = WHITE_A((int)(110 + 120 * frand())); }
  else if (k == SEASON_CARNIVAL) { q->r = 4 + frand() * 4; q->vy = 60 + frand() * 70; q->vx = -20 + frand() * 40; q->c = rgb_hex(CONF[rand() % 6] & 0xffffff); }
  else if (k == SEASON_HALLOWEEN) { q->r = 7 + frand() * 6; q->vy = 30 + frand() * 30; q->vx = 10 + frand() * 30; q->c = rgb_hex(frand() < 0.5f ? 0xea580c : 0xb45309); }
}
static void burst(void) { float cx = 200 + frand() * (SB_W - 400), cy = 120 + frand() * 380; uint32_t c = rgb_hex(CONF[rand() % 6] & 0xffffff); int made = 0;
  for (int i = 0; i < NP && made < 36; i++) if (p[i].life <= 0) { float a = made / 36.f * 6.283f, sp = 160 + frand() * 120; p[i].x = cx; p[i].y = cy; p[i].vx = cosf(a) * sp; p[i].vy = sinf(a) * sp; p[i].r = 3.5f; p[i].life = 1.6f; p[i].c = c; made++; } }
void season_draw(float dt, float alpha) {
  int k = season_now(); if (k == SEASON_NONE || alpha <= 0.01f) { kind = k; return; }
  if (dt > 0.1f) dt = 0.1f;
  if (k != kind) { kind = k; for (int i = 0; i < NP; i++) { if (k == SEASON_NEWYEAR) p[i].life = 0; else seed(&p[i], k, 0); } nextBurst = 0; }
  gfx_opacity_push(alpha);
  if (k == SEASON_HALLOWEEN) gfx_rrect_v(0, SB_H - 360, SB_W, 360, 0, RGBA(234, 88, 12, 0), RGBA(234, 88, 12, 46));
  if (k == SEASON_NEWYEAR) { if (app_now() >= nextBurst) { burst(); nextBurst = app_now() + 0.7 + frand() * 1.1; }
    for (int i = 0; i < NP; i++) if (p[i].life > 0) { P *q = &p[i]; q->life -= dt; q->vy += 70 * dt; q->vx *= 1 - 0.9f * dt; q->vy *= 1 - 0.6f * dt; q->x += q->vx * dt; q->y += q->vy * dt;
      float a = q->life > 1 ? 1 : q->life; if (a > 0) { gfx_circle(q->x, q->y, q->r * 2.6f, col_alpha(q->c, 0.18f * a)); gfx_circle(q->x, q->y, q->r, col_alpha(q->c, a)); } } }
  else for (int i = 0; i < NP; i++) { P *q = &p[i]; q->ph += dt * (k == SEASON_XMAS ? 1.2f : 3); q->y += q->vy * dt; q->x += (q->vx + sinf(q->ph) * (k == SEASON_XMAS ? 14 : 30)) * dt;
    if (q->y > SB_H + 20 || q->x < -40 || q->x > SB_W + 40) seed(q, k, 1);
    if (k == SEASON_XMAS) gfx_circle(q->x, q->y, q->r, q->c);
    else if (k == SEASON_CARNIVAL) { float w = q->r * (0.4f + 0.6f * fabsf(sinf(q->ph))); gfx_rect(q->x - w, q->y - q->r * 0.6f, 2 * w, q->r * 1.2f, q->c); }
    else icon_draw_rot("leaf", q->x - q->r, q->y - q->r, q->r * 2, col_alpha(q->c, 0.85f), q->ph * 40); }
  gfx_opacity_pop();
}
void season_hat(float cx, float cy, float R, float a) {
  if (season_now() != SEASON_XMAS || a <= 0.01f) return;
  float S = R * 1.25f, x = cx - R * 1.05f, y = cy - R * 1.55f;
  gfx_opacity_push(a); icon_draw_rot("hat_red", x, y, S, RGB(0xdc, 0x26, 0x26), -18); icon_draw_rot("hat_fur", x, y, S, WHITE_A(250), -18); gfx_opacity_pop();
}
