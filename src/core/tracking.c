#include "core/tracking.h"
#include "core/simkl.h"
#include "core/stremio.h"
#include "core/addons.h"
#include "core/store.h"
#include "core/net.h"
#include "core/json.h"
#include <stdio.h>
#include <string.h>

static int on(void) { return strcmp(store_get("trackingAddons", "true"), "false") != 0; }
static void on_answer(int st, const unsigned char *b, size_t n, void *ud) { (void)b; (void)n; (void)ud; if (st != 200) fprintf(stderr, "[tracking] an addon answered %d\n", st); }
static int send_all(const char *resource, const char *type, const char *id, const char *extra) {
  if (!on() || !id || !*id) return 0; cJSON *a = sb_addons(); const cJSON *ad; int k = 0; if (!a) return 0;
  cJSON_ArrayForEach(ad, a) { if (!sb_addon_on(ad) || !addon_serves(ad, resource, type, id)) continue;
    char base[512], url[1400]; addon_base(ad, base, sizeof base); addon_url(url, sizeof url, base, resource, type, id, extra);
    net_get(url, on_answer, NULL); k++; fprintf(stderr, "[tracking] %s: %s %s %s\n", addon_name(ad), resource, id, extra); }
  return k; }
int tracking_count(void) { cJSON *a = sb_addons(); const cJSON *ad; int k = 0; if (a) cJSON_ArrayForEach(ad, a) if (sb_addon_on(ad) && (addon_serves(ad, "player", "movie", NULL) || addon_serves(ad, "player", "series", NULL) || addon_serves(ad, "library", "movie", NULL) || addon_serves(ad, "library", "series", NULL))) k++; return k; }

static struct { char type[16], vid[200]; int sent; double lastPos, steadyAt; } S; /* sent: the last action told (0 none, 1 start, 2 pause) */
static void tell(const char *action, double pos, double dur) {
  char ex[160] = "", v[32]; addon_extra_add(ex, sizeof ex, "action", action);
  snprintf(v, sizeof v, "%.0f", pos * 1000); addon_extra_add(ex, sizeof ex, "currentTime", v);
  snprintf(v, sizeof v, "%.0f", dur * 1000); addon_extra_add(ex, sizeof ex, "duration", v);
  send_all("player", S.type, S.vid, ex); }
void tracking_player(const char *type, const char *videoId, int state, double pos, double dur) {
  simkl_player(type, videoId, state, pos, dur); /* (Simkl too, when linked) */
  if (!type || !videoId || !*videoId || dur <= 0) return;
  if (strcmp(S.vid, videoId)) { if (S.sent) tell("stop", S.lastPos, dur); memset(&S, 0, sizeof S); snprintf(S.type, sizeof S.type, "%s", type); snprintf(S.vid, sizeof S.vid, "%s", videoId); S.lastPos = pos; }
  int want = state == 1 ? 1 : state == 2 ? 2 : S.sent;
  double jump = pos - S.lastPos; int seeked = S.sent && (jump > 15 || jump < -5); S.lastPos = pos;
  if (want != S.sent || seeked) { tell(want == 1 ? "start" : "pause", pos, dur); S.sent = want; } }
void tracking_player_end(double pos, double dur) { simkl_player_end(pos, dur); if (S.sent && S.vid[0]) tell("stop", pos, dur > 0 ? dur : pos); memset(&S, 0, sizeof S); }
void tracking_library(const char *type, const char *id, const char *action, const char *videoIds) {
  char ex[1200] = ""; addon_extra_add(ex, sizeof ex, "action", action); if (videoIds && *videoIds) addon_extra_add(ex, sizeof ex, "videoId", videoIds);
  send_all("library", type && *type ? type : "movie", id, ex); }
