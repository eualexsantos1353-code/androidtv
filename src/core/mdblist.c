#include "core/mdblist.h"
#include "core/net.h"
#include "core/store.h"
#include "core/httpd.h"
#include "cJSON.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MDB_API "https://api.mdblist.com"
#define NCACHE 96
static char key[80], keyPath[1200]; static int inited; static unsigned gen;
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static char phoneKey[80]; static unsigned phoneKeyGen, seenKeyGen; /* a key saved from the phone (server thread) */
typedef struct { char id[24]; char line[120]; int state; } Ent; /* state 0 empty, 1 asked, 2 known */
static Ent cache[NCACHE]; static int next;
static int key_ok(const char *k) { size_t n = strlen(k); if (n < 10 || n > 72) return 0; for (const char *p = k; *p; p++) if (!isalnum((unsigned char)*p) && *p != '-' && *p != '_') return 0; return 1; }
static void strip_ws(const char *in, char *out, size_t cap) { size_t o = 0; for (; *in && o + 1 < cap; in++) if (!isspace((unsigned char)*in)) out[o++] = *in; out[o] = 0; }
static int write_file(const char *p, const char *data, size_t len) {
  char tmp[1300]; snprintf(tmp, sizeof tmp, "%s.tmp", p); FILE *f = fopen(tmp, "wb"); if (!f) return 0;
  size_t w = fwrite(data, 1, len, f); fclose(f); if (w != len) { remove(tmp); return 0; } return rename(tmp, p) == 0;
}
/* the phone page's Save (server thread): the key is tried on MDBList before it is kept */
static int key_from_phone(const char *raw) {
  char k[80]; strip_ws(raw, k, sizeof k); if (!key_ok(k)) return 0;
  char url[200]; snprintf(url, sizeof url, MDB_API "/user?apikey=%s", k); size_t len = 0; int st = 0; unsigned char *b = net_fetch_sync(url, &len, &st, 12000); free(b);
  if (st != 200) return 0;
  if (!write_file(keyPath, k, strlen(k))) return 0;
  pthread_mutex_lock(&mu); snprintf(phoneKey, sizeof phoneKey, "%s", k); phoneKeyGen++; pthread_mutex_unlock(&mu);
  return 1;
}
void mdb_init(void) {
  if (inited) return; inited = 1;
  snprintf(keyPath, sizeof keyPath, "%s/mdblist.key", store_data_dir());
  FILE *f = fopen(keyPath, "rb"); if (f) { char t[200]; size_t n = fread(t, 1, sizeof t - 1, f); t[n] = 0; fclose(f); strip_ws(t, key, sizeof key); if (!key_ok(key)) key[0] = 0; }
  httpd_set_mdb_handler(key_from_phone);
}
static void drain(void) { /* a key that came from the phone since the last look */
  if (!inited) mdb_init();
  pthread_mutex_lock(&mu); int kg = phoneKeyGen != seenKeyGen; if (kg) { seenKeyGen = phoneKeyGen; snprintf(key, sizeof key, "%s", phoneKey); } pthread_mutex_unlock(&mu);
  if (kg) { memset(cache, 0, sizeof cache); gen++; }
}
int mdb_has_key(void) { drain(); return key[0] != 0; }
void mdb_key_clear(void) { drain(); remove(keyPath); key[0] = 0; memset(cache, 0, sizeof cache); gen++; }
unsigned mdb_generation(void) { drain(); return gen; }

/* one answer: the ratings list, as a line */
static void on_answer(int st, const unsigned char *body, size_t len, void *ud) {
  (void)len; char id[24]; snprintf(id, sizeof id, "%s", (const char *)ud); free(ud);
  Ent *e = NULL; for (int i = 0; i < NCACHE; i++) if (cache[i].state == 1 && !strcmp(cache[i].id, id)) { e = &cache[i]; break; } if (!e) return; /* (the key changed meanwhile: the cache was cleared) */
  e->line[0] = 0;
  cJSON *j = st == 200 ? net_json(body) : NULL;
  if (j) {
    const cJSON *rs = cJSON_GetObjectItemCaseSensitive(j, "ratings"); double rt = -1, mc = -1, tr = -1, lb = -1, au = -1; const cJSON *r;
    cJSON_ArrayForEach(r, rs) { const cJSON *s = cJSON_GetObjectItemCaseSensitive(r, "source"), *v = cJSON_GetObjectItemCaseSensitive(r, "value"), *sc = cJSON_GetObjectItemCaseSensitive(r, "score");
      if (!cJSON_IsString(s) || !cJSON_IsNumber(v) || v->valuedouble < 0) continue; const char *n = s->valuestring; double x = v->valuedouble;
      if (!strcmp(n, "tomatoes")) rt = x; else if (!strcmp(n, "metacritic")) mc = x; else if (!strcmp(n, "trakt")) tr = x;
      else if (!strcmp(n, "letterboxd")) lb = cJSON_IsNumber(sc) ? sc->valuedouble / 20.0 : x;
      else if (!strcmp(n, "popcorn") || !strcmp(n, "audience") || !strcmp(n, "tomatoesaudience")) au = x; }
    char *o = e->line; size_t cap = sizeof e->line; o[0] = 0;
    #define ADD(...) do { size_t l = strlen(o); snprintf(o + l, cap - l, __VA_ARGS__); } while (0)
    if (rt >= 0 && rt <= 100) ADD("RT %d%%", (int)(rt + 0.5));
    if (au >= 0 && au <= 100) ADD("%sAudience %d%%", o[0] ? "  \xc2\xb7  " : "", (int)(au + 0.5));
    if (mc >= 0 && mc <= 100) ADD("%sMC %d", o[0] ? "  \xc2\xb7  " : "", (int)(mc + 0.5));
    if (tr >= 0 && tr <= 100) ADD("%sTrakt %d%%", o[0] ? "  \xc2\xb7  " : "", (int)(tr + 0.5));
    if (lb >= 0 && lb <= 5) ADD("%sLetterboxd %.1f", o[0] ? "  \xc2\xb7  " : "", lb);
    #undef ADD
    net_json_free(j); }
  e->state = 2; gen++;
}
const char *mdb_line(const char *type, const char *id) {
  drain(); if (!key[0] || !id || strncmp(id, "tt", 2)) return "";
  char base[24]; size_t n = 0; while (id[n] && id[n] != ':' && n + 1 < sizeof base) { base[n] = id[n]; n++; } base[n] = 0;
  for (int i = 0; i < NCACHE; i++) if (cache[i].state && !strcmp(cache[i].id, base)) return cache[i].state == 2 ? cache[i].line : "";
  Ent *e = &cache[next++ % NCACHE]; memset(e, 0, sizeof *e); snprintf(e->id, sizeof e->id, "%s", base); e->state = 1;
  char url[300]; snprintf(url, sizeof url, MDB_API "/imdb/%s/%s?apikey=%s", type && !strcmp(type, "series") ? "show" : "movie", base, key);
  char *ud = strdup(base); if (!ud || !net_get(url, on_answer, ud)) { free(ud); e->state = 2; }
  return "";
}
