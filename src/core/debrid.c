#include "core/debrid.h"
#include "core/net.h"
#include "core/store.h"
#include "core/httpd.h"
#include "core/json.h"
#include "core/stremio.h"
#include "core/app.h"
#include "core/i18n.h"
#include "screens/player.h"
#include "ui/widgets.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdint.h>

enum { SV_NONE, SV_TB, SV_RD, SV_PM };
static const char *SVN[] = { "", "TorBox", "Real-Debrid", "Premiumize" }, *SVK[] = { "", "torbox", "realdebrid", "premiumize" };
static int svc; static char key[128], keyPath[1200]; static int inited; static unsigned gen;
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER; static int phoneSvc; static char phoneKey[128]; static unsigned phoneGen, seenGen;
static DebFile *F; static int nF, capF, state; /* state: 0 not asked, 1 asking, 2 known */ static int pend; static unsigned tok;

static void strip_ws(const char *in, char *out, size_t cap) { size_t o = 0; for (; *in && o + 1 < cap; in++) if (!isspace((unsigned char)*in)) out[o++] = *in; out[o] = 0; }
static int key_ok(const char *k) { size_t n = strlen(k); if (n < 8 || n > 120) return 0; for (const char *p = k; *p; p++) if (!isalnum((unsigned char)*p) && *p != '-' && *p != '_') return 0; return 1; }
static int svc_of(const char *s) { for (int i = 1; i <= 3; i++) if (!strcasecmp(s, SVK[i])) return i; return 0; }
static void auth(char *h, size_t cap, int sv, const char *k) { if (sv == SV_PM) h[0] = 0; else snprintf(h, cap, "Authorization: Bearer %s", k); }

/* the phone's Save (server thread): the key tried on its service first */
static int key_from_phone(const char *s, const char *raw) {
  int sv = svc_of(s); char k[128]; strip_ws(raw, k, sizeof k); if (!sv || !key_ok(k)) return 0;
  char url[300], h[200]; auth(h, sizeof h, sv, k);
  if (sv == SV_TB) snprintf(url, sizeof url, "https://api.torbox.app/v1/api/user/me");
  else if (sv == SV_RD) snprintf(url, sizeof url, "https://api.real-debrid.com/rest/1.0/user");
  else snprintf(url, sizeof url, "https://www.premiumize.me/api/account/info?apikey=%s", k);
  size_t len = 0; int st = 0; unsigned char *b = net_fetch_sync_hdr(url, h[0] ? h : NULL, &len, &st, 15000);
  int ok = st == 200 && (sv != SV_PM || (b && strstr((const char *)b, "\"success\""))); free(b);
  if (!ok) return 0;
  char data[200]; snprintf(data, sizeof data, "%s\n%s", SVK[sv], k); FILE *f = fopen(keyPath, "wb"); if (!f) return 0; fputs(data, f); fclose(f);
  pthread_mutex_lock(&mu); phoneSvc = sv; snprintf(phoneKey, sizeof phoneKey, "%s", k); phoneGen++; pthread_mutex_unlock(&mu);
  return 1;
}
void deb_init(void) {
  if (inited) return; inited = 1; snprintf(keyPath, sizeof keyPath, "%s/debrid.key", store_data_dir());
  FILE *f = fopen(keyPath, "rb"); if (f) { char t[300]; size_t n = fread(t, 1, sizeof t - 1, f); t[n] = 0; fclose(f); char *nl = strchr(t, '\n');
    if (nl) { *nl = 0; svc = svc_of(t); strip_ws(nl + 1, key, sizeof key); if (!svc || !key_ok(key)) { svc = 0; key[0] = 0; } } }
  httpd_set_deb_handler(key_from_phone);
}
static void drain(void) {
  if (!inited) deb_init();
  pthread_mutex_lock(&mu); int g = phoneGen != seenGen; if (g) { seenGen = phoneGen; svc = phoneSvc; snprintf(key, sizeof key, "%s", phoneKey); } pthread_mutex_unlock(&mu);
  if (g) { state = 0; nF = 0; gen++; }
}
int deb_has_key(void) { drain(); return svc && key[0]; }
const char *deb_service_name(void) { drain(); return SVN[svc]; }
void deb_key_clear(void) { drain(); remove(keyPath); svc = 0; key[0] = 0; state = 0; nF = 0; gen++; }
unsigned deb_generation(void) { drain(); return gen; }
void deb_refresh(void) { if (state == 2) state = 0; }

static int is_video(const char *name, const char *mime) {
  if (mime && !strncmp(mime, "video/", 6)) return 1; const char *d = strrchr(name, '.'); if (!d) return 0;
  static const char *E[] = { ".mkv", ".mp4", ".avi", ".m4v", ".mov", ".ts", ".webm", ".wmv", ".m2ts", ".mpg" };
  for (unsigned i = 0; i < sizeof E / sizeof *E; i++) if (!strcasecmp(d, E[i])) return 1; return 0; }
static void add(const char *name, double size, const char *a, const char *b, const char *link) {
  if (nF == capF) { capF = capF ? capF * 2 : 64; F = realloc(F, (size_t)capF * sizeof *F); }
  DebFile *f = &F[nF++]; memset(f, 0, sizeof *f); const char *sl = strrchr(name, '/'); snprintf(f->name, sizeof f->name, "%s", sl ? sl + 1 : name);
  f->size = size; snprintf(f->a, sizeof f->a, "%s", a ? a : ""); snprintf(f->b, sizeof f->b, "%s", b ? b : ""); snprintf(f->link, sizeof f->link, "%s", link ? link : ""); }
static void done_one(void) { if (--pend <= 0) { state = 2; gen++; fprintf(stderr, "[debrid] %s: %d video files\n", SVN[svc], nF); } }
static void num(const cJSON *o, const char *k, char *out, size_t cap) { const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, k); if (cJSON_IsNumber(v)) snprintf(out, cap, "%.0f", v->valuedouble); else snprintf(out, cap, "%s", js_str(o, k, "")); }
static void on_tb(int st, const unsigned char *body, size_t len, void *ud) { /* TorBox: torrents (a = "t<id>") and usenet ("u<id>"), each one's files */
  (void)len; unsigned t = (unsigned)(uintptr_t)ud >> 1; int us = (int)((uintptr_t)ud & 1); if (t != tok) return; cJSON *j = st == 200 ? net_json(body) : NULL; const cJSON *it;
  cJSON_ArrayForEach(it, js_arr(j, "data")) { char id[32], a[48]; num(it, "id", id, sizeof id); snprintf(a, sizeof a, "%c%s", us ? 'u' : 't', id); const cJSON *fl;
    cJSON_ArrayForEach(fl, js_arr(it, "files")) { const char *nm = js_str(fl, "short_name", js_str(fl, "name", "")); if (!is_video(nm, js_str(fl, "mimetype", NULL))) continue; char fid[32]; num(fl, "id", fid, sizeof fid); add(nm, js_num(fl, "size", 0), a, fid, NULL); } }
  net_json_free(j); done_one(); }
static void on_rd(int st, const unsigned char *body, size_t len, void *ud) { /* Real-Debrid: its downloads, each with its link already */
  (void)len; if ((unsigned)(uintptr_t)ud != tok) return; cJSON *j = st == 200 ? net_json(body) : NULL; const cJSON *it;
  cJSON_ArrayForEach(it, j) { const char *nm = js_str(it, "filename", ""); if (!is_video(nm, js_str(it, "mimeType", NULL))) continue; add(nm, js_num(it, "filesize", 0), NULL, NULL, js_str(it, "download", "")); }
  net_json_free(j); done_one(); }
static void on_pm(int st, const unsigned char *body, size_t len, void *ud) { /* Premiumize: every file in the cloud */
  (void)len; if ((unsigned)(uintptr_t)ud != tok) return; cJSON *j = st == 200 ? net_json(body) : NULL; const cJSON *it;
  cJSON_ArrayForEach(it, js_arr(j, "files")) { const char *nm = js_str(it, "name", ""); if (!is_video(nm, js_str(it, "mime_type", NULL))) continue; add(nm, js_num(it, "size", 0), js_str(it, "id", ""), NULL, NULL); }
  net_json_free(j); done_one(); }
int deb_list(const DebFile **out) {
  drain(); *out = F; if (!svc) return 0; if (state == 2) return nF; if (state == 1) return -1;
  state = 1; nF = 0; tok++; char h[200], u[300]; auth(h, sizeof h, svc, key);
  if (svc == SV_TB) { pend = 2; net_get_hdr("https://api.torbox.app/v1/api/torrents/mylist?bypass_cache=true", h, on_tb, (void *)(uintptr_t)(tok << 1));
    net_get_hdr("https://api.torbox.app/v1/api/usenet/mylist?bypass_cache=true", h, on_tb, (void *)(uintptr_t)((tok << 1) | 1)); }
  else if (svc == SV_RD) { pend = 1; net_get_hdr("https://api.real-debrid.com/rest/1.0/downloads?limit=200", h, on_rd, (void *)(uintptr_t)tok); }
  else { pend = 1; snprintf(u, sizeof u, "https://www.premiumize.me/api/item/listall?apikey=%s", key); net_get(u, on_pm, (void *)(uintptr_t)tok); }
  return -1;
}

/* playing one: its link, then the player with nothing to keep in the Library (a file has no title page) */
static void play_link(const char *name, const char *url) {
  if (!url || !*url) { toast(T("Could not play"), 2400, NULL); return; }
  Stream *s = calloc(1, sizeof *s); if (!s) return; snprintf(s->url, sizeof s->url, "%s", url); snprintf(s->name, sizeof s->name, "%s", SVN[svc]); snprintf(s->title, sizeof s->title, "%s", name); snprintf(s->filename, sizeof s->filename, "%s", name);
  player_play(s, "other", "", NULL, 1); free(s); }
static char playName[256];
static void on_link(int st, const unsigned char *body, size_t len, void *ud) {
  (void)len; (void)ud; cJSON *j = st == 200 ? net_json(body) : NULL; const char *u = js_str(j, "data", ""); if (!*u) u = js_str(j, "link", "");
  play_link(playName, u); net_json_free(j); }
void deb_play(int i) {
  drain(); if (i < 0 || i >= nF) return; DebFile *f = &F[i]; snprintf(playName, sizeof playName, "%s", f->name);
  if (f->link[0]) { play_link(f->name, f->link); return; }
  char u[600];
  if (svc == SV_TB) snprintf(u, sizeof u, "https://api.torbox.app/v1/api/%s/requestdl?token=%s&%s=%s&file_id=%s", f->a[0] == 'u' ? "usenet" : "torrents", key, f->a[0] == 'u' ? "usenet_id" : "torrent_id", f->a + 1, f->b);
  else snprintf(u, sizeof u, "https://www.premiumize.me/api/item/details?id=%s&apikey=%s", f->a, key);
  toast(T("Starting playback…"), 1500, NULL); net_get(u, on_link, NULL);
}
