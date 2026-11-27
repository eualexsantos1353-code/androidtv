#include "screens/duo.h"
#include "core/app.h"
#include "core/keys.h"
#include "core/i18n.h"
#include "core/store.h"
#include "core/json.h"
#include "core/httpd.h"
#include "core/releases.h"
#include "core/later.h"
#include "core/organize.h"
#include "ui/gfx.h"
#include "ui/text.h"
#include "ui/img.h"
#include "ui/qr.h"
#include "ui/orbs.h"
#include "ui/sound.h"
#include "ui/widgets.h"
#include "screens/detail.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAXD 30
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
/* shared with the server thread (under mu) */
static struct { int n, sess; char id[MAXD][24], type[MAXD][12], name[MAXD][160], poster[MAXD][512]; signed char vote[2][MAXD]; int done[2], joined[2]; char who[2][32]; } SH;
/* the TV's own */
static struct { int open, closing, nm, sel, zone, seen[MAXD], matchShown; Item it[MAXD]; int match[MAXD]; char url[600]; double bannerAt; int banner; Tween a, b; Orbs orbs; } D;
static Screen scr;

static void jstr(char **p, size_t *n, size_t *cap, const char *s) { for (; *s; s++) { char e[8]; const char *w = e; if (*s == '"' || *s == '\\') { e[0] = '\\'; e[1] = *s; e[2] = 0; } else if ((unsigned char)*s < 0x20) { e[0] = ' '; e[1] = 0; } else { e[0] = *s; e[1] = 0; }
    size_t l = strlen(w); if (*n + l + 1 > *cap) { *cap = (*n + l + 1) * 2; *p = realloc(*p, *cap); } memcpy(*p + *n, w, l + 1); *n += l; } }
static void cat_(char **p, size_t *n, size_t *cap, const char *s) { size_t l = strlen(s); if (*n + l + 1 > *cap) { *cap = (*n + l + 1) * 2; *p = realloc(*p, *cap); } memcpy(*p + *n, s, l + 1); *n += l; }
static const char *PAGE_HTML =
"<!doctype html><html lang=pt-BR><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'><title>Lumen \xc2\xb7 Sess\xc3\xa3o a dois</title>"
"<style>body{margin:0;font-family:system-ui,sans-serif;background:#0c0b11;color:#fff;text-align:center}h1{font-size:22px;margin:18px}"
".b{display:inline-block;border:0;border-radius:40px;padding:16px 26px;margin:8px;font-size:18px;font-weight:700;color:#fff;background:#7c3aed}"
"#card img{width:72vw;max-width:340px;border-radius:18px;box-shadow:0 10px 40px #000}#nm{font-size:20px;font-weight:800;margin:12px}"
".v{width:84px;height:84px;border-radius:50%;font-size:38px;border:0;margin:10px 22px}#no{background:#33303f}#yes{background:#e11d74}#st{opacity:.7;font-size:14px}</style></head><body>"
"<h1>\xf0\x9f\x8e\xac Sess\xc3\xa3o a dois</h1><div id=pick><p>Quem \xc3\xa9 voc\xc3\xaa?</p><button class=b onclick=go(0) id=w0>Pessoa 1</button><button class=b onclick=go(1) id=w1>Pessoa 2</button></div>"
"<div id=card style=display:none><img id=im><div id=nm></div><button class=v id=no onclick=vote(0)>\xe2\x9c\x96</button><button class=v id=yes onclick=vote(1)>\xf0\x9f\x92\x9c</button><p id=st></p></div>"
"<div id=end style=display:none><h1>Pronto!</h1><p>Olhe a TV: os matches est\xc3\xa3o l\xc3\xa1 \xf0\x9f\x8d\xbf</p></div>"
"<script>var B=location.pathname,W=-1,L=[],I=0;fetch(B+'/state').then(r=>r.json()).then(s=>{L=s.items;if(s.who){document.getElementById('w0').textContent=s.who[0];document.getElementById('w1').textContent=s.who[1];}});"
"function go(w){W=w;fetch(B+'/vote?who='+w+'&i=-1&v=0');document.getElementById('pick').style.display='none';document.getElementById('card').style.display='';show()}"
"function show(){if(I>=L.length){document.getElementById('card').style.display='none';document.getElementById('end').style.display='';return}var t=L[I];document.getElementById('im').src=t.p;document.getElementById('nm').textContent=t.n;document.getElementById('st').textContent=(I+1)+' de '+L.length}"
"function vote(v){fetch(B+'/vote?who='+W+'&i='+I+'&v='+v);I++;show()}</script></body></html>";
/* the server thread */
static char *serve(const char *path, const char *q, const char **ct) {
  if (!strcmp(path, "/duo")) { *ct = "text/html"; return strdup(PAGE_HTML); }
  if (!strcmp(path, "/duo/state")) { size_t n = 0, cap = 4096; char *p = malloc(cap); p[0] = 0; pthread_mutex_lock(&mu);
    cat_(&p, &n, &cap, "{\"who\":[\""); jstr(&p, &n, &cap, SH.who[0]); cat_(&p, &n, &cap, "\",\""); jstr(&p, &n, &cap, SH.who[1]); cat_(&p, &n, &cap, "\"],\"items\":[");
    for (int i = 0; i < SH.n; i++) { cat_(&p, &n, &cap, i ? ",{\"n\":\"" : "{\"n\":\""); jstr(&p, &n, &cap, SH.name[i]); cat_(&p, &n, &cap, "\",\"p\":\""); jstr(&p, &n, &cap, SH.poster[i]); cat_(&p, &n, &cap, "\"}"); }
    cat_(&p, &n, &cap, "]}"); pthread_mutex_unlock(&mu); return p; }
  if (!strcmp(path, "/duo/vote")) { char w[8], i[8], v[8]; httpd_qparam(q, "who", w, sizeof w); httpd_qparam(q, "i", i, sizeof i); httpd_qparam(q, "v", v, sizeof v); int wi = atoi(w), ii = atoi(i), vi = atoi(v);
    pthread_mutex_lock(&mu); if (wi == 0 || wi == 1) { SH.joined[wi] = 1; if (ii >= 0 && ii < SH.n) { SH.vote[wi][ii] = vi ? 1 : -1; if (ii + 1 > SH.done[wi]) SH.done[wi] = ii + 1; } } pthread_mutex_unlock(&mu); return NULL; }
  return NULL; }

static void close_(void) { if (!D.open || D.closing) return; D.closing = 1; tween_to(&D.a, 0, 0.35f, EASE_IN_OUT); if (app_overlay == &scr) app_overlay = NULL; }
static int key(int k) {
  if (!D.open || D.closing) return 0;
  if (k == K_BACK) { close_(); return 1; }
  if (k == K_LEFT && D.sel > 0) { D.sel--; sound_play(SND_FOCUS); } else if (k == K_RIGHT && D.sel < D.nm - 1) { D.sel++; sound_play(SND_FOCUS); }
  else if (k == K_OK && D.nm) { DetailParams p; memset(&p, 0, sizeof p); p.it = D.it[D.match[D.sel]]; close_(); app_go(SCR_DETAIL, &p, 0); }
  return 1; }
static void tick(float dt) {
  tween_tick(&D.a, dt); tween_tick(&D.b, dt); orbs_step(&D.orbs, dt, 1, 1, 1); if (D.closing && D.a.v < 0.01f) { D.open = 0; D.closing = 0; } if (!D.open) return;
  httpd_start(); /* (kept on while the session is) */
  pthread_mutex_lock(&mu); for (int i = 0; i < SH.n; i++) if (!D.seen[i] && SH.vote[0][i] == 1 && SH.vote[1][i] == 1) { D.seen[i] = 1; D.match[D.nm++] = i; D.banner = i; D.bannerAt = app_now(); tween_set(&D.b, 0); tween_to(&D.b, 1, 0.5f, EASE_OUT); sound_play(SND_START); fprintf(stderr, "[duo] match: %s\n", SH.id[i]); } pthread_mutex_unlock(&mu);
  if (D.bannerAt && app_now() - D.bannerAt > 3.5) { D.bannerAt = 0; tween_to(&D.b, 0, 0.4f, EASE_IN_OUT); } }
static void poster_at(int i, float x, float y, float w, float h, int foc) { float asp = 0; GLuint t = img_get(D.it[i].poster, (int)w, (int)h, &asp); if (t) gfx_image(t, x, y, w, h, 14, WHITE_A(255), 0, 0, 1, 1); else gfx_rrect(x, y, w, h, 14, WHITE_A(30)); if (foc) gfx_ring(x - 6, y - 6, w + 12, h + 12, 18, 4, WHITE_A(240)); }
static void draw(void) {
  if (!D.open) return; float a = D.a.v; if (a <= 0.001f) return; gfx_opacity_push(a);
  gfx_rect(0, 0, SB_W, SB_H, PAGE); orbs_draw(&D.orbs, 1);
  text_draw(T("Duo session"), 100, 80, 64, 800, WHITE_A(255), -1, 0, ALIGN_LEFT);
  text_draw(T("Scan with both phones, each one votes without seeing the other"), 100, 170, 28, 500, WHITE_A(190), 0, 0, ALIGN_LEFT);
  if (!D.url[0] || !qr_draw(D.url, 100, 250, 300, 20, 12)) gfx_rrect(100, 250, 300, 300, 20, WHITE_A(30));
  int dn[2], jn[2], n; pthread_mutex_lock(&mu); for (int w = 0; w < 2; w++) { dn[w] = SH.done[w]; jn[w] = SH.joined[w]; } n = SH.n; pthread_mutex_unlock(&mu);
  for (int w = 0; w < 2; w++) { char l[120], c[16], t2[16]; snprintf(c, sizeof c, "%d", dn[w]); snprintf(t2, sizeof t2, "%d", n); snprintf(l, sizeof l, "%s: %s", SH.who[w], jn[w] ? T2("{a} of {b}", "a", c, "b", t2) : T("waiting")); text_draw(l, 460, 290 + w * 60, 32, 700, jn[w] ? WHITE_A(240) : WHITE_A(140), 0, 0, ALIGN_LEFT); }
  { char m[64], c[16]; snprintf(c, sizeof c, "%d", D.nm); snprintf(m, sizeof m, "%s", T1("Matches: {n}", "n", c)); text_draw(m, 100, 620, 40, 800, ACC2 | 0xff000000u, 0, 0, ALIGN_LEFT); }
  for (int k = 0; k < D.nm && k < 9; k++) poster_at(D.match[k], 100 + k * 190, 690, 160, 240, k == D.sel);
  if (!D.nm) text_draw(T("When you both want the same title, it shows up here"), 100, 720, 28, 500, WHITE_A(150), 0, 0, ALIGN_LEFT);
  if (D.b.v > 0.01f) { float b = D.b.v; gfx_opacity_push(b); gfx_rect(0, 0, SB_W, SB_H, BLACK_A(180)); text_draw(T("It's a match!"), SB_W / 2, 150, 90, 800, RGB(0xf4, 0x3f, 0x5e), -2, 0, ALIGN_CENTER);
    float s = 0.8f + 0.2f * b; gfx_scale_push(s, s, SB_W / 2, 560); poster_at(D.banner, SB_W / 2 - 180, 300, 360, 540, 0); gfx_scale_pop(); text_draw(D.it[D.banner].name, SB_W / 2, 880, 44, 800, WHITE_A(255), -0.5f, SB_W - 200, ALIGN_CENTER); gfx_opacity_pop(); }
  if (D.nm) key_hint(100, SB_H - 60, K_OK, T("Watch"), WHITE_A(220), 0); key_hint(D.nm ? 300 : 100, SB_H - 60, K_BACK, T("End"), WHITE_A(180), 0);
  gfx_opacity_pop(); }
static Screen scr = { NULL, NULL, key, tick, draw, NULL, NULL, NULL, NULL, NULL };
static void add(const Item *it) { if (SH.n >= MAXD || strncmp(it->id, "tt", 2) || !it->poster[0]) return; for (int i = 0; i < SH.n; i++) if (!strcmp(SH.id[i], it->id)) return;
  int i = SH.n++; snprintf(SH.id[i], sizeof SH.id[i], "%s", it->id); snprintf(SH.type[i], sizeof SH.type[i], "%s", it->type); snprintf(SH.name[i], sizeof SH.name[i], "%s", it->name); sb_poster_of(it, SH.poster[i], sizeof SH.poster[i]); D.it[i] = *it; }
void duo_open(void) {
  static int hooked; if (!hooked) { hooked = 1; httpd_set_duo_handler(serve); }
  pthread_mutex_lock(&mu); memset(&SH, 0, sizeof SH);
  snprintf(SH.who[0], sizeof SH.who[0], "%s", prof_count() > 0 ? prof_name(0) : T("Person 1")); snprintf(SH.who[1], sizeof SH.who[1], "%s", prof_count() > 1 && !prof_kid(1) ? prof_name(1) : T("Person 2"));
  memset(&D.it, 0, sizeof D.it); { ItemList l; if (later_items(&l)) { for (int i = 0; i < l.n; i++) add(&l.items[i]); itemlist_clear(&l); } if (releases_items(&l)) { for (int i = 0; i < l.n; i++) add(&l.items[i]); itemlist_clear(&l); } }
  { const cJSON *e; cJSON_ArrayForEach(e, lib_all()) { if (SH.n >= MAXD) break; if (strcmp(js_str(e, "type", ""), "movie") || js_bool(e, "removed", 0) || js_bool(e, "temp", 0) || js_num(js_obj(e, "state"), "timesWatched", 0) > 0 || js_num(js_obj(e, "state"), "timeWatched", 0) > 0) continue;
      Item it; memset(&it, 0, sizeof it); snprintf(it.id, sizeof it.id, "%s", js_str(e, "_id", "")); snprintf(it.type, sizeof it.type, "movie"); snprintf(it.name, sizeof it.name, "%s", js_str(e, "name", "")); snprintf(it.poster, sizeof it.poster, "%s", js_str(e, "poster", "")); add(&it); } }
  for (int i = SH.n - 1; i > 0; i--) { int j = rand() % (i + 1); /* (shuffled: both see the same order) */
    char t[512]; Item ti; snprintf(t, sizeof t, "%s", SH.id[i]); snprintf(SH.id[i], 24, "%s", SH.id[j]); snprintf(SH.id[j], 24, "%s", t); snprintf(t, sizeof t, "%s", SH.type[i]); snprintf(SH.type[i], 12, "%s", SH.type[j]); snprintf(SH.type[j], 12, "%s", t);
    snprintf(t, sizeof t, "%s", SH.name[i]); snprintf(SH.name[i], 160, "%s", SH.name[j]); snprintf(SH.name[j], 160, "%s", t); snprintf(t, sizeof t, "%s", SH.poster[i]); snprintf(SH.poster[i], 512, "%s", SH.poster[j]); snprintf(SH.poster[j], 512, "%s", t); ti = D.it[i]; D.it[i] = D.it[j]; D.it[j] = ti; }
  SH.sess++; pthread_mutex_unlock(&mu);
  fprintf(stderr, "[duo] a session with %d titles\n", SH.n);
  D.url[0] = 0; httpd_url("/duo", D.url, sizeof D.url); D.nm = 0; D.sel = 0; D.bannerAt = 0; memset(D.seen, 0, sizeof D.seen); tween_set(&D.b, 0);
  D.open = 1; D.closing = 0; tween_set(&D.a, 0); tween_to(&D.a, 1, 0.4f, EASE_OUT); orbs_place(&D.orbs, 1, 1.4f); app_overlay_keep(&scr); app_overlay = &scr; sound_play(SND_TAB); }
