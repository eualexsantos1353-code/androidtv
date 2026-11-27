#include "core/organize.h"
#include "ui/gfx.h"
#include "ui/icon.h"
#include "ui/text.h"
#include "core/store.h"
#include "core/json.h"
#include "core/i18n.h"
#include "core/store.h"
#include "core/i18n.h"
#include "core/best.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---- etapa-74: the profiles: "profiles" [{name, av, kid}], each one's account kept aside while another is in ---- */
static cJSON *plist(void) {
  cJSON *l = store_node("profiles"); if (cJSON_IsArray(l) && cJSON_GetArraySize(l) > 0) return l;
  cJSON *n = cJSON_CreateArray(); const char *em = sb_user_email(); char nm[64]; snprintf(nm, sizeof nm, "%s", *em ? em : T("Main")); { char *at = strchr(nm, '@'); if (at) *at = 0; } if (nm[0] >= 'a' && nm[0] <= 'z') nm[0] -= 32;
  cJSON *o = cJSON_CreateObject(); cJSON_AddStringToObject(o, "name", nm); cJSON_AddNumberToObject(o, "av", 0); cJSON_AddItemToArray(n, o);
  o = cJSON_CreateObject(); cJSON_AddStringToObject(o, "name", T("Kids")); cJSON_AddNumberToObject(o, "av", 5); cJSON_AddBoolToObject(o, "kid", 1); cJSON_AddItemToArray(n, o);
  store_set_node("profiles", n); return store_node("profiles"); }
int prof_count(void) { return cJSON_GetArraySize(plist()); }
int prof_cur(void) { int p = atoi(store_get("profCur", "0")); return p >= 0 && p < prof_count() ? p : 0; }
static const cJSON *pget(int i) { return cJSON_GetArrayItem(plist(), i); }
const char *prof_name(int i) { const char *n = js_str(pget(i), "name", ""); return *n ? n : T("Profile"); }
int prof_avatar(int i) { int a = (int)js_num(pget(i), "av", i); return a >= 0 && a < PROF_AV ? a : 0; }
int prof_kid(int i) { if (i < 0) i = prof_cur(); return cJSON_IsTrue(cJSON_GetObjectItem(pget(i), "kid")); }
static void pset(int i, const char *k, cJSON *v) { cJSON *l = cJSON_Duplicate(plist(), 1); cJSON *o = cJSON_GetArrayItem(l, i); if (!o) { cJSON_Delete(l); cJSON_Delete(v); return; }
  cJSON_DeleteItemFromObject(o, k); cJSON_AddItemToObject(o, k, v); store_set_node("profiles", l); store_flush(); }
void prof_set_name(int i, const char *name) { if (name && *name) pset(i, "name", cJSON_CreateString(name)); }
void prof_set_avatar(int i, int av) { pset(i, "av", cJSON_CreateNumber((av % PROF_AV + PROF_AV) % PROF_AV)); }
void prof_set_kid(int i, int on) { if (i > 0) pset(i, "kid", cJSON_CreateBool(on)); }
int prof_add(void) { int n = prof_count(); if (n >= PROF_N) return -1; cJSON *l = cJSON_Duplicate(plist(), 1), *o = cJSON_CreateObject(); char nm[32]; snprintf(nm, sizeof nm, "%s %d", T("Profile"), n + 1);
  cJSON_AddStringToObject(o, "name", nm); cJSON_AddNumberToObject(o, "av", n % PROF_AV); cJSON_AddItemToArray(l, o); store_set_node("profiles", l); store_flush(); return n; }
/* the account's own keys: they go aside as "acct_p<N>" while another profile is in */
static const char *AK[] = { "authKey", "user", "addons", "lib", "streams", "notifs", "cwHide", "upHide", "watchLater", "storiesSeen", "watchLog", "roletaHist", "diary", "traktShows", "traktYear", "traktLibSeen" };
#define NAK (int)(sizeof AK / sizeof *AK)
static void stash(int p) { cJSON *o = cJSON_CreateObject(); for (int i = 0; i < NAK; i++) { cJSON *v = store_node(AK[i]); if (v) cJSON_AddItemToObject(o, AK[i], cJSON_Duplicate(v, 1)); store_remove(AK[i]); }
  char k[24]; snprintf(k, sizeof k, "acct_p%d", p); store_set_node(k, o); }
static void unstash(int p) { char k[24]; snprintf(k, sizeof k, "acct_p%d", p); const cJSON *o = store_node(k), *v;
  cJSON_ArrayForEach(v, o) if (v->string) store_set_node(v->string, cJSON_Duplicate(v, 1)); store_remove(k); }
void prof_remove(int i) { int n = prof_count(); if (i <= 0 || i >= n) return; int cur = prof_cur(); if (i == cur) prof_switch(0); cur = prof_cur();
  char k[24]; snprintf(k, sizeof k, "acct_p%d", i); store_remove(k);
  for (int j = i + 1; j < n; j++) { char a[24], b[24]; snprintf(a, sizeof a, "acct_p%d", j); snprintf(b, sizeof b, "acct_p%d", j - 1); cJSON *v = store_node(a); if (v) { store_set_node(b, cJSON_Duplicate(v, 1)); store_remove(a); } }
  cJSON *l = cJSON_Duplicate(plist(), 1); cJSON_DeleteItemFromArray(l, i); store_set_node("profiles", l);
  if (cur > i) { char c[8]; snprintf(c, sizeof c, "%d", cur - 1); store_set("profCur", c); } store_flush(); }
int prof_switch(int to) {
  int cur = prof_cur(); if (to < 0 || to >= prof_count() || to == cur) return sb_signed_in();
  stash(cur); { char c[8]; snprintf(c, sizeof c, "%d", to); store_set("profCur", c); } unstash(to);
  if (!sb_signed_in() && prof_kid(to)) { /* the Kids profile: the first profile's account (its own Home, its own library on this TV) */
    char k[24]; snprintf(k, sizeof k, "acct_p%d", 0); const cJSON *o = to == 0 ? NULL : store_node(k);
    const char *keep[] = { "authKey", "user", "addons" }; for (int i = 0; i < 3; i++) { const cJSON *v = cJSON_GetObjectItem(o, keep[i]); if (v) store_set_node(keep[i], cJSON_Duplicate(v, 1)); } }
  store_flush(); sb_account_swapped();
  fprintf(stderr, "[profiles] %d -> %d (%s), signed in: %d\n", cur, to, prof_kid(to) ? "kids" : "own account", sb_signed_in());
  return sb_signed_in(); }
/* the avatars: a gradient disc and a figure (or the name's first letter) */
static const uint32_t AVC[PROF_AV][2] = { { 0x8b5cf6, 0xec4899 }, { 0x3b82f6, 0x06b6d4 }, { 0xf97316, 0xef4444 }, { 0x22c55e, 0x14b8a6 }, { 0xf59e0b, 0xf97316 }, { 0xf472b6, 0xa855f7 }, { 0x6366f1, 0x8b5cf6 }, { 0x64748b, 0x334155 } };
static const char *AVI[PROF_AV] = { "av_heart", "av_rocket", "av_game", "av_paw", "av_crown", "av_star", "av_music", NULL };
void prof_avatar_draw(int av, float cx, float cy, float r, const char *name, float a) {
  av = (av % PROF_AV + PROF_AV) % PROF_AV; uint32_t c0 = AVC[av][0], c1 = AVC[av][1];
  gfx_opacity_push(a); gfx_rrect_v(cx - r, cy - r, 2 * r, 2 * r, r, RGB((c0 >> 16) & 255, (c0 >> 8) & 255, c0 & 255), RGB((c1 >> 16) & 255, (c1 >> 8) & 255, c1 & 255));
  gfx_rrect_v(cx - r, cy - r, 2 * r, r, r, WHITE_A(40), WHITE_A(0));
  if (AVI[av]) icon_draw(AVI[av], cx - r * 0.5f, cy - r * 0.5f, r, WHITE_A(245));
  else { char l[8] = "?"; if (name && *name) { int n = 1; while (n < 4 && (name[n] & 0xc0) == 0x80) n++; snprintf(l, sizeof l, "%.*s", n, name); } text_draw(l, cx, cy - text_line_height((int)r) / 2, (int)r, 800, WHITE_A(250), 0, 0, ALIGN_CENTER); }
  gfx_opacity_pop(); }

static unsigned fnv(const char *s, unsigned h) { while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; } return h; }
void catorg_key(const CatalogDef *d, char area, char *out, size_t cap) { unsigned h = fnv(d->type, fnv(d->path[0] ? d->path : d->url, 2166136261u)); snprintf(out, cap, "co%c_%08x", area, h); }
int catorg_apply(CatalogDef *d, int n, char area) {
  if (n <= 0) return n; CatalogDef *tmp = malloc(sizeof *tmp * (size_t)n); if (!tmp) return n; int m = 0;
  for (int pass = 0; pass < 2; pass++) for (int i = 0; i < n; i++) { char k[32]; catorg_key(&d[i], area, k, sizeof k); const char *v = store_get(k, "show");
    if (!strcmp(v, "hide")) continue; if ((pass == 0) != !strcmp(v, "top")) continue; tmp[m++] = d[i]; }
  memcpy(d, tmp, sizeof *tmp * (size_t)m); free(tmp); return m; }

/* ---- the streams' filter ---- */
static void low(const char *in, char *out, size_t cap) { size_t o = 0; for (; *in && o + 1 < cap; in++) out[o++] = (char)tolower((unsigned char)*in); out[o] = 0; }
static double gb_of(const Stream *s, const char *t) {
  if (s->size > 0) return s->size / 1073741824.0;
  for (const char *p = t; *p; p++) if (isdigit((unsigned char)*p)) { char *e; double v = strtod(p, &e); while (*e == ' ') e++; if (!strncmp(e, "gb", 2)) return v; if (!strncmp(e, "mb", 2)) return v / 1024; p = e - 1 > p ? e - 1 : p; }
  return -1; }
int sfilter_on(void) { return strcmp(store_get("sf_res", "any"), "any") || strcmp(store_get("sf_lang", "any"), "any") || !strcmp(store_get("sf_cam", "false"), "true") || strcmp(store_get("sf_size", "any"), "any") || !strcmp(store_get("sf_nohdr", "false"), "true"); }
int sfilter_pass(const Stream *s) {
  if (!sfilter_on()) return 1;
  char all[1400]; snprintf(all, sizeof all, "%s %s %s", s->name, s->title, s->filename); char t[1400]; low(all, t, sizeof t);
  const char *rs = best_res_of(all); int res = !rs ? 0 : !strcmp(rs, "4K") ? 2160 : !strcmp(rs, "SD") ? 480 : atoi(rs);
  const char *fr = store_get("sf_res", "any");
  if (!strcmp(fr, "4k") && res != 2160) return 0;
  if (!strcmp(fr, "1080") && res > 1080) return 0; if (!strcmp(fr, "720") && res > 720) return 0;
  if (!strcmp(fr, "1080up") && res && res < 1080) return 0;
  if (!strcmp(store_get("sf_lang", "any"), "pt") && !(strstr(t, "dublad") || strstr(t, "dual") || strstr(t, "pt-br") || strstr(t, "ptbr") || strstr(t, "portugu") || strstr(t, "nacional") || strstr(t, "\xf0\x9f\x87\xa7\xf0\x9f\x87\xb7") || strstr(t, " por ") || strstr(t, "multi"))) return 0;
  if (!strcmp(store_get("sf_cam", "false"), "true")) { const char *C[] = { "cam", "hdcam", "camrip", "telesync", "hdts", " ts ", ".ts.", "telecine", "hdtc", "screener", "scr " }; for (int i = 0; i < 11; i++) if (strstr(t, C[i])) return 0; }
  if (!strcmp(store_get("sf_nohdr", "false"), "true")) { char hd[32]; best_hdr_of(all, hd, sizeof hd); if (hd[0]) return 0; }
  const char *fs = store_get("sf_size", "any"); if (strcmp(fs, "any")) { double g = gb_of(s, t), mx = atof(fs); if (g > 0 && g > mx) return 0; }
  return 1; }
