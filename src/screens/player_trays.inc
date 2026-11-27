/* etapa-89: part of player.c (included there, one translation unit): the trays: sources, episodes, audio, subtitles */
/* ---- trays ---- */
static int focusable(const TItem *it) { return it && it->k != 0 && it->k != 1; }
static int first_on(const TItem *l, int n) { for (int i = 0; i < n; i++) if (l[i].on && focusable(&l[i])) return i; for (int i = 0; i < n; i++) if (focusable(&l[i])) return i; return -1; }
static int season_of(const char *vid) { const char *c = strrchr(vid, ':'); if (!c) return 1; const char *p = c - 1; while (p > vid && *p != ':') p--; return *p == ':' ? atoi(p + 1) : 1; }
static TItem *add_item(TItem *list, int *n, int max, int k) { if (*n >= max) return NULL; TItem *it = &list[(*n)++]; memset(it, 0, sizeof *it); it->k = k; return it; }
static TItem *col_add(TCol *c, int k) { c->items = realloc(c->items, (c->n + 1) * sizeof *c->items); TItem *it = &c->items[c->n++]; memset(it, 0, sizeof *it); it->k = k; return it; }
static void cols_clear(void) { for (int i = 0; i < 4; i++) { free(P.tray.cols[i].items); P.tray.cols[i].items = NULL; P.tray.cols[i].n = 0; } P.tray.ncols = 0; }
/* ---- the sources tray: tabs (All, then each addon serving the video, in the user's order, with how many it has),
   the stream playing and the best fit for this TV pinned on top of All, then every addon's streams under its name.
   A stream is one calm row: its facts in the accent (resolution, HDR, sound, languages), its size and source with the
   release's name, and the one thing that matters on this TV in coral when there is one (dimmed when it won't play
   here at all). Every addon is asked as the tray opens; everything is let go when it closes ---- */
typedef struct { int k; /* 0 a note, 1 waiting, 3 a stream, 5 an addon's name, 6 the one playing, 7 the best fit */ int g, i; float y, h; } SRow;
static struct { SRow *r; int n, cap, idx, moved; Mo y, plY, plH, plA; float tot; } SR; /* moved: the viewer has moved in this list (until then the focus stays on its top: what plays, the best fit) */
static unsigned srcAsk;
/* the trays' small motions, all tweens on quads already drawn: the tab pill gliding (and lighting up when focused),
   the contents swapping tabs (out and in, sliding the way of the move), the column's backdrop following the focus,
   a list fading in when what it lists changes, the tick fading in where the choice went, a value fading up when it
   changes, the lit dot of a value that cycles gliding to its place */
static const float C_GLIDE[4] = { .25f, 1, .5f, 1 }; /* GLIDE_EASE's curve, for the Mo motions */
static struct {
  Mo tabX, tabW, tabF; int tabSeen; Mo swA; int swDir, swPend; Mo colBX, colBA; int colBSeen; int quiet;
  unsigned colSig[4], rowSig; Mo colA[4], rowA; int onIdx[4], rowOn; Mo chkA[4], rowChk; int vkC; char vkU[8]; Mo vkA;
  Mo dot[5]; int dotSeen[5]; /* 0 the picture size, 1 subtitle size, 2 colour, 3 background, 4 subtitle position */
  Mo swK[2]; int swFrom[2], swCur[2], swSeen[2]; /* the colour and background swatches: the one before, the one now, how far it has come */
} TM;
static void tm_tick(float dt) {
  Mo *m[] = { &TM.tabX, &TM.tabW, &TM.tabF, &TM.swA, &TM.colBX, &TM.colBA, &TM.rowA, &TM.rowChk, &TM.vkA, &TM.dot[0], &TM.dot[1], &TM.dot[2], &TM.dot[3], &TM.dot[4], &TM.swK[0], &TM.swK[1] };
  for (size_t i = 0; i < sizeof m / sizeof *m; i++) mo_tick(m[i], dt); for (int i = 0; i < 4; i++) { mo_tick(&TM.colA[i], dt); mo_tick(&TM.chkA[i], dt); }
}
static void tm_reset(void) { /* a tray opening: everything where it is, nothing gliding in from an old place */
  TM.tabSeen = TM.colBSeen = TM.swPend = 0; mo_set(&TM.swA, 1); mo_set(&TM.colBA, 0); mo_set(&TM.tabF, 0); mo_set(&TM.vkA, 1); TM.vkU[0] = 0;
  for (int i = 0; i < 4; i++) { mo_set(&TM.colA[i], 1); mo_set(&TM.chkA[i], 1); TM.onIdx[i] = -2; TM.colSig[i] = 0; } mo_set(&TM.rowA, 1); mo_set(&TM.rowChk, 1); TM.rowOn = -2; TM.rowSig = 0;
  for (int i = 1; i < 5; i++) TM.dotSeen[i] = 0; TM.swSeen[0] = TM.swSeen[1] = 0;
}
static unsigned str_sig(unsigned h, const char *s) { while (*s) h = h * 31 + (unsigned char)*s++; return h * 31 + 7; }
static int item_on(const TItem *it) { return strncmp(it->u, "lang:", 5) ? it->on && !it->tool : it->d > 0; }
/* after each rebuild: a list whose items changed fades in, a tick that moved fades in at its new place */
static void tm_after(void) {
  for (int c = 0; c < 4; c++) { unsigned h = 1; int on = -1; const TCol *col = &P.tray.cols[c]; if (c < P.tray.ncols) for (int i = 0; i < col->n; i++) { h = str_sig(h * 13 + (unsigned)col->items[i].k, col->items[i].lb); if (!col->items[i].tool) h = str_sig(h, col->items[i].sub); /* (a tool's line under it is its state, not the list) */ if (on < 0 && item_on(&col->items[i])) on = i; }
    if (!TM.quiet && TM.colSig[c] && h != TM.colSig[c]) { mo_set(&TM.colA[c], 0); mo_to(&TM.colA[c], 1, .22f, C_OUT, 0); }
    if (!TM.quiet && TM.onIdx[c] != -2 && on != TM.onIdx[c] && on >= 0) { mo_set(&TM.chkA[c], 0); mo_to(&TM.chkA[c], 1, .2f, C_EASE, 0); }
    TM.colSig[c] = h; TM.onIdx[c] = on; }
  unsigned h = 1; int on = -1; for (int i = 0; i < P.tray.n; i++) { h = str_sig(h * 13 + (unsigned)P.tray.row[i].k, P.tray.row[i].lb); if (on < 0 && P.tray.row[i].on) on = i; }
  if (!TM.quiet && TM.rowSig && h != TM.rowSig) { mo_set(&TM.rowA, 0); mo_to(&TM.rowA, 1, .22f, C_OUT, 0); }
  if (!TM.quiet && TM.rowOn != -2 && on != TM.rowOn && on >= 0) { mo_set(&TM.rowChk, 0); mo_to(&TM.rowChk, 1, .2f, C_EASE, 0); }
  TM.rowSig = h; TM.rowOn = on;
}
/* a value that cycles (the picture size, the subtitles' style): one dot per choice, the lit one gliding to the current */
static void cycle_dots(int which, int n, int cur, float x, float y, float gap, float r, uint32_t dim, uint32_t lit, int alignRight) {
  /* the current choice is a short pill, the others dots; as the choice moves the pill flows from one into the next (each
     dot's length follows how near the gliding choice is to it), the row staying centred on x (or ending at x) */
  if (!TM.dotSeen[which]) { mo_set(&TM.dot[which], (float)cur); TM.dotSeen[which] = 1; } else mo_to(&TM.dot[which], (float)cur, GLIDE_DUR * .9f, C_GLIDE, 0);
  float d = 2 * r, L = 3.4f * d, sp = gap - d, w[8], tot = 0; if (n > 8) n = 8;
  for (int i = 0; i < n; i++) { float k = 1 - fabsf(i - TM.dot[which].v); k = k < 0 ? 0 : k; w[i] = d + (L - d) * k; tot += w[i] + (i ? sp : 0); }
  float xx = alignRight ? x - tot : x - tot / 2;
  for (int i = 0; i < n; i++) { float k = 1 - fabsf(i - TM.dot[which].v); k = k < 0 ? 0 : k;
    gfx_rrect(xx, y - r, w[i], d, r, k > 0.01f ? col_mix(dim, lit, k) : dim); xx += w[i] + sp; }
}
/* the subtitles' colour and background as a round swatch: the colour itself; no background a small checkerboard, the
   transparent one that checkerboard darkened, solid plain black. A new choice comes in over the old one, growing a
   little into place as the old one fades (one short ease, nothing that repeats) */
static GLuint checkTex;
static void swatch_one(int which, int v, float cx, float cy, float r, float a) {
  if (a <= 0.004f) return; uint32_t A = (uint32_t)(a * 255 + .5f) << 24;
  if (which == 0) gfx_circle(cx, cy, r, (hexcol(SUB_COLORS[v]) & 0xffffff) | A);
  else if (v == 1) gfx_circle(cx, cy, r, A);
  else { if (!checkTex) { unsigned char px[4 * 4 * 4]; for (int i = 0; i < 16; i++) { unsigned char g = ((i & 3) + (i >> 2)) & 1 ? 150 : 236; px[i * 4] = px[i * 4 + 1] = px[i * 4 + 2] = g; px[i * 4 + 3] = 255; } checkTex = gfx_tex_create(px, 4, 4, 4, 0); }
    uint32_t tint = v == 2 ? RGB(102, 102, 102) : RGB(255, 255, 255); /* (.6 black over it) */
    gfx_image(checkTex, cx - r, cy - r, 2 * r, 2 * r, r, (tint & 0xffffff) | A, 0, 0, 1, 1); }
}
static void swatch_draw(int which, int v, float cx, float cy, float r) {
  if (!TM.swSeen[which]) { TM.swSeen[which] = 1; TM.swCur[which] = TM.swFrom[which] = v; mo_set(&TM.swK[which], 1); }
  else if (v != TM.swCur[which]) { TM.swFrom[which] = TM.swCur[which]; TM.swCur[which] = v; mo_set(&TM.swK[which], 0); mo_to(&TM.swK[which], 1, .22f, C_OUT, 0); }
  float k = TM.swK[which].v;
  if (k < 1) swatch_one(which, TM.swFrom[which], cx, cy, r, 1 - k);
  swatch_one(which, v, cx, cy, r * (.84f + .16f * k), k < 1 ? fminf(1, k * 1.6f) : 1);
  gfx_ring(cx - r, cy - r, 2 * r, 2 * r, r, 1.5f, WHITE_A(64)); /* its edge, so black shows on the dark tray */
}
#define SRC_W 1000.0f        /* the tray */
#define COL_LH 520.0f        /* the subtitle columns' window (about seven entries) */
#define TAB_FS 28            /* the trays' tabs: their words, and the pill around them */
#define TAB_PAD 60.0f
#define TAB_GAP 6.0f
#define SRC_RW (SRC_W - 62)  /* a row */
#define SRC_LH 630.0f        /* the list's window at most (the tray stays clear of the clock) */
#define SRC_VH SRC_LH /* the list window: always the same height, so the tray keeps one size on every tab */
static int src_tray(void) { int ak = P.tray.kind < 0 ? -P.tray.kind : P.tray.kind; return ak == 2 || ak == 5; }
static void srcs_free(void) { for (int i = 0; i < P.srcs.ng; i++) { free(P.srcs.g[i].items); free(P.srcs.g[i].f); } memset(&P.srcs, 0, sizeof P.srcs); P.srcs.bg = P.srcs.bi = -1; free(SR.r); SR.r = NULL; SR.n = SR.cap = 0; SR.idx = -1; }
static void src_rows(void);
static void src_tabs(void);
static void close_tray(void);
static int src_find(unsigned tok) { if (!tok) return -1; for (int i = 0; i < P.srcs.ng; i++) if (P.srcs.g[i].tok == tok) return i; return -1; }
static int src_on(const Stream *s) { char info[700]; sb_stream_info(s, info, sizeof info); return !strcmp(s->url, P.url) || !strcmp(s->url, P.st.url) || !strcmp(s->url, P.alt) || (P.sname[0] && !strncmp(info, P.sname, 200) && !strcmp(s->addon, P.saddon)); }
/* the best fit for this TV among everything that came (the same pick as Play's) */
static void src_best(void) { const Stream *all[600]; int gi[600], ii[600], n = 0;
  for (int g = 0; g < P.srcs.ng; g++) for (int i = 0; i < P.srcs.g[g].n && n < 600; i++) if (P.srcs.g[g].items[i].url[0]) { all[n] = &P.srcs.g[g].items[i]; gi[n] = g; ii[n] = i; n++; }
  P.srcs.bg = P.srcs.bi = -1; if (!n) return; BestRate br; const Stream *b = best_pick(all, n, best_dur(P.meta, P.type), NULL, -1, &br);
  for (int k = 0; k < n; k++) if (all[k] == b) { P.srcs.bg = gi[k]; P.srcs.bi = ii[k]; break; } }
static void on_src_part(StreamGroup *g, int gi, void *ud) { (void)gi; int k = P.open ? src_find((unsigned)(uintptr_t)ud) : -1; if (k < 0) { free(g->items); return; } /* a list for a tray since closed: dropped */
  free(P.srcs.g[k].items); free(P.srcs.g[k].f); P.srcs.g[k].items = g->items; P.srcs.g[k].n = g->n; P.srcs.g[k].nf = 0; P.srcs.g[k].f = calloc(g->n ? g->n : 1, sizeof(BestFacts)); /* the list is ours now; its streams are read a few a frame (src_tick) */
  if (!P.srcs.g[k].f) { free(g->items); P.srcs.g[k].items = NULL; P.srcs.g[k].n = 0; } }
/* an addon whose streams have all been read: shown (its rows fade in where the waiting ones were), the best fit again */
static void src_shown(int k) { int any = 0; for (int i = 0; i < P.srcs.g[k].n; i++) if (P.srcs.g[k].items[i].url[0]) any = 1; P.srcs.g[k].st = any ? 2 : 3;
  mo_set(&P.srcs.g[k].ia, 0); mo_to(&P.srcs.g[k].ia, 1, .24f, C_OUT, 0); src_best(); if (src_tray()) { src_tabs(); src_rows(); } }
static void on_src_done(int total, void *ud) { (void)total; int k = P.open ? src_find((unsigned)(uintptr_t)ud) : -1; if (k < 0) return; P.srcs.g[k].tok = 0; P.srcs.g[k].came = 1;
  if (P.srcs.g[k].nf >= P.srcs.g[k].n) src_shown(k); }
/* each stream read once (best_facts), 12 a frame at most: a big list never holds a frame on the TV */
static void src_tick(float dt) { int budget = 12, dm = -1;
  for (int k = 0; k < P.srcs.ng; k++) { mo_tick(&P.srcs.g[k].ia, dt); __typeof__(P.srcs.g[0]) *g = &P.srcs.g[k];
    while (budget > 0 && g->f && g->nf < g->n) { if (dm < 0) dm = best_dur(P.meta, P.type); best_facts(&g->items[g->nf], NULL, -1, dm, &g->f[g->nf]); g->nf++; budget--; }
    if (g->came && g->st == 1 && g->nf >= g->n) src_shown(k); } }
static void src_ask(int k) { if (k < 0 || k >= P.srcs.ng || P.srcs.g[k].st) return; P.srcs.g[k].st = 1; if (!++srcAsk) ++srcAsk; P.srcs.g[k].tok = srcAsk;
  sb_streams_one(P.type, P.srcs.vid, k, on_src_part, on_src_done, (void *)(uintptr_t)srcAsk); } /* (an addon that is gone answers at once: empty) */
static void srcs_begin(const char *vid) {
  if (P.srcs.on && !strcmp(P.srcs.vid, vid)) return; srcs_free(); snprintf(P.srcs.vid, sizeof P.srcs.vid, "%s", vid); P.srcs.on = 1;
  char names[SRC_G][96]; int n = sb_stream_addons(P.type, vid, names, SRC_G);
  for (int i = 0; i < n; i++) { snprintf(P.srcs.g[i].name, sizeof P.srcs.g[i].name, "%s", names[i][0] ? names[i] : T("Addon")); mo_set(&P.srcs.g[i].ia, 1); } P.srcs.ng = n;
  for (int i = 0; i < n; i++) src_ask(i); /* every addon at once: All and the counts fill as they answer */
}
/* the tabs: All, then each addon with how many it has (a dot while it is asked) */
static int src_count(int g) { int n = 0; for (int i = 0; i < P.srcs.g[g].n; i++) if (P.srcs.g[g].items[i].url[0]) n++; return n; }
static int srcTabG[40]; /* tab -> addon (tab 0 = All): an addon with nothing to show has no tab */
static char srcLast[96]; static int srcRestored; /* the addon tab last used (kept while the app runs); restored once per opening */
/* how much each addon is used (a play of one of its streams): "count<TAB>name" lines in the store, so the favourite goes first */
#define SU_MAX 24
static void su_clean(char *d, const char *s) { snprintf(d, 96, "%s", s); for (char *q = d; *q; q++) if (*q == '\t' || *q == '\n') *q = ' '; }
static int su_load(char (*nm)[96], int *ct) { int n = 0; const char *p = store_get("srcUse", "");
  while (*p && n < SU_MAX) { int c = 0; if (sscanf(p, "%d", &c) != 1) break; const char *t = strchr(p, '\t'); if (!t) break; t++; const char *e = strchr(t, '\n'); size_t l = e ? (size_t)(e - t) : strlen(t); if (l > 95) l = 95;
    memcpy(nm[n], t, l); nm[n][l] = 0; ct[n++] = c; p = e ? e + 1 : t + strlen(t); }
  return n; }
static int src_use_of(const char *name) { char nm[SU_MAX][96], c[96]; int ct[SU_MAX], n = su_load(nm, ct); su_clean(c, name); for (int i = 0; i < n; i++) if (!strcmp(nm[i], c)) return ct[i]; return 0; }
static void src_use_add(const char *name) { if (!name || !*name) return; char nm[SU_MAX + 1][96], c[96]; int ct[SU_MAX + 1], n = su_load(nm, ct), f = -1; su_clean(c, name);
  for (int i = 0; i < n; i++) if (!strcmp(nm[i], c)) f = i;
  if (f < 0) { if (n >= SU_MAX) { int lo = 0; for (int i = 1; i < n; i++) if (ct[i] < ct[lo]) lo = i; f = lo; } else f = n++; snprintf(nm[f], 96, "%s", c); ct[f] = 0; }
  ct[f]++; char buf[SU_MAX * 110 + 8]; size_t o = 0; buf[0] = 0;
  for (int i = 0; i < n && o + 110 < sizeof buf; i++) o += (size_t)snprintf(buf + o, sizeof buf - o, "%d\t%s\n", ct[i], nm[i]);
  store_set("srcUse", buf); }
static int has_ci(const char *h, const char *n) { size_t ln = strlen(n); for (; *h; h++) if (!strncasecmp(h, n, ln)) return 1; return 0; }
static int pt_tok(const char *f) { for (const char *p = f; *p; p++) if ((p[0] == 'P' || p[0] == 'p') && (p[1] == 'T' || p[1] == 't') && (p == f || !isalnum((unsigned char)p[-1])) && !isalnum((unsigned char)p[2])) return 1; return 0; }
/* an addon with a stream that is both 4K and dubbed (Portuguese audio): its tab goes first */
static int src_dub4k(int g) { __typeof__(P.srcs.g[0]) *x = &P.srcs.g[g]; if (!x->f) return 0;
  for (int i = 0; i < x->n && i < x->nf; i++) { const Stream *st = &x->items[i]; if (!st->url[0]) continue; char info[700]; sb_stream_info(st, info, sizeof info);
    int k4 = has_ci(x->f[i].facts, "4K") || has_ci(info, "2160p") || has_ci(info, "4k");
    int dub = pt_tok(x->f[i].facts) || has_ci(info, "dublado") || has_ci(info, "dubbed") || has_ci(info, "pt-br") || has_ci(info, "dual");
    if (k4 && dub) return 1; }
  return 0; }
static void src_tabs(void) { int tot = 0, cur = P.tray.tab > 0 ? srcTabG[P.tray.tab] : -1, ord[16], no = 0, key[16][3]; P.tray.ntabs = 0;
  for (int g = 0; g < P.srcs.ng; g++) tot += src_count(g);
  for (int g = 0; g < P.srcs.ng && g < 16; g++) { if (P.srcs.g[g].st == 3) continue; /* answered with nothing: no tab */
    int a = P.srcs.g[g].st >= 2; key[g][0] = a ? src_dub4k(g) : -1; key[g][1] = a || P.srcs.g[g].st == 1 ? src_use_of(P.srcs.g[g].name) : 0; key[g][2] = a ? src_count(g) : -1;
    int j = no++; /* 4K dubbed first, else the addon you use most, then the most streams; the ones still asked last (stable) */
    while (j > 0 && (key[ord[j - 1]][0] < key[g][0] || (key[ord[j - 1]][0] == key[g][0] && (key[ord[j - 1]][1] < key[g][1] || (key[ord[j - 1]][1] == key[g][1] && key[ord[j - 1]][2] < key[g][2]))))) { ord[j] = ord[j - 1]; j--; } ord[j] = g; }
  { char n[12]; snprintf(n, sizeof n, "%d", tot); srcTabG[P.tray.ntabs] = -1; snprintf(P.tray.tabs[P.tray.ntabs++], 48, "%s  %s", T("All"), tot ? n : "\xc2\xb7"); }
  for (int o = 0; o < no && P.tray.ntabs < 40; o++) { int g = ord[o]; char n[12];
    if (P.srcs.g[g].st >= 2) snprintf(n, sizeof n, "%d", src_count(g)); else snprintf(n, sizeof n, "\xc2\xb7");
    srcTabG[P.tray.ntabs] = g; snprintf(P.tray.tabs[P.tray.ntabs++], 48, "%.36s  %s", P.srcs.g[g].name, n); }
  if (!srcRestored && srcLast[0]) for (int i = 1; i < P.tray.ntabs; i++) if (!strcmp(P.srcs.g[srcTabG[i]].name, srcLast)) { cur = srcTabG[i]; srcRestored = 1; break; } /* the last addon you used, once it has answered */
  { int t = 0; if (cur >= 0) for (int i = 1; i < P.tray.ntabs; i++) if (srcTabG[i] == cur) { t = i; break; } P.tray.tab = t; } }
/* the rows' heights: a stream two lines (three with its note), a pinned one the same, an addon's name a short line */
#define SN_PX 27      /* a stream's facts: 27 px on 34 */
#define SN_LH 34.0f
#define SD_PX 23      /* its size, source and release: 23 px on 31 */
#define SD_LH 31.0f
#define SRC_RH (16 + SN_LH + 4 + SD_LH + 16)
static SRow *sr_add(int k, int g, int i) { if (SR.n >= SR.cap) { int c = SR.cap ? SR.cap * 2 : 64; SRow *r = realloc(SR.r, c * sizeof *r); if (!r) return NULL; SR.r = r; SR.cap = c; } SRow *x = &SR.r[SR.n++]; memset(x, 0, sizeof *x); x->k = k; x->g = g; x->i = i; return x; }
static int src_focusable(int i) { return i >= 0 && i < SR.n && (SR.r[i].k == 3 || SR.r[i].k == 6 || SR.r[i].k == 7); }
static int src_url_on(const Stream *s) { return s->url[0] && (!strcmp(s->url, P.url) || !strcmp(s->url, P.st.url) || !strcmp(s->url, P.alt)); }
/* the one stream playing (an addon may list the same release twice: one only): its address first, its name when no address matches */
static int src_playing(int *g, int *i) { if (P.tray.kind != 2 && P.tray.kind != -2) return 0;
  for (int pass = 0; pass < 2; pass++) for (int a = 0; a < P.srcs.ng; a++) for (int b = 0; b < P.srcs.g[a].n; b++) { const Stream *s = &P.srcs.g[a].items[b];
    if (s->url[0] && (pass ? src_on(s) : src_url_on(s))) { *g = a; *i = b; return 1; } }
  return 0; }
static int srcPG = -1, srcPI = -1; /* (the one playing, as the rows were laid out: its tag goes on it alone) */
/* an addon's streams in its own order, the ones that won't play on this TV after the rest */
static void src_add_group(int g) { int all = P.tray.tab == 0; /* (on All, what is pinned on top is not listed again) */
  for (int pass = 0; pass < 2; pass++) for (int i = 0; i < P.srcs.g[g].n; i++) if (P.srcs.g[g].items[i].url[0] && !!P.srcs.g[g].f[i].out == pass) {
    if (all && ((g == srcPG && i == srcPI) || (g == P.srcs.bg && i == P.srcs.bi))) continue; sr_add(3, g, i); } }
static void src_rows(void) {
  int fk = -1, fg = -1, fi = -1; if (SR.moved && src_focusable(SR.idx)) { fk = SR.r[SR.idx].k; fg = SR.r[SR.idx].g; fi = SR.r[SR.idx].i; }
  if (P.tray.tab > 0 && P.tray.tab < P.tray.ntabs) { snprintf(srcLast, sizeof srcLast, "%s", P.srcs.g[srcTabG[P.tray.tab]].name); srcRestored = 1; } else if (srcRestored) srcLast[0] = 0; /* remember the tab you chose (All too) */
  SR.n = 0; int tab = P.tray.tab, pg = -1, pi = -1, playing = src_playing(&pg, &pi); srcPG = playing ? pg : -1; srcPI = playing ? pi : -1;
  if (tab == 0) { /* All: what plays now, the best fit, then every addon under its name */
    if (playing) sr_add(6, pg, pi);
    if (P.srcs.bg >= 0 && !(playing && pg == P.srcs.bg && pi == P.srcs.bi)) sr_add(7, P.srcs.bg, P.srcs.bi);
    for (int g = 0; g < P.srcs.ng; g++) { int st = P.srcs.g[g].st; if (st == 3) continue; sr_add(5, g, -1);
      if (st <= 1) { sr_add(1, g, 0); sr_add(1, g, 1); } else src_add_group(g); }
    int any = 0; for (int i = 0; i < SR.n; i++) if (SR.r[i].k != 0) any = 1; if (!any) sr_add(0, -1, 0); }
  else { int g = tab < P.tray.ntabs ? srcTabG[tab] : -1; if (g >= 0 && g < P.srcs.ng) { int st = P.srcs.g[g].st;
      if (st <= 1) { sr_add(1, g, 0); sr_add(1, g, 1); sr_add(1, g, 2); } else if (st == 3) sr_add(0, g, 0); else src_add_group(g); } }
  float y = 0;
  for (int i = 0; i < SR.n; i++) { SRow *r = &SR.r[i];
    if (r->k == 5) { y += i ? 18 : 4; r->h = 40; }
    else if (r->k == 3 || r->k == 6 || r->k == 7) r->h = SRC_RH + (P.srcs.g[r->g].f[r->i].warn[0] ? 4 + SD_LH : 0);
    else r->h = r->k == 1 ? SRC_RH : 64;
    r->y = y; y += r->h + (r->k == 5 ? 6 : 10); }
  SR.tot = y;
  SR.idx = -1; if (fk >= 0) for (int i = 0; i < SR.n; i++) if (SR.r[i].k == fk && SR.r[i].g == fg && SR.r[i].i == fi) { SR.idx = i; break; }
  if (SR.idx < 0 && fk >= 0) for (int i = 0; i < SR.n; i++) if (src_focusable(i) && SR.r[i].g == fg && SR.r[i].i == fi) { SR.idx = i; break; } /* the same stream, in its other place */
  if (SR.idx < 0) for (int i = 0; i < SR.n; i++) if (src_focusable(i)) { SR.idx = i; break; } /* fresh: the one playing, else the best fit, else the first */
}
static void src_choose(int i) {
  if (!src_focusable(i)) return; SRow r = SR.r[i];
  if (r.k == 6) { close_tray(); return; } /* (the one playing: nothing to change) */
  int same = P.tray.kind == 2; char v[SB_VID]; snprintf(v, sizeof v, "%s", same ? P.vid : P.tray.epvid); Stream s = P.srcs.g[r.g].items[r.i]; play_stream(&s, v, same ? video_time() : 0);
}
/* an episode not aired yet (its date after today): nothing to give away, and no still to blur */
static int ep_unaired(const cJSON *e) { const char *rel = js_str(e, "released", js_str(e, "firstAired", "")); int y, m, d; if (sscanf(rel, "%d-%d-%d", &y, &m, &d) != 3) return 0; time_t now = time(NULL); struct tm *t = localtime(&now); int today = (t->tm_year + 1900) * 10000 + (t->tm_mon + 1) * 100 + t->tm_mday; return y * 10000 + m * 100 + d > today; }
/* ---- the sound's extras in the Audio tray: night mode, volume boost, audio delay, speed (NuvioTV's) ---- */
static const int BOOSTS[] = { 0, 25, 50, 100 }; static const float SPEEDS[] = { 1, 1.25f, 1.5f, 2, 0.5f, 0.75f };
static int night_on(void) { return !strcmp(store_get("audNight", "off"), "on"); }
static int boost_idx(void) { int v = atoi(store_get("audBoost", "0")), k = 0; for (int i = 0; i < 4; i++) if (BOOSTS[i] == v) k = i; return k; }
static void aud_fx_apply(void) { video_audio_fx(night_on(), BOOSTS[boost_idx()]); }
static void aud_fx_chips(void) {
  TItem *x;
  x = add_item(P.tray.row, &P.tray.n, MAXROW, 4); snprintf(x->lb, 200, "%s", T("Night mode")); snprintf(x->sub, 120, "%s", night_on() ? T("On") : T("Off")); snprintf(x->u, 1024, "fx:night"); x->i = -101; x->on = night_on();
  x = add_item(P.tray.row, &P.tray.n, MAXROW, 4); snprintf(x->lb, 200, "%s", T("Volume boost")); { int b = BOOSTS[boost_idx()]; if (b) snprintf(x->sub, 120, "+%d%%", b); else snprintf(x->sub, 120, "%s", T("Off")); } snprintf(x->u, 1024, "fx:boost"); x->i = -102; x->on = boost_idx() > 0;
  x = add_item(P.tray.row, &P.tray.n, MAXROW, 4); snprintf(x->lb, 200, "%s", T("Audio delay")); if (P.audDelay == 0) snprintf(x->sub, 120, "0.00 s"); else snprintf(x->sub, 120, "%s%.2f s", P.audDelay > 0 ? "+" : "\xe2\x88\x92", fabs(P.audDelay)); snprintf(x->u, 1024, "fx:adly"); x->i = -103; x->on = P.audDelay != 0;
  x = add_item(P.tray.row, &P.tray.n, MAXROW, 4); snprintf(x->lb, 200, "%s", T("Speed")); { float sp = P.speed > 0 ? P.speed : 1; char b[16]; snprintf(b, sizeof b, "%.2gx", sp); for (char *q = b; *q; q++) if (*q == '.') *q = ','; snprintf(x->sub, 120, "%s", b); } snprintf(x->u, 1024, "fx:speed"); x->i = -104; x->on = P.speed > 0 && P.speed != 1;
}
static void dly_open_k(int kind);
static int aud_fx_choose(TItem *it) {
  if (strncmp(it->u, "fx:", 3)) return 0;
  if (!strcmp(it->u, "fx:night")) { store_set("audNight", night_on() ? "off" : "on"); aud_fx_apply(); }
  else if (!strcmp(it->u, "fx:boost")) { char b[8]; snprintf(b, sizeof b, "%d", BOOSTS[(boost_idx() + 1) % 4]); store_set("audBoost", b); aud_fx_apply(); }
  else if (!strcmp(it->u, "fx:speed")) { float sp = P.speed > 0 ? P.speed : 1; int k = 0; for (int i = 0; i < 6; i++) if (fabsf(SPEEDS[i] - sp) < 0.01f) k = i; P.speed = SPEEDS[(k + 1) % 6]; video_speed(P.speed); }
  else if (!strcmp(it->u, "fx:adly")) { dly_open_k(1); return 1; }
  render_tray(0); return 1;
}
static void tray_data(void) {
  struct { int kind; } t = { P.tray.kind }; P.tray.ntabs = 0; P.tray.n = 0; cols_clear(); P.tray.title[0] = 0;
  if (t.kind == 4) { /* subtitles: four columns; with subtitles addons installed (or subtitles the stream carried), a second tab lists theirs by language */
    TCol *c1 = &P.tray.cols[0], *c2 = &P.tray.cols[1], *c3 = &P.tray.cols[2], *c4 = &P.tray.cols[3]; P.tray.ncols = 4;
    /* the tabs: Subtitles, the addons' when there are any, then Style (size, colour, background, position and the delay,
       for whichever subtitle is on): each tab two columns, the tray narrower for it. A trailer: one list, as before */
    int addonsTab = 0, styleTab = 0;
    if (!P.isTrailer) { int ad = P.nSubAddons > 0 || P.nAdx > 0; P.tray.ntabs = 0; snprintf(P.tray.tabs[P.tray.ntabs++], 48, "%s", T("Subtitles")); if (ad) snprintf(P.tray.tabs[P.tray.ntabs++], 48, "%s", T("Addons")); snprintf(P.tray.tabs[P.tray.ntabs++], 48, "%s/%s", T("Style"), T("Delay")); /* both on that tab: "Style/Delay", no wider than it needs */
      if (P.tray.tab >= P.tray.ntabs) P.tray.tab = 0; addonsTab = ad && P.tray.tab == 1; styleTab = P.tray.tab == P.tray.ntabs - 1; } else P.tray.tab = 0;
    snprintf(c3->title, 64, "%s", T("Style")); snprintf(c4->title, 64, "%s", T("Delay"));
    { char dl[24]; snprintf(dl, sizeof dl, "%s%.2f", P.subDelay > 0 ? "+" : "", P.subDelay); snprintf(c4->vl, 32, "%s", T1("{n} s", "n", dl)); }
    TItem *x; TCol *cs = c3, *cd = c4; /* where the style and the delay go: their own tab, or (a trailer) beside the list */
    if (styleTab) { cs = c1; cd = c2; snprintf(c1->title, 64, "%s", T("Style")); snprintf(c2->title, 64, "%s", T("Delay")); { char dl[24]; snprintf(dl, sizeof dl, "%s%.2f", P.subDelay > 0 ? "+" : "", P.subDelay); snprintf(c2->vl, 32, "%s", T1("{n} s", "n", dl)); } }
    else if (!addonsTab) {
      snprintf(c1->title, 64, "%s", P.isTrailer ? T("Subtitles") : T("In the file")); snprintf(c2->title, 64, "%s", P.isTrailer ? T("Languages") : "OpenSubtitles");
      x = col_add(c1, 4); snprintf(x->lb, 200, "%s", T("Off")); x->on = !strcmp(P.curSub, "off"); snprintf(x->u, 1024, "off");
      if (!P.isTrailer) { if (!P.embKnown && !P.nEmb) { col_add(c1, 1); col_add(c1, 1); } /* the file's head is still being read */ else if (!P.nEmb) { x = col_add(c1, 0); snprintf(x->lb, 200, "%s", T("None in this file")); } else for (int i = 0; i < P.nEmb; i++) { if (!only_mine(P.emb[i].lang, 0)) continue; x = col_add(c1, 4); emb_label(i, x->lb, 200); x->i = P.emb[i].idx; char k[16]; snprintf(k, sizeof k, "emb:%d", P.emb[i].idx); x->on = !strcmp(P.curSub, k); x->spin = x->on && SL.wait; snprintf(x->u, 1024, "%s", k); } }
      /* the built-in OpenSubtitles list (the addons' are in their own tab) */
      int onExt = sub_row(1, P.ext, P.nExt);
      for (int i = 0; i < P.nExt; i++) { if (!only_mine(P.ext[i].lang, i == onExt)) continue; int same = 0; for (int j = 0; j < P.nExt; j++) if (!strcmp(P.ext[j].lang, P.ext[i].lang)) same++; x = col_add(c2, 4); if (same > 1) snprintf(x->lb, 200, "%s %d", T(P.ext[i].name), P.ext[i].n); /* the page's two spaces show as one */ else snprintf(x->lb, 200, "%s", T(P.ext[i].name)); tag_forced(x->lb, 200, P.ext[i].forced); x->on = i == onExt; x->spin = x->on && SL.wait; x->ls = 1; x->i = i; snprintf(x->u, 1024, "%s", P.ext[i].url); }
      if (!c2->n) { if (P.extLoading && !P.isTrailer) { col_add(c2, 1); col_add(c2, 1); col_add(c2, 1); } else { x = col_add(c2, 0); snprintf(x->lb, 200, "%s", T("None found")); } }
    } else {
      /* the addons' tab: their languages (the one you read first, then by name), and the chosen language's subtitles
         from every addon, each with its addon's name */
      const char *w = want_sub(); char keys[64][24], names[64][64], onKey[24] = ""; int cnt[64], nk = 0, onAdx = sub_row(2, P.adx, P.nAdx);
      if (onAdx >= 0) subs_lang_canon(P.adx[onAdx].lang, onKey, sizeof onKey);
      for (int i = 0; i < P.nAdx; i++) { const SubEntry *e = &P.adx[i]; char k[24]; subs_lang_canon(e->lang, k, sizeof k); int f = -1; for (int q = 0; q < nk; q++) if (!strcmp(keys[q], k)) f = q;
        if (f < 0 && nk < 64) { f = nk++; snprintf(keys[f], 24, "%s", k); snprintf(names[f], 64, "%s", T(e->name)); cnt[f] = 0; } if (f >= 0) cnt[f]++; }
      for (int a = 1; a < nk; a++) for (int b = a; b > 0; b--) { int pa = lang_match(keys[b - 1], w), pb = lang_match(keys[b], w); if (pb > pa || (pb == pa && strcasecmp(names[b - 1], names[b]) > 0)) { char t1[24], t2[64]; int tc = cnt[b]; snprintf(t1, 24, "%s", keys[b]); snprintf(t2, 64, "%s", names[b]); snprintf(keys[b], 24, "%s", keys[b - 1]); snprintf(names[b], 64, "%s", names[b - 1]); cnt[b] = cnt[b - 1]; snprintf(keys[b - 1], 24, "%s", t1); snprintf(names[b - 1], 64, "%s", t2); cnt[b - 1] = tc; } else break; }
      { int keep = 0; for (int q = 0; q < nk; q++) if (only_mine(keys[q], !strcmp(keys[q], onKey))) { if (q != keep) { snprintf(keys[keep], 24, "%s", keys[q]); snprintf(names[keep], 64, "%s", names[q]); cnt[keep] = cnt[q]; } keep++; } if (keep) nk = keep; } /* (only my language: the others out, unless none is left) */
      int have = 0; for (int q = 0; q < nk; q++) if (!strcmp(keys[q], P.subLang)) have = 1;
      if (!have) { int pick = -1; /* the language of the subtitle on, else the one you read, else the first */
        if (onKey[0]) for (int q = 0; q < nk; q++) if (!strcmp(keys[q], onKey)) pick = q;
        if (pick < 0 && nk) pick = 0; if (pick >= 0) snprintf(P.subLang, sizeof P.subLang, "%s", keys[pick]); }
      snprintf(c1->title, 64, "%s", T("Language")); c2->title[0] = 0;
      for (int q = 0; q < nk; q++) { x = col_add(c1, 4); snprintf(x->lb, 200, "%s", names[q]); snprintf(x->vl, 48, "%d", cnt[q]); snprintf(x->u, 1024, "lang:%s", keys[q]); x->on = !strcmp(keys[q], P.subLang); if (x->on) snprintf(c2->title, 64, "%s", names[q]);
        if (onKey[0] && !strcmp(onKey, keys[q])) x->d = 1; } /* d: the tick (a subtitle of this language is on); on: the language shown beside */
      if (!nk) { if (P.adxLoading) { col_add(c1, 1); col_add(c1, 1); col_add(c1, 1); } else { x = col_add(c1, 0); snprintf(x->lb, 200, "%s", T("None found")); } }
      if (!c2->title[0]) snprintf(c2->title, 64, "%s", T("Subtitles"));
      for (int i = 0; i < P.nAdx; i++) { const SubEntry *e = &P.adx[i]; char k[24]; if (strcmp(subs_lang_canon(e->lang, k, sizeof k), P.subLang)) continue;
        int same = 0; for (int j = 0; j < P.nAdx; j++) { char k2[24]; if (P.adx[j].order == e->order && !strcmp(subs_lang_canon(P.adx[j].lang, k2, sizeof k2), k)) same++; }
        x = col_add(c2, 4); if (e->label[0]) snprintf(x->lb, 200, "%s", e->label); else if (same > 1) snprintf(x->lb, 200, "%s %d", T(e->name), e->n); else snprintf(x->lb, 200, "%s", T(e->name));
        tag_forced(x->lb, 200, e->forced); snprintf(x->sub, 120, "%s", e->addon); x->on = i == onAdx; x->spin = x->on && SL.wait; x->ls = 2; x->i = i; snprintf(x->u, 1024, "%s", e->url); }
      if (!c2->n && nk) { x = col_add(c2, 0); snprintf(x->lb, 200, "%s", T("None found")); }
      else if (!c2->n && P.adxLoading) { col_add(c2, 1); col_add(c2, 1); }
    }
    if (!P.isTrailer) P.tray.ncols = 2; /* (the lists alone, or the style and the delay) */
    if (styleTab || P.isTrailer) {
    int ci = color_idx(); x = col_add(cs, 4); x->tool = 1; snprintf(x->lb, 200, "%s", T("Size")); snprintf(x->vl, 48, "%d%%", SUB_SIZES[size_idx()]); snprintf(x->u, 1024, "size");
    { int fl = subfont_lat(), fa = subfont_ar(); x = col_add(cs, 4); x->tool = 1; snprintf(x->lb, 200, "%s", T("Font")); snprintf(x->vl, 48, "%s", fl ? SUBF_LAT[fl] : T("Default")); x->vwt = text_sub_weight(700, fl, 0); snprintf(x->u, 1024, "font");
      if (sub_is_arabic()) { x = col_add(cs, 4); x->tool = 1; snprintf(x->lb, 200, "%s", T("Arabic font")); snprintf(x->vl, 48, "%s  \xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd8\xa7", fa ? SUBF_AR[fa] : T("Default")); x->vwt = text_sub_weight(700, 0, fa); snprintf(x->u, 1024, "fontar"); } } /* (the Arabic font only while Arabic subtitles are on) */ /* (each in its own font; the Arabic one with a sample beside its name) */
    x = col_add(cs, 4); x->tool = 1; snprintf(x->lb, 200, "%s", T("Colour")); snprintf(x->u, 1024, "color"); /* colour and background: shown by their swatch (drawn with the column) */
    x = col_add(cs, 4); x->tool = 1; snprintf(x->lb, 200, "%s", T("Background")); snprintf(x->u, 1024, "bg"); (void)ci;
    x = col_add(cs, 4); x->tool = 1; snprintf(x->lb, 200, "%s", T("Brightness")); snprintf(x->vl, 48, "%s%%", SUB_BRIGHT[bright_idx()]); snprintf(x->u, 1024, "bright");
    x = col_add(cs, 4); x->tool = 1; snprintf(x->lb, 200, "%s", T("Position")); snprintf(x->vl, 48, "%d%%", pos_pct()); snprintf(x->u, 1024, "pos");
    { int sc = P.isTrailer ? 0 : player_sub_scope(); /* what the subtitles shown can't take: greyed, one line why (styled lines keep their colours and boxes; pictures, or lines the TV draws, take nothing) */
      if (sc) { for (int i = 0; i < cs->n; i++) { TItem *y = &cs->items[i]; if (y->tool && (sc == 2 || !strcmp(y->u, "color") || !strcmp(y->u, "bg") || !strcmp(y->u, "font") || !strcmp(y->u, "fontar"))) y->grey = 1; }
        x = col_add(cs, 0); snprintf(x->lb, 200, "%s", sc == 2 ? (sub_pictures() ? T("These subtitles are pictures: their look can\xe2\x80\x99t be changed.") : T("The TV draws these subtitles: their look can\xe2\x80\x99t be changed.")) : T("Styled subtitles keep their own fonts, colours and background.")); } }
    if (!P.isTrailer && !strncmp(P.curSub, "ext:", 4)) { x = col_add(cd, 4); x->tool = 1; snprintf(x->lb, 200, "%s", T("Auto-sync")); snprintf(x->sub, 120, "%s", SY.st); snprintf(x->u, 1024, "sync"); } /* a downloaded subtitle: lined up with the file's own */
    x = col_add(cd, 4); x->tool = 1; snprintf(x->lb, 200, "%s", T("Adjust")); snprintf(x->sub, 120, "%s", T("While you watch")); snprintf(x->u, 1024, "dlyopen"); /* the small card at the top: the film stays in view */
    x = col_add(cd, 4); x->tool = 1; x->d = 0; snprintf(x->lb, 200, "%s", T("Reset")); snprintf(x->u, 1024, "dly"); }
    if (P.isTrailer) { /* one list: Off + the languages */ TCol *m = c1; for (int i = 0; i < c2->n; i++) if (c2->items[i].k == 4) { TItem *y = col_add(m, 4); *y = c2->items[i]; } free(c2->items); P.tray.cols[1] = P.tray.cols[2]; P.tray.cols[2] = P.tray.cols[3]; memset(&P.tray.cols[3], 0, sizeof P.tray.cols[3]); P.tray.ncols = 3; }
  } else if (t.kind == 3 && P.dts.conv) { snprintf(P.tray.title, 300, "%s", T("Audio")); MkvAudio ma[16]; int m = mkv_audio_tracks(mkv, ma, 16), on = P.dts.convOrd;
    /* converted here: the file's own tracks (another one is converted from the same moment) */
    if (on < 0) for (int i = 0; i < m; i++) if (aud_kind(ma[i].codec)) { on = i; break; }
    /* (with the picture converted too: every track the stream can carry, the ones this TV can't decode as Dolby Digital) */
    for (int i = 0; i < m; i++) { int asIs = P.dts.dv && !mkv_aud_bad(&ma[i]) && aud_copyable(ma[i].codec); if (!asIs && !aud_kind(ma[i].codec)) continue;
      TItem *x = add_item(P.tray.row, &P.tray.n, MAXROW, 4); const char *ln = lang_name(ma[i].lang);
      if (*ln) snprintf(x->lb, 200, "%s", ln); else if (ma[i].lang[0]) snprintf(x->lb, 200, "%s", ma[i].lang); else { char k[8]; snprintf(k, sizeof k, "%d", i + 1); snprintf(x->lb, 200, "%s %s", T("Audio"), k); }
      if (asIs) { VidTrack v; memset(&v, 0, sizeof v); snprintf(v.codec, sizeof v.codec, "%s", ma[i].codec); v.channels = ma[i].channels; aud_label(&v, x->sub, 120); }
      else snprintf(x->sub, 120, "%s", ma[i].channels >= 6 ? T("Dolby Digital 5.1 (converted)") : T("Dolby Digital 2.0 (converted)"));
      x->i = 1000 + i; x->on = i == on; }
    if (!P.tray.n) { TItem *x = add_item(P.tray.row, &P.tray.n, MAXROW, 4); snprintf(x->lb, 200, "%s", T("Dolby Digital 5.1 (converted)")); x->i = 1000 + on; x->on = 1; }
    if (!P.isTrailer) aud_fx_chips();
  } else if (t.kind == 3) { snprintf(P.tray.title, 300, "%s", T("Audio")); VidTrack ts[16]; int n = aud_list(ts, 16); int sel = P.curAud >= 0 ? P.curAud : video_audio_current();
    /* the language is the name; the format (AC3 5.1, EAC3 Atmos...) the line under it */
    for (int i = 0; i < n; i++) { TItem *x = add_item(P.tray.row, &P.tray.n, MAXROW, 4); const char *ln = lang_name(ts[i].lang);
      if (*ln) snprintf(x->lb, 200, "%s", ln); else if (ts[i].lang[0]) snprintf(x->lb, 200, "%s", ts[i].lang); else { char k[8]; snprintf(k, sizeof k, "%d", i + 1); snprintf(x->lb, 200, "%s %s", T("Audio"), k); }
      if (ts[i].name[0] && strcmp(ts[i].name, x->lb)) snprintf(x->sub, 120, "%s", ts[i].name); x->i = ts[i].idx; x->on = ts[i].idx == sel; }
    if (!n) { TItem *x = add_item(P.tray.row, &P.tray.n, MAXROW, 0); snprintf(x->lb, 200, "%s", T("None")); }
    if (!P.isTrailer) aud_fx_chips(); /* (a live channel: night mode and the boost; delay and speed are refused there below) */
  } else if (t.kind == 6) { snprintf(P.tray.title, 300, "%s", T("Channels")); /* the line-up: a card each, the one playing marked */
    for (int i = 0; i < live_lineup_n() && P.tray.n < MAXROW; i++) { const LiveCh *c = live_lineup_item(i); TItem *x = add_item(P.tray.row, &P.tray.n, MAXROW, 2);
      snprintf(x->lb, 200, "%s", c->name); snprintf(x->th, 512, "%s", c->poster); x->i = i; x->on = !strcmp(c->id, P.id); }
  } else if (t.kind == 1) { /* episodes: seasons on top, episode cards in a row */
    int ss[40], ns = 0; const cJSON *v; cJSON_ArrayForEach(v, P.videos) { int sn = cJSON_GetObjectItem(v, "season") ? (int)js_num(v, "season", 1) : 1; int f = 0; for (int i = 0; i < ns; i++) if (ss[i] == sn) f = 1; if (!f && ns < 40) ss[ns++] = sn; }
    for (int i = 1; i < ns; i++) for (int j = i; j > 0; j--) { int a = ss[j - 1], b = ss[j]; if ((a == 0 && b != 0) || (a != 0 && b != 0 && a > b)) { int tmp = ss[j]; ss[j] = ss[j - 1]; ss[j - 1] = tmp; } else break; }
    int found = 0; for (int i = 0; i < ns; i++) if (ss[i] == P.tray.season) found = 1; if (!found && ns) P.tray.season = ss[0];
    for (int i = 0; i < ns; i++) { P.tray.seasonList[i] = ss[i]; if (ss[i] == 0) snprintf(P.tray.tabs[i], 48, "%s", T("Specials")); else { char n[8]; snprintf(n, sizeof n, "%d", ss[i]); snprintf(P.tray.tabs[i], 48, "%s", T1("Season {n}", "n", n)); } if (ss[i] == P.tray.season) P.tray.tab = i; } P.tray.ntabs = ns;
    const cJSON *eps[400]; int ne = 0; cJSON_ArrayForEach(v, P.videos) { int sn = cJSON_GetObjectItem(v, "season") ? (int)js_num(v, "season", 1) : 1; if (sn == P.tray.season && ne < 400) eps[ne++] = v; }
    for (int i = 1; i < ne; i++) for (int j = i; j > 0 && js_num(eps[j - 1], "episode", 0) > js_num(eps[j], "episode", 0); j--) { const cJSON *tmp = eps[j]; eps[j] = eps[j - 1]; eps[j - 1] = tmp; }
    time_t nowT = time(NULL);
    static int wf[WATCH_MAX]; int sp = spoil_mode(); cJSON *wl = sp ? lib_get(P.id) : NULL; memset(wf, 0, sizeof wf); if (wl || (sp && tw_on())) lib_ep_watched_map(wl, P.videos, wf); /* (spoilers: what is watched, once per tray; a show not in the Library: nothing watched, hidden as on its page) */
    for (int i = 0; i < ne && P.tray.n < MAXROW; i++) { const cJSON *e = eps[i]; TItem *x = add_item(P.tray.row, &P.tray.n, MAXROW, 2); x->v = e; int en = (int)js_num(e, "episode", 0); const char *nm = js_str(e, "name", js_str(e, "title", NULL));
      if (nm) snprintf(x->lb, 200, "%d. %s", en, nm); else { char n[8]; snprintf(n, sizeof n, "%d", en); snprintf(x->lb, 200, "%d. %s", en, T1("Episode {n}", "n", n)); }
      const char *rel = js_str(e, "released", js_str(e, "firstAired", "")); int y, m, d; char ds[32] = ""; if (sscanf(rel, "%d-%d-%d", &y, &m, &d) == 3) snprintf(ds, sizeof ds, "%d/%d/%d", d, m, y);
      int up = 0; if (strlen(rel) >= 10) { struct tm tm; memset(&tm, 0, sizeof tm); if (sscanf(rel, "%d-%d-%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday) == 3) { tm.tm_year -= 1900; tm.tm_mon--; up = timegm(&tm) > nowT; } }
      if (up) snprintf(x->sub, 120, "%s", T1("Upcoming · {d}", "d", ds)); else snprintf(x->sub, 120, "%s", ds);
      snprintf(x->ov, 300, "%s", js_str(e, "overview", js_str(e, "description", "")));
      const char *th = js_str(e, "thumbnail", NULL); if (th && *th) snprintf(x->th, 512, "%s", th); else if (!strncmp(P.id, "tt", 2) && cJSON_GetObjectItem(e, "episode")) snprintf(x->th, 512, "https://episodes.metahub.space/%s/%d/%d/w300.jpg", P.id, P.tray.season, en);
      { char *p = strstr(x->th, "image.tmdb.org/t/p/"); if (p) { p += 19; char *q = strchr(p, '/'); if (q) { char rest[512]; snprintf(rest, sizeof rest, "%s", q); snprintf(p, 512 - (p - x->th), "w300%s", rest); } } }
      x->on = !strcmp(js_str(e, "id", ""), P.vid);
      if (sp) { int k = 0, w = 0; const cJSON *q; cJSON_ArrayForEach(q, P.videos) { if (q == e) { w = k < WATCH_MAX && wf[k]; break; } k++; } x->hid = !x->on && (!ep_unaired(e) || (*js_str(e, "thumbnail", "") && spoil_blurable(x->th))) && spoil_hidden(P.id, js_str(e, "id", ""), w, P.next.has ? P.next.vid : NULL); } } /* (no still: nothing to blur, never a grey card) */
  } else if (t.kind == 2 || t.kind == 5) {
    if (t.kind == 2) snprintf(P.tray.title, 300, "%s", T("Sources"));
    srcs_begin(t.kind == 5 ? P.tray.epvid : P.vid); src_tabs(); src_rows();
  }
}
static void render_tray(int fresh) {
  if (!P.tray.kind) return;
  if (src_tray()) { if (fresh) SR.idx = -1; int z = P.tray.zone; tray_data(); P.tray.zone = !fresh && z == 1 ? 1 : 0; return; } /* the sources: their own list (on the tabs: they keep it) */ char keepU[1024] = ""; char keepLb[200] = ""; int keepK = -1;
  { TItem *cur = NULL; if (P.tray.ncols) cur = P.tray.c < P.tray.ncols && P.tray.r[P.tray.c] >= 0 && P.tray.r[P.tray.c] < P.tray.cols[P.tray.c].n ? &P.tray.cols[P.tray.c].items[P.tray.r[P.tray.c]] : NULL; else cur = P.tray.idx >= 0 && P.tray.idx < P.tray.n ? &P.tray.row[P.tray.idx] : NULL;
    if (cur && !fresh) { snprintf(keepU, sizeof keepU, "%s", cur->u); snprintf(keepLb, sizeof keepLb, "%s", cur->lb); keepK = cur->k; } }
  tray_data();
  if (P.tray.ncols) {
    if (fresh) for (int i = 0; i < P.tray.ncols; i++) P.tray.r[i] = first_on(P.tray.cols[i].items, P.tray.cols[i].n);
    for (int i = 0; i < P.tray.ncols; i++) { if (P.tray.r[i] < 0 || P.tray.r[i] >= P.tray.cols[i].n || !focusable(&P.tray.cols[i].items[P.tray.r[i]])) P.tray.r[i] = first_on(P.tray.cols[i].items, P.tray.cols[i].n);
      if (!fresh && keepK >= 0) for (int j = 0; j < P.tray.cols[i].n; j++) if (P.tray.cols[i].items[j].k == keepK && !strcmp(P.tray.cols[i].items[j].u, keepU) && !strcmp(P.tray.cols[i].items[j].lb, keepLb)) { P.tray.c = i; P.tray.r[i] = j; } }
    if (fresh || P.tray.c >= P.tray.ncols || P.tray.r[P.tray.c] < 0) { P.tray.c = 0; for (int i = 0; i < P.tray.ncols; i++) if (P.tray.r[i] >= 0) { P.tray.c = i; break; } }
    if (P.tray.zone != 1 || !P.tray.ntabs) P.tray.zone = 2; /* (on the tabs: they keep it) */
  } else {
    int idx = -1; if (!fresh && keepK >= 0) for (int i = 0; i < P.tray.n; i++) if (P.tray.row[i].k == keepK && !strcmp(P.tray.row[i].lb, keepLb) && (keepK != 3 || (P.tray.row[i].st && !strcmp(P.tray.row[i].st->url, keepU)))) { idx = i; break; }
    if (idx < 0 && !fresh && P.tray.idx >= 0 && P.tray.idx < P.tray.n && focusable(&P.tray.row[P.tray.idx])) idx = P.tray.idx;
    if (idx < 0) idx = first_on(P.tray.row, P.tray.n);
    P.tray.idx = idx; if (P.tray.zone != 1 || !P.tray.ntabs) P.tray.zone = idx >= 0 ? 0 : (P.tray.ntabs ? 1 : 0);
  }
  tm_after();
}
static int subTab; /* the subtitle tab last used (kept while the app runs) */
static void open_tray(int kind) {
  if (kind != 2 && kind != 5) srcs_free(); else { mo_set(&SR.y, 0); mo_set(&SR.plA, 0); P.tray.tab = 0; SR.moved = 0; srcRestored = 0; }
  P.mode = 2; memset(&P.tray.row, 0, sizeof P.tray.row); P.tray.kind = kind; P.tray.idx = -1; P.tray.zone = 0; P.tray.onBtn = 0; P.tray.n = 0; P.tray.ntabs = 0;
  if (kind == 1) P.tray.season = season_of(P.vid);
  if (kind == 4) { load_emb_tracks(); P.tray.tab = subTab; P.tray.zone = 1; } /* the subtitles open on their tabs (kept when there are any) */
  tm_reset(); TM.quiet = 1; render_tray(1); TM.quiet = 0;
  tray_origin();
  /* it starts small at its button and grows (the closing motion is set by the classes, see css()) */
  mo_set(&P.tray.open, 0.12f); mo_set(&P.tray.openA, 0); mo_set(&P.tray.rowX, 0); mo_set(&P.tray.tabsX, 0); for (int i = 0; i < 4; i++) mo_set(&P.tray.colY[i], 0); mo_set(&P.tray.ringA, 0); P.tray.ringW = 0; for (int i = 0; i < 4; i++) mo_set(&P.tray.plA[i], 0);
}
static void tray_origin(void) { int ak = P.tray.kind < 0 ? -P.tray.kind : P.tray.kind; int b = btn_index(ak == 1 || ak == 5 ? "eps" : ak == 6 ? "chn" : ak == 2 ? "src" : ak == 3 ? "aud" : "subs");
  P.tray.ox = b >= 0 ? P.btns[b].x0 + P.btns[b].w / 2 : SB_W - 72 - 39; P.tray.oy = SB_H - 54 - 47; } /* its button's centre, measured again on close: the tray may have grown since */
static void dly_open(void);
static void pos_open(void);
static void style_open(int kind);
static void close_tray(void) { if (P.tray.kind == 4) subTab = P.tray.tab; P.mode = 1; P.tray.kind = -P.tray.kind; tray_origin(); arm_hide(); }
static void tray_back(void) {
  if (P.tray.onBtn) { close_tray(); return; }
  if (P.tray.zone == 1 && P.tray.idx >= 0) { P.tray.zone = 0; return; }
  if (P.tray.kind == 5) { srcs_free(); P.tray.kind = 1; P.tray.tab = P.tray.backTab; P.tray.season = P.tray.seasonList[P.tray.tab]; P.tray.zone = 0; render_tray(1); P.tray.idx = P.tray.backIdx; return; }
  close_tray();
}
static void adjust(const char *k) {
  if (!strcmp(k, "size")) { char v[4]; snprintf(v, sizeof v, "%d", (size_idx() + 1) % 7); store_set("subtitleSize", v); }
  else if (!strcmp(k, "color")) { int ci = color_idx(); store_set("subsColor", SUB_COLORS[((ci < 0 ? 0 : ci) + 1) % 3]); }
  else if (!strcmp(k, "bg")) store_set("subsBgStyle", SUB_BGS[(bg_idx() + 1) % 3]);
  render_tray(0);
}
static void choose(TItem *it) {
  if (!it || it->grey) return; /* (greyed: these subtitles can't take it; the line under says why) */
  if (it->k == 4 && P.tray.kind == 4) {
    if (!strncmp(it->u, "lang:", 5)) { snprintf(P.subLang, sizeof P.subLang, "%s", it->u + 5); render_tray(0); if (P.tray.ncols > 1) { int r = first_on(P.tray.cols[1].items, P.tray.cols[1].n); if (r >= 0) { P.tray.c = 1; P.tray.r[1] = r; } } } /* its subtitles: the focus goes over */
    else if (!strcmp(it->u, "off")) { subs_off(); show_pref_sub(); } else if (!strncmp(it->u, "emb:", 4)) { set_emb_sub(it->i); show_pref_sub(); } else if (it->tool) { if (strcmp(it->u, "sync")) { TM.vkC = P.tray.c; snprintf(TM.vkU, sizeof TM.vkU, "%s", it->u); mo_set(&TM.vkA, 0); mo_to(&TM.vkA, 1, .2f, C_OUT, 0); } if (!strcmp(it->u, "sync")) sync_run(); else if (!strcmp(it->u, "dlyopen")) { dly_open(); return; } else if (!strcmp(it->u, "pos")) { pos_open(); return; } else if (!strcmp(it->u, "size") || !strcmp(it->u, "color") || !strcmp(it->u, "bg") || !strcmp(it->u, "bright") || !strcmp(it->u, "font") || !strcmp(it->u, "fontar")) { style_open(!strcmp(it->u, "size") ? 1 : !strcmp(it->u, "color") ? 2 : !strcmp(it->u, "bright") ? 4 : !strcmp(it->u, "font") ? 5 : !strcmp(it->u, "fontar") ? 6 : 3); return; } else if (!strcmp(it->u, "dly")) { P.subDelay = it->d ? (double)((int)((P.subDelay + it->d) * 10 + (P.subDelay + it->d >= 0 ? 0.5 : -0.5))) / 10 : 0; if (!it->d) sync_undo(); render_tray(0); } else adjust(it->u); }
    else if (it->ls) { const SubEntry *l = it->ls == 1 ? P.ext : P.adx; int n = it->ls == 1 ? P.nExt : P.nAdx, k = it->i; /* the row's entry (found again by its file if the list moved) */
      if (k < 0 || k >= n || strcmp(l[k].url, it->u)) for (k = 0; k < n && strcmp(l[k].url, it->u); k++) ;
      if (k < n) { set_ext_sub(it->ls, &l[k]); show_pref_sub(); } }
  } else if (it->k == 4 && P.tray.kind == 3 && aud_fx_choose(it)) { }
  else if (it->k == 4 && P.tray.kind == 3 && it->i >= 1000) { int ord = it->i - 1000; /* another track converted, from here */
    { MkvAudio ma[16]; int m = mkv_audio_tracks(mkv, ma, 16); if (ord >= 0 && ord < m) show_pref_aud(ma[ord].lang, NULL); }
    if (P.dts.conv == 2 && ord != P.dts.convOrd && ord >= 0) { P.dts.convOrd = ord; P.dts.conv = 1; P.dts.convAt = video_time(); P.dts.convT0 = now_();
      int fl = conv_flags(ord); P.dts.copyA = !!(fl & HC_COPYAUDIO);
      video_unload(); P.live = 0; P.liveT0 = 0; hls_conv_start(P.url, ord, fl); fprintf(stderr, "[dts] audio track %d instead (%s)\n", ord, P.dts.copyA ? "copied" : "converted"); }
    render_tray(0); }
  else if (it->k == 4 && P.tray.kind == 3 && it->i == (P.curAud >= 0 ? P.curAud : video_audio_current())) { /* the track that already plays (the only one, often): nothing to switch, the tray closes; still the show's choice from now on (from Android TV's 10c87a66) */
    P.autoAudDone = 1; { VidTrack ts[16]; int n = video_audio_tracks(ts, 16); for (int i = 0; i < n; i++) if (ts[i].idx == it->i) show_pref_aud(ts[i].lang, ts[i].name); } fprintf(stderr, "[audio] the track playing chosen again: the tray closes\n"); close_tray(); }
  else if (it->k == 4 && P.tray.kind == 3) { P.autoAudDone = 1; set_audio(it->i); { VidTrack ts[16]; int n = video_audio_tracks(ts, 16); for (int i = 0; i < n; i++) if (ts[i].idx == it->i) show_pref_aud(ts[i].lang, ts[i].name); } render_tray(0); }
  else if (it->k == 2 && P.tray.kind == 6) { const LiveCh *c = live_lineup_item(it->i); if (c && strcmp(c->id, P.id)) { close_tray(); Item t; live_item(c, &t); live_start(&t, NULL, 0, 1); } else close_tray(); } /* a channel: switched to (the picture stays until it is found) */
  else if (it->k == 2 && it->hid) { spoil_reveal(js_str(it->v, "id", "")); it->hid = 0; sound_play(SND_TOGGLE); } /* a hidden episode: OK shows it first (its blur fading off) */
  else if (it->k == 2) { /* an episode: its sources, right here */
    const cJSON *v = it->v; P.tray.backIdx = P.tray.idx; P.tray.backTab = P.tray.tab; P.tray.kind = 5; snprintf(P.tray.epvid, sizeof P.tray.epvid, "%s", js_str(v, "id", "")); P.tray.idx = -1; P.tray.tab = 0; P.tray.zone = 0;
    snprintf(P.tray.title, 300, "S%d:E%d  ·  %s", (int)js_num(v, "season", 0), (int)js_num(v, "episode", 0), js_str(v, "name", js_str(v, "title", ""))); render_tray(1);
  } else if (it->k == 3) { int same = P.tray.kind == 2; const char *vid = same ? P.vid : P.tray.epvid; Stream s = *it->st; play_stream(&s, vid, same ? video_time() : 0); }
}
static TItem *tray_cur(void) { if (P.tray.ncols) return P.tray.c < P.tray.ncols && P.tray.r[P.tray.c] >= 0 && P.tray.r[P.tray.c] < P.tray.cols[P.tray.c].n ? &P.tray.cols[P.tray.c].items[P.tray.r[P.tray.c]] : NULL; return P.tray.idx >= 0 && P.tray.idx < P.tray.n ? &P.tray.row[P.tray.idx] : NULL; }
static int step_in(const TItem *l, int n, int from, int d) { for (int i = from + d; i >= 0 && i < n; i += d) if (focusable(&l[i])) return i; return -1; }
/* another tab: the pill glides there at once; what it shows slides out the other way, then the new one slides in */
static void tray_tab(int to) {
  if (to == P.tray.tab || to < 0 || to >= P.tray.ntabs) return;
  TM.swDir = to > P.tray.tab ? 1 : -1; P.tray.tab = to; if (P.tray.kind == 1) P.tray.season = P.tray.seasonList[to]; P.tray.idx = -1;
  TM.swPend = 1; mo_to(&TM.swA, 0, .1f, C_EASE, 0);
}
static void tray_swap_step(void) {
  if (!TM.swPend || TM.swA.on || TM.swA.v > 0.001f) return; TM.swPend = 0;
  if (P.tray.kind <= 0) { mo_set(&TM.swA, 1); return; }
  int z = P.tray.zone; TM.quiet = 1; render_tray(1); TM.quiet = 0; if (z == 1 && P.tray.ntabs) P.tray.zone = 1;
  mo_to(&TM.swA, 1, .18f, C_OUT, 0);
}
static int tray_key(int k) {
  if (P.tray.onBtn) { if (k == K_OK) close_tray(); else if (k == K_UP) P.tray.onBtn = 0; else if (k == K_LEFT || k == K_RIGHT) { close_tray(); P.bIdx = k == K_RIGHT ? (P.bIdx + 1 < P.nbtn ? P.bIdx + 1 : P.bIdx) : (P.bIdx > 0 ? P.bIdx - 1 : 0); } else if (k == K_BACK) close_tray(); return 1; }
  if (src_tray()) { /* the sources: Left / Right go from tab to tab wherever the focus is; Up from the first stream reaches the tabs */
    if (k == K_LEFT || k == K_RIGHT) { int to = P.tray.tab + (k == K_RIGHT ? 1 : -1); if (to >= 0 && to < P.tray.ntabs) { tray_tab(to); SR.moved = 0; } else nudge_start(&P.nudge, k); return 1; }
    if (k == K_BACK) { if (P.tray.kind == 5) { P.tray.zone = 0; tray_back(); } else close_tray(); return 1; } /* (Back: out at once, from the tabs or the list) */
    if (P.tray.zone == 1) { int any = 0; for (int i = 0; i < SR.n; i++) if (src_focusable(i)) any = 1;
      if ((k == K_DOWN || k == K_OK) && any) P.tray.zone = 0; else if (k == K_DOWN) P.tray.onBtn = 1; else nudge_start(&P.nudge, k); return 1; }
    if (k == K_UP || k == K_DOWN) { int d = k == K_DOWN ? 1 : -1, j = -1; for (int i = SR.idx + d; i >= 0 && i < SR.n; i += d) if (src_focusable(i)) { j = i; break; }
      if (j >= 0) { SR.idx = j; SR.moved = 1; } else if (k == K_DOWN) P.tray.onBtn = 1; else P.tray.zone = 1; }
    else if (k == K_OK) src_choose(SR.idx);
    else if (k == K_BACK) tray_back();
    return 1; }
  if (P.tray.zone == 1) {
    if ((k == K_LEFT && P.tray.tab > 0) || (k == K_RIGHT && P.tray.tab < P.tray.ntabs - 1)) tray_tab(P.tray.tab + (k == K_RIGHT ? 1 : -1));
    else if ((k == K_DOWN || k == K_OK) && P.tray.ncols) P.tray.zone = 2;
    else if ((k == K_DOWN || k == K_OK) && P.tray.idx >= 0) P.tray.zone = 0;
    else if (k == K_BACK) tray_back(); else if (k != K_OK) nudge_start(&P.nudge, k);
    return 1; }
  if (P.tray.zone == 2) { TCol *col = &P.tray.cols[P.tray.c];
    if (k == K_UP || k == K_DOWN) { int n = step_in(col->items, col->n, P.tray.r[P.tray.c], k == K_DOWN ? 1 : -1); if (n >= 0) P.tray.r[P.tray.c] = n; else if (k == K_DOWN) P.tray.onBtn = 1; else if (P.tray.ntabs) P.tray.zone = 1; else nudge_start(&P.nudge, k);
      if (n >= 0 && P.tray.kind == 4 && !strncmp(col->items[n].u, "lang:", 5)) { snprintf(P.subLang, sizeof P.subLang, "%s", col->items[n].u + 5); render_tray(0); } } /* the language under the focus: its subtitles beside it */
    else if (k == K_LEFT || k == K_RIGHT) { int c0 = P.tray.c; for (int c = P.tray.c + (k == K_RIGHT ? 1 : -1); c >= 0 && c < P.tray.ncols; c += k == K_RIGHT ? 1 : -1) if (P.tray.r[c] >= 0) { P.tray.c = c; break; } if (P.tray.c == c0) nudge_start(&P.nudge, k); }
    else if (k == K_OK) choose(tray_cur()); else if (k == K_BACK) tray_back();
    return 1; }
  if (k == K_LEFT) { int a = step_in(P.tray.row, P.tray.n, P.tray.idx, -1); if (a >= 0) P.tray.idx = a; else nudge_start(&P.nudge, k); }
  else if (k == K_RIGHT) { int b = step_in(P.tray.row, P.tray.n, P.tray.idx, 1); if (b >= 0) P.tray.idx = b; else nudge_start(&P.nudge, k); }
  else if (k == K_UP) { if (P.tray.ntabs) P.tray.zone = 1; else nudge_start(&P.nudge, k); }
  else if (k == K_DOWN) P.tray.onBtn = 1;
  else if (k == K_OK && tray_cur()) choose(tray_cur());
  else if (k == K_BACK) tray_back();
  return 1;
}

