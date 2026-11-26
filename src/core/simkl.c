#include "core/simkl.h"
#include "core/net.h"
#include "core/store.h"
#include "core/httpd.h"
#include "core/json.h"
#include "core/app.h"
#include "core/i18n.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#define API "https://api.simkl.com"
static char client[100], token[100], path[1200], code[16]; static int inited, st; static double pollAt, pollEnd, every = 5; static unsigned tok;
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER; static char phoneId[100]; static unsigned phoneGen, seenGen;
static void save(void) { FILE *f = fopen(path, "wb"); if (f) { fprintf(f, "%s\n%s", client, token); fclose(f); } }
static int id_ok(const char *k) { size_t n = strlen(k); if (n < 16 || n > 96) return 0; for (const char *p = k; *p; p++) if (!isalnum((unsigned char)*p)) return 0; return 1; }
static int from_phone(const char *raw) { /* the server thread: the Client ID pasted on the phone */
  char k[100]; size_t o = 0; for (; *raw && o + 1 < sizeof k; raw++) if (!isspace((unsigned char)*raw)) k[o++] = *raw; k[o] = 0; if (!id_ok(k)) return 0;
  pthread_mutex_lock(&mu); snprintf(phoneId, sizeof phoneId, "%s", k); phoneGen++; pthread_mutex_unlock(&mu); return 1; }
void simkl_init(void) {
  if (inited) return; inited = 1; snprintf(path, sizeof path, "%s/simkl.auth", store_data_dir());
  FILE *f = fopen(path, "rb"); if (f) { char t[256]; size_t n = fread(t, 1, sizeof t - 1, f); t[n] = 0; fclose(f); char *nl = strchr(t, '\n'); if (nl) { *nl = 0; snprintf(token, sizeof token, "%s", nl + 1); } snprintf(client, sizeof client, "%s", t); }
  st = client[0] && token[0] ? 2 : 0; httpd_set_simkl_handler(from_phone); }
int simkl_state(void) { simkl_init(); return st; }
const char *simkl_code(void) { return st == 1 ? code : ""; }
void simkl_unlink(void) { simkl_init(); token[0] = 0; code[0] = 0; st = 0; remove(path); }
static void hdr(char *h, size_t cap) { snprintf(h, cap, "simkl-api-key: %s\nAuthorization: Bearer %s", client, token); }
static void on_poll(int status, const unsigned char *b, size_t len, void *ud) {
  (void)len; if ((unsigned)(uintptr_t)ud != tok || st != 1) return; cJSON *j = status == 200 ? net_json(b) : NULL; const char *at = js_str(j, "access_token", "");
  if (*at) { snprintf(token, sizeof token, "%.*s", (int)sizeof token - 1, at); st = 2; code[0] = 0; save(); toast(T("Simkl is connected"), 2500, NULL); fprintf(stderr, "[simkl] linked\n"); }
  net_json_free(j); }
static void on_pin(int status, const unsigned char *b, size_t len, void *ud) {
  (void)len; if ((unsigned)(uintptr_t)ud != tok) return; cJSON *j = status == 200 ? net_json(b) : NULL; const char *uc = js_str(j, "user_code", "");
  if (*uc) { snprintf(code, sizeof code, "%s", uc); st = 1; every = js_num(j, "interval", 5); if (every < 3) every = 3; pollAt = app_now() + every; pollEnd = app_now() + js_num(j, "expires_in", 900);
    char m[160]; snprintf(m, sizeof m, "%s", T1("Simkl: enter {code} at simkl.com/pin", "code", code)); toast(m, 30000, NULL); }
  else { st = 0; toast(T("Simkl didn't accept this Client ID"), 3000, NULL); }
  net_json_free(j); }
void simkl_tick(void) {
  simkl_init();
  pthread_mutex_lock(&mu); int g = phoneGen != seenGen; if (g) { seenGen = phoneGen; snprintf(client, sizeof client, "%s", phoneId); } pthread_mutex_unlock(&mu);
  if (g) { token[0] = 0; save(); tok++; char u[300]; snprintf(u, sizeof u, API "/oauth/pin?client_id=%s&redirect=urn:ietf:wg:oauth:2.0:oob", client); net_get(u, on_pin, (void *)(uintptr_t)tok); }
  if (st == 1 && app_now() > pollAt) { if (app_now() > pollEnd) { st = 0; code[0] = 0; return; } pollAt = app_now() + every; char u[300]; snprintf(u, sizeof u, API "/oauth/pin/%s?client_id=%s", code, client); net_get(u, on_poll, (void *)(uintptr_t)tok); }
}
/* ---- what plays ---- */
static struct { char type[16], vid[200]; int sent; double last; } S;
static void on_scr(int status, const unsigned char *b, size_t len, void *ud) { (void)b; (void)len; (void)ud; if (status >= 300 || status <= 0) fprintf(stderr, "[simkl] scrobble answered %d\n", status); }
static void tell(const char *action, double pos, double dur) {
  if (st != 2 || strncmp(S.vid, "tt", 2)) return; cJSON *o = cJSON_CreateObject(); double pct = dur > 0 ? pos * 100.0 / dur : 0; if (pct > 100) pct = 100; cJSON_AddNumberToObject(o, "progress", (int)(pct * 100) / 100.0);
  char im[24]; snprintf(im, sizeof im, "%s", S.vid); char *c = strchr(im, ':'); if (c) *c = 0;
  int se = 0, ep = 0; { const char *l = strrchr(S.vid, ':'); if (l && !strcmp(S.type, "series")) { const char *p = l - 1; while (p > S.vid && *p != ':') p--; if (*p == ':') { se = atoi(p + 1); ep = atoi(l + 1); } } }
  if (se > 0) { cJSON *sh = cJSON_AddObjectToObject(o, "show"); cJSON_AddStringToObject(cJSON_AddObjectToObject(sh, "ids"), "imdb", im); cJSON *e = cJSON_AddObjectToObject(o, "episode"); cJSON_AddNumberToObject(e, "season", se); cJSON_AddNumberToObject(e, "number", ep); }
  else { cJSON *mv = cJSON_AddObjectToObject(o, "movie"); cJSON_AddStringToObject(cJSON_AddObjectToObject(mv, "ids"), "imdb", im); }
  char *body = cJSON_PrintUnformatted(o); cJSON_Delete(o); if (!body) return; char h[300], u[120]; hdr(h, sizeof h); snprintf(u, sizeof u, API "/scrobble/%s", action);
  net_post_hdr_t(u, body, "application/json", h, 12000, on_scr, NULL); free(body); fprintf(stderr, "[simkl] %s %s\n", action, S.vid); }
void simkl_player(const char *type, const char *videoId, int state, double pos, double dur) {
  if (st != 2 || !type || !videoId || !*videoId || dur <= 0) return;
  if (strcmp(S.vid, videoId)) { if (S.sent) tell("stop", S.last, dur); memset(&S, 0, sizeof S); snprintf(S.type, sizeof S.type, "%s", type); snprintf(S.vid, sizeof S.vid, "%s", videoId); S.last = pos; }
  int want = state == 1 ? 1 : state == 2 ? 2 : S.sent; double jump = pos - S.last; int seek = S.sent && (jump > 30 || jump < -10); S.last = pos;
  if (want != S.sent || seek) { tell(want == 1 ? "start" : "pause", pos, dur); S.sent = want; } }
void simkl_player_end(double pos, double dur) { if (S.sent && S.vid[0]) tell("stop", pos, dur > 0 ? dur : pos); memset(&S, 0, sizeof S); }
