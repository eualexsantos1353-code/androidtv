/* The cast receiver (core/cast.c) joined to this app: a phone's play goes through the player (its subtitles, the
   moment to start at), its keys through the app's own keys; the pairing code shows on a small glass pill at the top,
   and a phone connecting or going is said once. */
#include "core/castlink.h"
#include "core/cast.h"
#include "core/app.h"
#include "core/keys.h"
#include "core/store.h"
#include "core/i18n.h"
#include "core/update.h"
#include "screens/player.h"
#include "core/live.h"
#include "screens/settings.h"
#include "screens/board.h"
#include "screens/btrailer.h"
#include "cJSON.h"
#include "ui/gfx.h"
#include "ui/text.h"
#include "ui/glass.h"
#include "platform/plat.h"
#include "core/notify.h"
#include "core/insight.h"
#include "core/tmdb.h"
#include "core/net.h"
#include "core/json.h"
#include "core/stremio.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

static char codeNow[8], castTitle[256]; static Tween codeA;

/* ---- the assistant (JARVIS on the home server): what it asks beyond a phone's remote ---- */
static char *reply(cJSON *j) { char *s = cJSON_PrintUnformatted(j); cJSON_Delete(j); return s; }
static void norm(const char *in, char *out, size_t cap) { /* lower case, accents off, spaces kept: "Globo MG" ~ "globo mg" */
  size_t o = 0; for (const unsigned char *p = (const unsigned char *)in; *p && o + 1 < cap; p++) {
    unsigned c = *p; if (c == 0xc3 && p[1]) { unsigned d = p[1]; p++; c = (d >= 0xa0 && d <= 0xa5) || (d >= 0x80 && d <= 0x85) ? 'a' : (d >= 0xa8 && d <= 0xab) || (d >= 0x88 && d <= 0x8b) ? 'e' : (d >= 0xac && d <= 0xaf) || (d >= 0x8c && d <= 0x8f) ? 'i' : (d >= 0xb2 && d <= 0xb6) || (d >= 0x92 && d <= 0x96) ? 'o' : (d >= 0xb9 && d <= 0xbc) || (d >= 0x99 && d <= 0x9c) ? 'u' : (d == 0xa7 || d == 0x87) ? 'c' : ' '; }
    else if (c >= 'A' && c <= 'Z') c += 32; else if (c >= 0x80) c = ' ';
    if (c == ' ' && (!o || out[o - 1] == ' ')) continue; out[o++] = (char)c; }
  while (o && out[o - 1] == ' ') o--; out[o] = 0; }
static struct { int state, n; unsigned tok; char wk[16]; cJSON *list; } NW; /* the films out this week (TMDB), asked once a day */
static void on_news(int st, const unsigned char *b, size_t len, void *ud) { (void)len; if ((unsigned)(uintptr_t)ud != NW.tok) return; cJSON *j = st == 200 ? net_json(b) : NULL; cJSON_Delete(NW.list); NW.list = cJSON_CreateArray(); const cJSON *r;
  cJSON_ArrayForEach(r, js_arr(j, "results")) { if (cJSON_GetArraySize(NW.list) >= 10) break; cJSON *o = cJSON_CreateObject(); cJSON_AddStringToObject(o, "name", js_str(r, "title", "")); cJSON_AddStringToObject(o, "date", js_str(r, "release_date", "")); cJSON_AddNumberToObject(o, "id", js_num(r, "id", 0)); cJSON_AddItemToArray(NW.list, o); }
  net_json_free(j); NW.state = 2; }
static char *on_ask(const char *t, const void *msgv) {
  const cJSON *m = (const cJSON *)msgv; cJSON *r = cJSON_CreateObject(); /* (questions are not activity: the screen saver stays) */
  if (!strcmp(t, "cw?")) { /* Continue Watching, newest first */
    cJSON_AddStringToObject(r, "t", "cw"); cJSON *a = cJSON_AddArrayToObject(r, "items"); ItemList l; memset(&l, 0, sizeof l); lib_cw_items(&l);
    for (int i = 0; i < l.n && i < 6; i++) { const Item *it = &l.items[i]; CwInfo ci; memset(&ci, 0, sizeof ci); lib_cw_info(it, &ci); cJSON *o = cJSON_CreateObject();
      cJSON_AddStringToObject(o, "type", it->type); cJSON_AddStringToObject(o, "id", it->id); cJSON_AddStringToObject(o, "name", it->name); cJSON_AddStringToObject(o, "vid", ci.vid[0] ? ci.vid : it->videoId); cJSON_AddNumberToObject(o, "pct", ci.pct); cJSON_AddNumberToObject(o, "new", it->newEp); cJSON_AddItemToArray(a, o); }
    itemlist_clear(&l); return reply(r); }
  if (!strcmp(t, "cal?")) { /* the followed series' episodes (aired and coming) */
    cJSON_AddStringToObject(r, "t", "cal"); cJSON *a = cJSON_AddArrayToObject(r, "items"); ItemList l; memset(&l, 0, sizeof l); int n = notify_calendar(&l); cJSON_AddNumberToObject(r, "known", n >= 0);
    for (int i = 0; i < l.n && i < 200; i++) { const Item *it = &l.items[i]; cJSON *o = cJSON_CreateObject(); cJSON_AddStringToObject(o, "sid", it->id); cJSON_AddStringToObject(o, "name", it->name); cJSON_AddStringToObject(o, "vid", it->videoId);
      cJSON_AddStringToObject(o, "ep", it->metaLine); cJSON_AddStringToObject(o, "title", it->epName); cJSON_AddNumberToObject(o, "at", it->airAt); cJSON_AddItemToArray(a, o); }
    itemlist_clear(&l); return reply(r); }
  if (!strcmp(t, "chan")) { /* a live channel by its name or its number in All channels: started */
    char q[128]; norm(js_str(m, "q", ""), q, sizeof q); int num = atoi(q), tot = 0, best = -1, bs = 0; for (int c = 0; c < live_catalogs(); c++) { int k = live_catalog(c, NULL, NULL); if (k > 0) tot += k; }
    const LiveCh **all = tot ? malloc(sizeof(*all) * (size_t)tot) : NULL; int n = 0;
    if (all) for (int c = 0; c < live_catalogs(); c++) { const LiveCh *ch; int k = live_catalog(c, NULL, &ch); for (int i = 0; i < k; i++) all[n++] = &ch[i]; }
    if (num > 0 && num <= n && (int)strspn(q, "0123456789") == (int)strlen(q)) best = num - 1;
    else if (*q) for (int i = 0; i < n; i++) { char nm[128]; norm(all[i]->name, nm, sizeof nm); int sc = !strcmp(nm, q) ? 100 : !strncmp(nm, q, strlen(q)) ? 60 : strstr(nm, q) ? 40 : 0; if (sc && live_reach(all[i]->id) > 0) sc += 5; if (sc > bs) { bs = sc; best = i; } }
    cJSON_AddStringToObject(r, "t", "chan"); cJSON_AddNumberToObject(r, "ok", best >= 0); cJSON_AddNumberToObject(r, "count", n);
    if (best >= 0) { cJSON_AddStringToObject(r, "name", all[best]->name); cJSON_AddNumberToObject(r, "num", best + 1); app_activity(); app_overlay_close(); live_start_list(all, n, best, player_open()); }
    free(all); return reply(r); }
  if (!strcmp(t, "libadd")) { /* a title into the Library */
    Item it; memset(&it, 0, sizeof it); snprintf(it.type, sizeof it.type, "%s", js_str(m, "type", "movie")); snprintf(it.id, sizeof it.id, "%s", js_str(m, "id", "")); snprintf(it.name, sizeof it.name, "%s", js_str(m, "name", "")); snprintf(it.poster, sizeof it.poster, "%s", js_str(m, "poster", ""));
    cJSON_AddStringToObject(r, "t", "libadd"); int had = *it.id && lib_has(it.id); if (*it.id && !had) { lib_toggle(&it); board_invalidate(); notify_refresh(); }
    cJSON_AddNumberToObject(r, "ok", *it.id != 0); cJSON_AddNumberToObject(r, "already", had); return reply(r); }
  if (!strcmp(t, "cast?")) { /* who plays in what is on (the X-Ray's cast) */
    char ty[16], id[96], vid[96]; player_cast_ident(ty, sizeof ty, id, sizeof id, vid, sizeof vid); cJSON_AddStringToObject(r, "t", "cast"); cJSON_AddStringToObject(r, "id", id);
    int s = -1, e = -1; { const char *c2 = strrchr(vid, ':'); if (c2 && !strcmp(ty, "series")) { const char *p = c2 - 1; while (p > vid && *p != ':') p--; if (*p == ':') { s = atoi(p + 1); e = atoi(c2 + 1); } } }
    const InsightFace *fc; int nf = *id ? insight_get(ty, id, s, e, &fc) : 0; cJSON_AddNumberToObject(r, "state", !*id ? 0 : nf > 0 || insight_state() == 2 ? 2 : 1); cJSON *a = cJSON_AddArrayToObject(r, "faces");
    for (int i = 0; i < nf && i < 12; i++) { cJSON *o = cJSON_CreateObject(); cJSON_AddStringToObject(o, "name", fc[i].name); cJSON_AddStringToObject(o, "role", fc[i].role); cJSON_AddItemToArray(a, o); }
    return reply(r); }
  if (!strcmp(t, "news?")) { /* the films out this week, from TMDB (asked once a week's day) */
    time_t now = time(NULL); struct tm tm; localtime_r(&now, &tm); char d0[16], d1[16]; strftime(d0, sizeof d0, "%Y-%m-%d", &tm); time_t e7 = now + 7 * 86400; struct tm t7; localtime_r(&e7, &t7); strftime(d1, sizeof d1, "%Y-%m-%d", &t7);
    if (strcmp(NW.wk, d0) || NW.state == 0) { snprintf(NW.wk, sizeof NW.wk, "%s", d0); NW.state = 1; NW.tok++; char p[300]; snprintf(p, sizeof p, "/discover/movie?language=%s&region=BR&sort_by=popularity.desc&primary_release_date.gte=%s&primary_release_date.lte=%s", i18n_lang(), d0, d1);
      if (!tmdb_has_key() || !tmdb_request(p, on_news, (void *)(uintptr_t)NW.tok)) NW.state = 2; }
    cJSON_AddStringToObject(r, "t", "news"); cJSON_AddNumberToObject(r, "state", NW.state); cJSON_AddItemToObject(r, "items", NW.state == 2 && NW.list ? cJSON_Duplicate(NW.list, 1) : cJSON_CreateArray()); return reply(r); }
  cJSON_Delete(r); return NULL;
}


static const char *tv_name(void) { return plat_device_name(); }
/* a channel a phone started that will not play: the phone told why (its remote leaves "loading"), in this TV's words */
static void on_live_fail(const char *id, const char *code, int back) { if (!cast_phones()) return; fprintf(stderr, "[cast] channel %s did not play (%s)%s: phones told\n", id, code, back ? ", the one before plays on" : "");
  cast_error(T("This channel isn't broadcasting right now"), code, id, back); }
static void on_play(const CastPlay *p) {
  app_activity(); app_overlay_close(); /* (the guide or a card open: closed, the film is not hidden behind it) */
  snprintf(castTitle, sizeof castTitle, "%s", p->title);
  if (live_type(p->type) && *p->id) { /* a channel from a phone: started as one picked here (its own stream found, the channel player, Up / Down along its line-up) */
    Item it; memset(&it, 0, sizeof it); snprintf(it.type, sizeof it.type, "%s", p->type); snprintf(it.id, sizeof it.id, "%s", p->id); snprintf(it.name, sizeof it.name, "%s", p->title); snprintf(it.poster, sizeof it.poster, "%s", p->poster); snprintf(it.logo, sizeof it.logo, "%s", p->logo);
    char ty[16], pid[96], pv[96]; player_cast_ident(ty, sizeof ty, pid, sizeof pid, pv, sizeof pv); live_start(&it, NULL, 0, player_open() && live_type(ty)); return; } /* (a channel already on: switched in its player, as Up / Down) */
  Stream s; memset(&s, 0, sizeof s); snprintf(s.url, sizeof s.url, "%s", p->url); snprintf(s.name, sizeof s.name, "%s", p->title); snprintf(s.title, sizeof s.title, "%s", p->title);
  const char *type = *p->type ? p->type : "movie", *id = *p->id ? p->id : "cast"; char vid[SB_VID];
  if (!strcmp(type, "series") && p->season > 0) snprintf(vid, sizeof vid, "%s:%d:%d", id, p->season, p->episode); else snprintf(vid, sizeof vid, "%s", id);
  const char *urls[16], *langs[16]; for (int i = 0; i < p->nsubs && i < 16; i++) { urls[i] = p->subs[i].url; langs[i] = p->subs[i].lang; }
  player_next_subs(urls, langs, p->nsubs); player_next_art(p->title, p->poster, p->background, p->logo);
  player_play_at(&s, type, id, vid, p->pos); /* pos 0: where it was left, as a play on the TV */
}
/* the phone names a title and this TV finds its stream (its own best-stream choice, as with best stream on here): the
   title's pictures at once, "choosing" told to the phone while it searches, then it plays from pos (0: from the start) */
static void on_playbest(const CastPlay *p) {
  app_activity(); app_overlay_close(); /* (the first start's guide open: it stayed over the film, which played unseen behind it) */
  snprintf(castTitle, sizeof castTitle, "%s", p->title);
  const char *type = *p->type ? p->type : "movie", *id = p->id; char vid[SB_VID];
  if (*p->vid) snprintf(vid, sizeof vid, "%s", p->vid); else if (!strcmp(type, "series") && p->season > 0) snprintf(vid, sizeof vid, "%s:%d:%d", id, p->season, p->episode); else snprintf(vid, sizeof vid, "%s", id);
  player_next_subs(NULL, NULL, 0); player_next_art(p->title, p->poster, p->background, p->logo);
  player_cast_best(type, id, vid, p->pos);
}
static void on_cmd(int cmd, double pos) { static const char *N[] = { "", "pause", "resume", "seek", "stop", "playnow" }; if (cmd > 0 && cmd < 6) fprintf(stderr, "[cast] %s from the phone\n", N[cmd]); app_activity(); player_cast(cmd, pos); } /* (a phone's pause is activity: the screen saver never comes at once over it) */
static void on_track(const char *k, const char *id, int ms) { app_activity(); player_cast_track(k, id, ms); }
static int on_state(double *pos, double *dur, char *title, int cap) { int s = player_cast_state(pos, dur, title, cap); if (s && !title[0]) snprintf(title, (size_t)cap, "%s", castTitle); return s; } /* (no title known here: the phone's own) */
static void on_key(int k) {
  if (k == CAST_K_HOME) { if (player_open()) player_cast(4, 0); else app_go(SCR_HOME, NULL, 1); return; }
  static const int M[] = { 0, K_UP, K_DOWN, K_LEFT, K_RIGHT, K_OK, K_BACK, 0, K_PLAY, K_PAUSE };
  int ak = k > 0 && k < 10 ? M[k] : 0; if (!ak) return; app_key(ak, 1, 0); app_key(ak, 0, 0);
}
/* the phone's account: this TV follows it (signed out, or another user: signed in as the phone is, its addons and
   library brought down and Home made again, as a sign-in here); the same user, or a phone signed out: left as it is */
static void on_account(const char *ak, const char *userJson) {
  if (!ak || !*ak) return; cJSON *u = cJSON_Parse(userJson ? userJson : ""); const cJSON *id = cJSON_GetObjectItemCaseSensitive(u, "_id");
  int same = sb_signed_in() && cJSON_IsString(id) && sb_user_id() && !strcmp(sb_user_id(), id->valuestring); cJSON_Delete(u);
  if (same || (sb_signed_in() && !strcmp(sb_auth_key(), ak))) return;
  app_overlay_close(); /* (the sign-in opens: nothing left over it) */
  fprintf(stderr, "[cast] account from the phone: %s\n", sb_signed_in() ? "another user" : "signing in");
  if (player_open()) player_cast(4, 0);
  if (sb_signed_in()) { sb_forget_local(); board_invalidate(); }
  toast(T("Signing in with your phone\xe2\x80\x99s account"), 2500, NULL);
  extern void login_finish(const char *); if (SCR != SCR_LOGIN) app_go(SCR_LOGIN, NULL, 1); login_finish(ak); /* the sign-in's own loading, then Home */
}
static void on_pref(const char *k, const char *v) { static double saidAt = -10; if (settings_apply_pref(k, v) && !player_open() && app_now() - saidAt > 3) { saidAt = app_now(); toast(T("Settings updated from your phone"), 2000, NULL); } } /* (once for a phone's settings sent together: each one changed had its own toast, nine at a connect) */ /* (one short notice, no key cap before it; none while a title plays: the change shows itself, the subtitles moving or changing) */
static void on_pairing(const char *code) { snprintf(codeNow, sizeof codeNow, "%s", code); }
static void on_connected(int n) { static int was; if (n > was && !player_open()) btr_cast_hold(20); /* a phone just connected: its play comes next, no trailer flashes before it */ if (!player_open()) { if (n > was) toast(T("Phone connected"), 2500, NULL); else if (n < was && !n) toast(T("Phone disconnected"), 2500, NULL); } was = n; } /* (none while a film plays: a phone put away or locked closes its connection, normal use, not news) */

void castlink_start(void) {
  static CastHooks h; const char *dn = plat_device_name();
  h.plat = strstr(dn, "webOS") ? "webos" : strstr(dn, "Android") ? "androidtv" : strstr(dn, "Tizen") ? "tizen" : "mac"; h.ver = update_current(); h.name = tv_name;
  h.get = store_get; h.set = store_set; h.play = on_play; h.playbest = on_playbest; h.cmd = on_cmd; h.key = on_key; h.state = on_state; h.ident = player_cast_ident; h.info = player_cast_info; h.chips = player_cast_chips; h.ask = on_ask; h.p2p = player_p2p_line; h.sync = player_cast_sync; h.pairing = on_pairing; h.connected = on_connected; h.account = on_account; h.pref = on_pref; h.tracks = player_cast_tracks; h.track = on_track; h.previews = player_cast_previews;
  live_fail_hook = on_live_fail;
  cast_start(&h);
}
void castlink_tick(float dt) { cast_tick(); tween_to(&codeA, codeNow[0] ? 1 : 0, 0.3f, EASE_CSS_EASE); tween_tick(&codeA, dt); }
/* the code to type on the phone: a glass pill at the top middle, over everything, while it is asked for */
void castlink_draw(void) {
  float a = codeA.v; if (a <= 0.004f) return; static char shown[8]; if (codeNow[0]) snprintf(shown, sizeof shown, "%s", codeNow);
  const char *msg = T("A phone wants to connect"), *sub = T("Only phones that enter this code can control the TV, on your home network"); /* (plain WebSocket on the LAN: the code is what protects it) */
  float tw = text_width(msg, 26, 600, 0), cw = text_width(shown, 40, 800, 8), w = 44 + tw + 30 + cw + 44, h = 84, x = SB_W / 2 - w / 2, y = 120 - 16 * (1 - a);
  gfx_opacity_push(a); lglass_light(0); lglass_shell(x, y, w, h, h / 2, RGBA(14, 12, 22, 200), 1);
  text_draw(msg, x + 44, y + h / 2 - text_line_height(26) / 2.0f, 26, 600, WHITE_A(220), 0, 0, ALIGN_LEFT);
  text_draw(shown, x + 44 + tw + 30, y + h / 2 - text_line_height(40) / 2.0f, 40, 800, ACC2 | 0xff000000u, 8, 0, ALIGN_LEFT);
  text_draw(sub, SB_W / 2, y + h + 14, 21, 500, WHITE_A(190), 0, 0, ALIGN_CENTER);
  gfx_opacity_pop();
}
