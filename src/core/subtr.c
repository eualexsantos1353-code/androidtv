#include "core/subtr.h"
#include "core/net.h"
#include "core/store.h"
#include "core/i18n.h"
#include "core/addons.h"
#include "core/json.h"
#include "core/app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <strings.h>

#define CHUNK 3200   /* characters of text per request */
#define INFLIGHT 2
static struct { Cue *c; int n; unsigned tok, gen, run; int (*still)(Cue *, int, unsigned);
  int start, next, wrapped, done, inflight; char tl[12], sl[12]; } S;

unsigned subtr_gen(void) { return S.gen; }
int subtr_active(void) { return S.c && !S.done; }
void subtr_stop(void) { S.c = NULL; S.n = 0; S.run++; S.inflight = 0; }

/* the app's language as Google names it, "" = none to translate into */
static void target(char *o, size_t n) { const char *l = i18n_lang(); if (!l || !*l) l = "en";
  if (!strncmp(l, "pt", 2)) snprintf(o, n, "pt"); else if (!strncmp(l, "zh", 2)) snprintf(o, n, "%s", strstr(l, "TW") || strstr(l, "Hant") ? "zh-TW" : "zh-CN");
  else { snprintf(o, n, "%.2s", l); } }
/* the subtitle's language in two letters, "" unknown (Google then guesses) */
static void source(const char *lang, char *o, size_t n) { o[0] = 0; if (!lang || !*lang) return;
  static const char *M[][2] = { { "eng", "en" }, { "spa", "es" }, { "fre", "fr" }, { "fra", "fr" }, { "ger", "de" }, { "deu", "de" }, { "ita", "it" }, { "jpn", "ja" }, { "kor", "ko" }, { "rus", "ru" }, { "chi", "zh-CN" }, { "zho", "zh-CN" }, { "ara", "ar" }, { "tur", "tr" }, { "pol", "pl" }, { "dut", "nl" }, { "nld", "nl" }, { "por", "pt" }, { "pob", "pt" } };
  for (size_t i = 0; i < sizeof M / sizeof *M; i++) if (!strncasecmp(lang, M[i][0], 3)) { snprintf(o, n, "%s", M[i][1]); return; }
  if (strlen(lang) >= 2 && (lang[2] == 0 || lang[2] == '-' || lang[2] == '_')) snprintf(o, n, "%.2s", lang); }

/* a cue's text as one line for the request: its own line breaks become spaces, tags are left out */
static void flat(const char *t, char *o, size_t n) { size_t k = 0; int tag = 0;
  for (const char *p = t; *p && k + 1 < n; p++) { if (*p == '<' || *p == '{') { tag = 1; continue; } if (tag) { if (*p == '>' || *p == '}') tag = 0; continue; }
    char c = *p == '\n' || *p == '\r' || *p == '\t' ? ' ' : *p; if (c == ' ' && (k == 0 || o[k - 1] == ' ')) continue; o[k++] = c; }
  while (k && o[k - 1] == ' ') k--; o[k] = 0; }
/* the translated line back in two lines when the original had two (split at the space nearest the middle) */
static char *rewrap(const char *orig, const char *tr) { int two = strchr(orig, '\n') != NULL; size_t L = strlen(tr); char *o = malloc(L + 1); if (!o) return NULL; memcpy(o, tr, L + 1);
  if (two && L > 24) { size_t mid = L / 2, best = 0; for (size_t d = 0; d < mid; d++) { if (o[mid + d] == ' ') { best = mid + d; break; } if (o[mid - d] == ' ') { best = mid - d; break; } } if (best) o[best] = '\n'; }
  return o; }

static void ask(void);
typedef struct { unsigned run; int a, b; } Req;
static void on_tr(int st, const unsigned char *body, size_t len, void *ud) {
  (void)len; Req *r = ud; int mine = r->run == S.run && S.c; if (mine) S.inflight--;
  if (!mine || !S.still || !S.still(S.c, S.n, S.tok)) { free(r); return; }
  cJSON *j = st == 200 ? net_json(body) : NULL; const cJSON *segs = cJSON_IsArray(j) ? cJSON_GetArrayItem(j, 0) : NULL, *sg;
  size_t cap = 16384, k = 0; char *all = malloc(cap); if (!all) { net_json_free(j); free(r); return; } all[0] = 0;
  cJSON_ArrayForEach(sg, segs) { const cJSON *t0 = cJSON_IsArray(sg) ? cJSON_GetArrayItem(sg, 0) : NULL; if (!cJSON_IsString(t0)) continue; size_t l = strlen(t0->valuestring);
    if (k + l + 1 > cap) { cap = (k + l + 1) * 2; char *nb = realloc(all, cap); if (!nb) break; all = nb; } memcpy(all + k, t0->valuestring, l + 1); k += l; }
  net_json_free(j);
  int want = r->b - r->a, got = 0; char *lines[200]; for (char *p = all, *q; p && got < 200; p = q) { q = strchr(p, '\n'); if (q) *q++ = 0; lines[got++] = p; }
  while (got > want && !*lines[got - 1]) got--; /* (a last empty line) */
  if (st == 200 && got == want) { for (int i = 0; i < want; i++) { Cue *c = &S.c[r->a + i]; char *s = rewrap(c->t, lines[i]); if (s && *s) { free(c->t); c->t = s; } else free(s); } S.gen++; }
  else fprintf(stderr, "[subtr] lines %d-%d: %s (%d of %d)\n", r->a, r->b, st == 200 ? "the lines did not match" : "no answer", got, want);
  free(all); free(r); ask();
}
/* the next block: from where the film was when it started to the end, then the beginning up to there */
static void ask(void) {
  while (S.c && S.inflight < INFLIGHT && !S.done) {
    if (!S.wrapped && S.next >= S.n) { S.wrapped = 1; S.next = 0; }
    if (S.wrapped && S.next >= S.start) { if (!S.inflight) { S.done = 1; fprintf(stderr, "[subtr] all %d lines translated\n", S.n); } return; }
    int end = S.wrapped ? S.start : S.n, a = S.next, b = a; size_t tot = 0; char *q = malloc(CHUNK * 3 + 64); if (!q) return; size_t k = 0;
    char *txt = malloc(CHUNK + 600); if (!txt) { free(q); return; } txt[0] = 0; size_t tk = 0;
    while (b < end && b - a < 120) { char one[600]; flat(S.c[b].t ? S.c[b].t : "", one, sizeof one); size_t l = strlen(one); if (tot + l + 1 > CHUNK && b > a) break; if (!l) snprintf(one, sizeof one, "..."), l = 3;
      memcpy(txt + tk, one, l); tk += l; txt[tk++] = '\n'; txt[tk] = 0; tot += l + 1; b++; }
    if (tk) txt[--tk] = 0;
    S.next = b; k = (size_t)snprintf(q, CHUNK * 3 + 64, "q="); addon_urlenc(txt, q + k, CHUNK * 3 + 64 - k); free(txt);
    char url[200]; snprintf(url, sizeof url, "https://translate.googleapis.com/translate_a/single?client=gtx&sl=%s&tl=%s&dt=t", S.sl[0] ? S.sl : "auto", S.tl);
    Req *r = malloc(sizeof *r); if (!r) { free(q); return; } r->run = S.run; r->a = a; r->b = b; S.inflight++;
    net_post_hdr_t(url, q, "application/x-www-form-urlencoded;charset=UTF-8", "User-Agent: Mozilla/5.0", 15000, on_tr, r); free(q);
  }
}
void subtr_start(Cue *cues, int n, const char *lang, int hasMine, double at, unsigned tok, int (*still)(Cue *, int, unsigned)) {
  subtr_stop(); if (!cues || n <= 0) return;
  const char *mode = store_get("subsTranslate", "auto"); if (!strcmp(mode, "false") || (strcmp(mode, "always") && hasMine)) return;
  char tl[12], sl[12]; target(tl, sizeof tl); source(lang, sl, sizeof sl);
  if (sl[0] && !strncmp(sl, tl, 2)) return; /* already in the app's language */
  if (!sl[0] && lang && *lang) return; /* a language we cannot name: left alone */
  S.c = cues; S.n = n; S.tok = tok; S.still = still; S.done = 0; S.wrapped = 0; S.inflight = 0;
  snprintf(S.tl, sizeof S.tl, "%s", tl); snprintf(S.sl, sizeof S.sl, "%s", sl);
  int s = 0; while (s < n && cues[s].e < at - 5) s++; if (s >= n) s = 0; S.start = S.next = s;
  fprintf(stderr, "[subtr] translating %d lines %s -> %s from line %d\n", n, sl[0] ? sl : "auto", tl, s);
  ask();
}
