#include "core/organize.h"
#include "core/store.h"
#include "core/i18n.h"
#include "core/best.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int prof_cur(void) { int p = atoi(store_get("profCur", "0")); return p >= 0 && p < PROF_N ? p : 0; }
const char *prof_name(int i) { static const char *N[PROF_N] = { "Main", "Family", "Kids" }; return T(N[i >= 0 && i < PROF_N ? i : 0]); }

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
