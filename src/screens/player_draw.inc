/* etapa-89: part of player.c (included there, one translation unit): drawing: the art, subtitles, the controls, the trays */
/* ---- drawing ---- */
/* the drawing's words measured, upper-cased or wrapped once per text (and language), not every frame */
typedef struct { char s[640]; int px, wt, n, lines; float tr, w, mw; unsigned lg; } TMemo;
static int tm_same(TMemo *m, const char *s, int px, int wt, float tr) { if (m->px == px && m->wt == wt && m->tr == tr && !strcmp(m->s, s)) return 1; snprintf(m->s, sizeof m->s, "%s", s); m->px = px; m->wt = wt; m->tr = tr; return 0; }
static float tm_width(TMemo *m, const char *s, int px, int wt, float tr) { if (!tm_same(m, s, px, wt, tr) || m->n != -1) { m->n = -1; m->w = text_width(s, px, wt, tr); } return m->w; }
static int tm_wrap(TMemo *m, const char *s, float maxW, int lines, int px, int wt, float tr) { if (!tm_same(m, s, px, wt, tr) || m->mw != maxW || m->lines != lines) { m->mw = maxW; m->lines = lines; m->n = text_wrap_count(s, maxW, lines, px, wt, tr); } return m->n; }
typedef struct { char src[200], up[200]; unsigned lg; } UMemo;
static const char *um_upper(UMemo *m, const char *s) { if (m->lg != i18n_gen() || strcmp(m->src, s)) { m->lg = i18n_gen(); snprintf(m->src, sizeof m->src, "%s", s); snprintf(m->up, sizeof m->up, "%s", s); text_upper(m->up, sizeof m->up); } return m->up; }
/* where the last frame put what the pointer can click (the tray's tabs, cards and columns, the prompt) */
static struct { int tray; float x, y, w, h; int ntab; float tabX[40], tabW[40], tabY, tabH; int nrow; float rowX[MAXROW], rowW[MAXROW], rowY, rowH;
  float colX[4], colW, colTop, colBot; int nit[4]; float itY[4][300], itH[4][300]; int prompt, card, npr; float prX[2], prY[2], prW[2], prH[2]; int src; float srcX, srcTop; } PG;
/* the paused vignette and the art's shade, baked once into small alpha pictures the GPU stretches, as the page draws them
   into tiny canvases: one quad each, no rings or bands */
static GLuint vigTex, shTex;
static void bake(void) {
  if (vigTex) return;
  int W = 384, H = 216; unsigned char *a = malloc((size_t)W * H);
  for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) { float px = x + .5f, py = y + .5f, dx = (px - .6f * W) / (.744f * W), dy = (py - .4f * H) / (.62f * H), r = sqrtf(dx * dx + dy * dy);
    float ra = r <= .45f ? 0 : r >= 1 ? .6f : .6f * (r - .45f) / .55f, u = px / W, la = u < .55f ? .42f * (1 - u / .55f) : 0;
    a[y * W + x] = (unsigned char)((ra + la - ra * la) * 255 + .5f); }
  vigTex = gfx_tex_create(a, W, H, 1, 1);
  W = 256; H = 144; /* radial-gradient(ellipse at center): .28 in the middle, .92 from 78 % of the way to the corners */
  for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) { float dx = (x + .5f - W / 2.0f) / (W / 2.0f * 1.41421f), dy = (y + .5f - H / 2.0f) / (H / 2.0f * 1.41421f), r = sqrtf(dx * dx + dy * dy);
    a[y * W + x] = (unsigned char)((r >= .78f ? .92f : .28f + .64f * r / .78f) * 255 + .5f); }
  shTex = gfx_tex_create(a, W, H, 1, 1); free(a);
}
static float txt_sh(const char *t, float x, float y, int px, int wt, uint32_t col, float ls, float maxW, int align, int sa) { /* text-shadow: 0 1px 0 */
  text_draw(t, x, y + 1, px, wt, BLACK_A(sa), ls, maxW, align); return text_draw(t, x, y, px, wt, col, ls, maxW, align); }
static const char *logo_url(void) { return pageLogo[0] && !strcmp(pageLogoId, P.id) ? pageLogo : P.logo; }
static int name_shown(GLuint logoTex) { /* the name only when there is no text art by 2.5 s (never for a few frames first) */
  const char *lu = logo_url(); if (logoTex) return 0; if (lu[0] && !img_failed(lu) && now_() - P.openedAt < 2.5) return 0;
  if (!P.nameAt) P.nameAt = now_(); return 1; }
static GLuint logo_fit(float mw, float mh, float *w, float *h) {
  const char *lu = logo_url(); float asp = 1; GLuint t = lu[0] ? img_get(lu, (int)mw, (int)mh, &asp) : 0; if (!t) { *w = *h = 0; return 0; }
  *w = mw; *h = mw / asp; if (*h > mh) { *h = mh; *w = mh * asp; } return t; }
static const char *trailer_name(void) { return P.trName[0] ? P.trName : T("Trailer"); }
/* a channel loading: a deep gradient in its logo's colours, a slow glow drifting across it */
static void live_backdrop(void) {
  static char url[512]; static uint32_t c1 = RGB(40, 30, 64), c2 = RGB(14, 12, 22); static int done;
  const char *u = P.logo[0] ? P.logo : P.poster; if (strcmp(url, u)) { snprintf(url, sizeof url, "%s", u); done = 0; c1 = RGB(40, 30, 64); c2 = RGB(14, 12, 22); }
  if (!done && u[0]) { Palette p; int r = img_palette(u, 64, 64, 1, &p); if (r) { done = 1; if (r > 0) { const Swatch *v = p.vibrant.ok ? &p.vibrant : p.muted.ok ? &p.muted : NULL, *d = p.darkMuted.ok ? &p.darkMuted : p.dark.ok ? &p.dark : v;
      if (v) c1 = hsl2rgb(v->h, fminf(v->s, 0.6f), 0.26f); if (d) c2 = hsl2rgb(d->h, fminf(d->s, 0.45f), 0.08f); } } }
  gfx_rrect_v(0, 0, SB_W, SB_H, 0, c1, c2);
  float ph = (float)fmod(now_() / 12, 1) * 6.2832f; gfx_alpha_tex(shTex, -200 + 140 * sinf(ph), 0, SB_W + 400, SB_H, BLACK_A(120)); /* (the page's own shade, drifting) */
}
static void draw_art(void) {
  float a = P.artA.v; if (a <= 0.001f) return; gfx_opacity_push(a); bake();
  int ss = P.ssOn && !P.closing; /* the screensaver: the cover comes back brighter and drifts very slowly (26 s each way); the logo alone */
  gfx_rect(0, 0, SB_W, SB_H, PAGE);
  if (live_title()) live_backdrop(); /* a channel: its logo's own colours, never its logo blown up behind itself */
  float asp; GLuint bg = P.bg[0] && !live_title() ? img_get(P.bg, 1280, 720, &asp) : 0; if (bg) { float u0, v0, u1, v1; img_cover_uv(asp, SB_W / SB_H, 1, &u0, &v0, &u1, &v1);
    if (ss) { float ph = fmodf((float)(now_() - P.ssAt) / 26, 2), k = bez(C_INOUT, ph < 1 ? ph : 2 - ph), sc = 1.02f + 0.07f * k;
      gfx_scale_push(sc, sc, SB_W / 2, SB_H / 2); gfx_offset_push(SB_W * (-0.01f + 0.022f * k), SB_H * (-0.008f * k)); gfx_image(bg, 0, 0, SB_W, SB_H, 0, WHITE_A(199), u0, v0, u1, v1); gfx_offset_pop(); gfx_scale_pop(); }
    else gfx_image(bg, 0, 0, SB_W, SB_H, 0, WHITE_A(107), u0, v0, u1, v1); }
  gfx_alpha_tex(shTex, 0, 0, SB_W, SB_H, PAGE);
  float bk = !P.closing ? 1 : closeArt ? 1 - (float)((now_() - closeT0) / 0.22) : 0; /* closed while loading: the rest of the block fades as the logo leaves */
  if (!P.closing && sx_flying() && sx_since() < 0.5) { float q = (float)((sx_since() - 0.12) / 0.3); bk = q < 0 ? 0 : q > 1 ? 1 : q; bk = bk * bk * (3 - 2 * bk); } /* opened: it fades into place as the logo lands */
  if (!P.closing || bk > 0.001f) { /* the title block, centred as one column: art, trailer tag, loading line, status */
    float lw, lh; GLuint t = logo_fit(760, 260, &lw, &lh); int nm = name_shown(t), nl = 0;
    static TMemo nmW; if (nm) { nl = tm_wrap(&nmW, P.name, SB_W * 0.8f, 3, 78, 800, -1.5f); lh = nl * 78 * 1.05f; }
    int tag = P.isTrailer || !strcmp(P.type, "other"), ln = !ss && !P.failed; float L24 = text_line_height(24), L25 = text_line_height(25);
    float pwh = ss ? 0 : 92 * bez(C_INOUT, P.pwA.v); /* (Play now: 36 apart and 56 tall, its room opening smoothly) */
    float tot = lh + (tag && !ss ? 28 + L24 : 0) + (ln ? 49 : 0) + (ss ? 0 : 20 + fmaxf(34, L25)) + pwh, y = SB_H / 2 - tot / 2;
    float o = 1; if ((!P.live || P.pw.on) && !ss) { float p = fmodf((float)(now_() - P.openedAt) / 3.6f + .5f, 1); /* from full strength: it lands as it flew */ o = p < .5f ? .9f + .1f * bez(C_INOUT, p * 2) : 1 - .1f * bez(C_INOUT, (p - .5f) * 2); } /* breathing */
    if (P.closing) {} /* (the page's copy flies back) */
    else if (t) { char sk[80]; snprintf(sk, sizeof sk, "logo:%s", P.id); sx_image(sk, t, SB_W / 2 - lw / 2, y, lw, lh, 0, WHITE_A((int)(255 * o)), 0, 0, 1, 1); }
    else if (nm) { float k = kf(P.nameAt, .45f, C_LGIN, 0); { char sk[80]; snprintf(sk, sizeof sk, "logo:%s", P.id); sx_text_wrapped(sk, P.name, SB_W / 2, y, SB_W * 0.8f, 3, 78, 800, WHITE_A((int)(255 * o * k)), -1.5f, ALIGN_CENTER, 78 * 1.05f); } }
    y += lh; if (bk < 1) gfx_opacity_push(bk);
    if (tag && !ss) { y += 28; static UMemo tgU; static TMemo tgW; const char *tg = um_upper(&tgU, trailer_name()); float tw = tm_width(&tgW, tg, 24, 700, 8), totW = 84 + 22 + tw + 14 + 84, x0 = SB_W / 2 - totW / 2, ly = y + L24 / 2 - 1;
      gfx_rrect_h(x0, ly, 84, 2, 0, col_alpha(ACC, 0), ACC2); text_draw(tg, x0 + 84 + 22, y, 24, 700, WHITE_A(255), 8, 0, ALIGN_LEFT); gfx_rrect_h(x0 + 84 + 22 + tw + 14, ly, 84, 2, 0, ACC2, col_alpha(ACC, 0)); y += L24; }
    if (ln) { y += 46; float pa = P.pwA.v; /* previews first: the running bar becomes the previews' own (70 % fills it) */
      if (pa > 0.001f) { float x0 = SB_W / 2 - 130; gfx_opacity_push(pa * P.lnA.v); gfx_rrect(x0, y, 260, 3, 1.5f, WHITE_A(46)); if (P.pwF.v > 0.004f) gfx_rrect_h(x0, y, 260 * P.pwF.v, 3, 1.5f, ACC, ACC2); gfx_opacity_pop(); }
      if (P.lnA.v * (1 - pa) > 0.001f) { float x0 = SB_W / 2 - 130, p = bez(C_INOUT, clock_ph(1.5)), sx = x0 - 117 + 380.25f * p; /* 45 % of the line, from -100 % to 225 % of itself */
        gfx_opacity_push(P.lnA.v * (1 - pa)); gfx_clip_push(x0, y, 260, 3); gfx_rrect_h(sx, y, 58.5f, 3, 2, col_alpha(ACC, 0), ACC2); gfx_rrect_h(sx + 58.5f, y, 58.5f, 3, 2, ACC2, col_alpha(ACC, 0)); gfx_clip_pop(); gfx_opacity_pop(); }
      y += 3; }
    if (!ss) { y += 20; xline_draw(&stX[0], loading_line(status), SB_W / 2, y, 25, P.failed ? RGB(0xff, 0xb9, 0xb9) : WHITE_A(168), SB_W - 200, ALIGN_CENTER); y += fmaxf(34, L25); }
    if (pwh > 0.5f) { /* Play now: one quiet pill, the OK key and its words, rising into place */
      float a = bez(C_EASE, P.pwA.v), kw = key_hint(-9999, 0, K_OK, T("Play now"), WHITE_A(255), 0), w = kw + 2 * 26, h = 56, x = SB_W / 2 - w / 2, py = y + 36 + 10 * (1 - a);
      gfx_opacity_push(a); gfx_rrect(x, py, w, h, h / 2, WHITE_A(26)); gfx_ring(x, py, w, h, h / 2, 1.5f, WHITE_A(56));
      key_hint(x + 26, py + h / 2, K_OK, T("Play now"), WHITE_A(240), 0); gfx_opacity_pop(); }
    if (bk < 1) gfx_opacity_pop(); }
  gfx_opacity_pop();
}
/* the subtitle style, read once while the settings stay as they are */
typedef struct { unsigned ser; int px; uint32_t col, ol; float bo; } SubStyle;
static const SubStyle *sub_style(void) {
  static SubStyle st; if (st.ser == store_serial() && st.ser) return &st;
  st.ser = store_serial(); st.px = (int)(38 * SUB_SIZES[size_idx()] / 100.0f + 0.5f); st.col = hexcol(store_get("subsColor", "#ffffff")); st.ol = hexcol(store_get("subsOutlineColor", "#000000")); st.bo = (float)atof(SUB_BGS[bg_idx()]);
  { float k = atoi(SUB_BRIGHT[bright_idx()]) / 100.0f; st.col = col_dim(st.col, k); st.ol = col_dim(st.ol, k); asssubs_brightness(k); } /* (the brightness: plain, embedded and styled lines alike) */
  return &st;
}
/* the cue list's index, made once per list: whether it runs in start order, and the latest end so far at each cue */
typedef struct { const Cue *c; int n; const char *t0, *t1; int sorted; unsigned gen, tg; double *maxE; int cap; } CueIx;
static const CueIx *cue_ix(void) {
  static CueIx ix; int n = P.ncues;
  if (ix.c == P.cues && ix.n == n && n && ix.t0 == P.cues[0].t && ix.t1 == P.cues[n - 1].t && ix.tg == cueTimesGen) return &ix;
  ix.c = P.cues; ix.n = n; ix.t0 = n ? P.cues[0].t : NULL; ix.t1 = n ? P.cues[n - 1].t : NULL; ix.gen++; ix.tg = cueTimesGen; ix.sorted = 0;
  if (n > ix.cap) { double *m = realloc(ix.maxE, (size_t)n * sizeof *m); if (!m) return &ix; ix.maxE = m; ix.cap = n; }
  ix.sorted = 1; for (int i = 0; i < n; i++) { if (i && P.cues[i].s < P.cues[i - 1].s) ix.sorted = 0; ix.maxE[i] = i && ix.maxE[i - 1] > P.cues[i].e ? ix.maxE[i - 1] : P.cues[i].e; }
  return &ix;
}
static int sub_is_arabic(void) { const char *l = ""; /* the subtitles on are Arabic (a file's, an addon's or the film's own) */
  if (!strncmp(P.curSub, "ext:", 4)) l = P.pick.lang; else if (!strncmp(P.curSub, "emb:", 4)) { int n = atoi(P.curSub + 4); for (int i = 0; i < P.nEmb; i++) if (P.emb[i].idx == n) l = P.emb[i].lang; }
  return !strncasecmp(l, "ar", 2) && (!l[2] || l[2] == '-' || l[2] == '_' || !strcasecmp(l, "ara") || !strncasecmp(l, "arab", 4)); }
static int subw(void) { return subfont_w(600); } /* (the subtitles' weight in the viewer's fonts) */
static void draw_mkv_line(const MkvLine *l, float bx, float by, int px, uint32_t col, uint32_t ol, float bo, int align) {
  float lh = px * 1.35f; char buf[1024]; snprintf(buf, sizeof buf, "%s", l->t); char *lp[12]; int n = 0; for (char *c = strtok(buf, "\n"); c && n < 12; c = strtok(NULL, "\n")) lp[n++] = c;
  int an = l->an ? l->an : 2; float y = an >= 7 ? by : an >= 4 ? by - n * lh / 2 : by - n * lh; /* numpad: 7-9 hang from the point, 1-3 stand on it */
  if (l->color) col = 0xff000000u | (l->color & 0xffffff); /* a sign keeps its own colour */
  if (bo > 0 && n) { float mw = 0; for (int i = 0; i < n; i++) mw = fmaxf(mw, text_width(lp[i], px, subw(), 0)); float x0 = align == ALIGN_CENTER ? bx - mw / 2 : align == ALIGN_RIGHT ? bx - mw : bx;
    gfx_rrect(x0 - 16, y - 2, mw + 32, n * lh + 4, 8, BLACK_A((int)(bo * 255))); } /* one box around the cue's lines, as an addon's (one per line overlapped by 4 px: a darker band between them) */
  for (int i = 0; i < n; i++) {
    for (int d = 0; d < 8; d++) { static const float ox[8] = { -2, 2, 0, 0, -1, 1, -1, 1 }, oy[8] = { 0, 0, -2, 2, -1, 1, 1, -1 }; text_draw(lp[i], bx + ox[d], y + oy[d] + (lh - text_line_height(px)) / 2, px, subw(), ol, 0, 0, align); }
    text_draw(lp[i], bx, y + (lh - text_line_height(px)) / 2, px, subw(), col, 0, 0, align); y += lh; }
}
static int subDrew; /* draw_subs put a line on screen this frame */
static void draw_subs_lines(void) {
  float bottom = P.subB.v; /* the page's bottom: 8 %, above the controls (420), the seek bar (200) or an open tray (62 %), gliding 0.3 s */
  if (asssubs_on()) { (void)sub_style(); asssubs_draw(); subDrew = asssubs_visible(); return; } /* styled (libass): the viewer's brightness handed to its lines too (sub_style); between its lines the style's sample may show */
  if (!strncmp(P.curSub, "emb:", 4) && SL.mkv && !SL.tv && mkv) { /* the file's own lines (the TV draws them while it holds the track) */
    MkvLine ls[8]; static char tb[4096]; int n = mkv_lines_at(mkv, atoi(P.curSub + 4), video_time() - P.subDelay, ls, 8, tb, sizeof tb); if (!n) return; subDrew = 1;
    if (!SL.shown) { SL.shown = 1; fprintf(stderr, "[subs] first line on screen %.1f s after the choice: \"%.40s\"\n", now_() - SL.at, ls[0].t); }
    const SubStyle *sst = sub_style(); int px = sst->px; uint32_t col = sst->col, ol = sst->ol; float bo = sst->bo;
    float lh = px * 1.35f, stackY = SB_H - bottom;
    for (int i = n - 1; i >= 0; i--) { const MkvLine *l = &ls[i]; int an = l->an ? l->an : 2, al = an % 3 == 1 ? ALIGN_LEFT : an % 3 == 0 ? ALIGN_RIGHT : ALIGN_CENTER;
      if (l->x >= 0 && l->y >= 0) draw_mkv_line(l, l->x / 100 * SB_W, l->y / 100 * SB_H, px, col, ol, bo, al); /* placed by the file */
      else if (an >= 7) draw_mkv_line(l, al == ALIGN_LEFT ? 80 : al == ALIGN_RIGHT ? SB_W - 80 : SB_W / 2, 70, px, col, ol, bo, al);
      else if (an >= 4) draw_mkv_line(l, al == ALIGN_LEFT ? 80 : al == ALIGN_RIGHT ? SB_W - 80 : SB_W / 2, SB_H / 2, px, col, ol, bo, al);
      else { int nl = 1; for (const char *c = l->t; *c; c++) if (*c == '\n') nl++; draw_mkv_line(l, al == ALIGN_LEFT ? 80 : al == ALIGN_RIGHT ? SB_W - 80 : SB_W / 2, stackY, px, col, ol, bo, al); stackY -= nl * lh; } } /* the dialogue stacks up from the bottom */
    return; }
  if (!P.ncues || !strncmp(P.curSub, "emb", 3)) return; double t = video_time() - P.subDelay; const Cue *show[2]; int ns = 0;
  const CueIx *ix = cue_ix();
  if (!ix->sorted) { for (int i = 0; i < P.ncues; i++) if (t >= P.cues[i].s && t <= P.cues[i].e) { if (ns < 2) show[ns++] = &P.cues[i]; else { show[0] = show[1]; show[1] = &P.cues[i]; } } }
  else { /* the last two cues showing at t: those starting by t (a binary search), back from there while an earlier one can still last until t */
    int lo = 0, hi = P.ncues; while (lo < hi) { int mid = (lo + hi) / 2; if (P.cues[mid].s <= t) lo = mid + 1; else hi = mid; }
    const Cue *f[2]; int nf = 0;
    for (int i = lo - 1; i >= 0 && nf < 2; i--) { if (ix->maxE[i] < t) break; if (t >= P.cues[i].s && t <= P.cues[i].e) f[nf++] = &P.cues[i]; }
    if (nf == 2) { show[0] = f[1]; show[1] = f[0]; ns = 2; } else if (nf == 1) { show[0] = f[0]; ns = 1; } }
  if (!ns) return; subDrew = 1;
  if (!SL.shown) { SL.shown = 1; fprintf(stderr, "[subs] first line on screen %.1f s after the choice: \"%.40s\"\n", now_() - SL.at, show[0]->t); }
  const SubStyle *st = sub_style(); int px = st->px; uint32_t col = st->col, ol = st->ol; float bo = st->bo;
  float lh = px * 1.35f;
  /* the shown cues' lines, split and measured once while they stay up */
  static struct { const Cue *a, *b; unsigned gen, tg; int px, lines; char buf[4096]; char *lp[12]; float mw; } SC;
  const Cue *ca = show[0], *cb = ns > 1 ? show[1] : NULL;
  if (SC.a != ca || SC.b != cb || SC.gen != ix->gen || SC.px != px || SC.tg != subtr_gen()) { SC.a = ca; SC.b = cb; SC.gen = ix->gen; SC.px = px; SC.tg = subtr_gen();
    char all[4096] = ""; for (int i = 0; i < ns; i++) { size_t l = strlen(all); snprintf(all + l, sizeof all - l, "%s%s", i ? "\n" : "", show[i]->t); }
    SC.lines = 0; snprintf(SC.buf, sizeof SC.buf, "%s", all); for (char *c = strtok(SC.buf, "\n"); c && SC.lines < 12; c = strtok(NULL, "\n")) SC.lp[SC.lines++] = c;
    SC.mw = 0; for (int i = 0; i < SC.lines; i++) SC.mw = fmaxf(SC.mw, text_width(SC.lp[i], px, subw(), 0)); }
  int lines = SC.lines; char **lp = SC.lp;
  float y = SB_H - bottom - lines * lh;
  if (bo > 0) { float mw = SC.mw; gfx_rrect(SB_W / 2 - mw / 2 - 16, y - 2, mw + 32, lines * lh + 4, 8, BLACK_A((int)(bo * 255))); } /* one box around the lines, as the page's span */
  for (int i = 0; i < lines; i++) {
    for (int d = 0; d < 8; d++) { static const float ox[8] = { -2, 2, 0, 0, -1, 1, -1, 1 }, oy[8] = { 0, 0, -2, 2, -1, 1, 1, -1 }; text_draw(lp[i], SB_W / 2 + ox[d], y + oy[d] + (lh - text_line_height(px)) / 2, px, subw(), ol, 0, 0, ALIGN_CENTER); }
    text_draw(lp[i], SB_W / 2, y + (lh - text_line_height(px)) / 2, px, subw(), col, 0, 0, ALIGN_CENTER); y += lh; }
}
/* while the position is set: a sample line in the viewer's style where the subtitles will sit, when none shows */
static void draw_subs_(void);
/* the subtitles */
static void draw_subs(void) { text_zoom_off(1); draw_subs_(); text_zoom_off(0); } /* (at their own brightness under every tray: an open tray lifts them over it, never dims them) */
static void draw_subs_(void) {
  subDrew = 0; draw_subs_lines(); float a = P.posA.v;
  if (asssubs_on()) { asssubs_sample(a > 0.01f); return; }
  if (player_sub_scope() == 2) return; /* (the TV draws them: no sample of ours over its lines) */ /* a styled track: its own lines show the style being set (the file's nearest one when none is due), never a plain sample over them */
  if (a <= 0.01f || subDrew) return;
  const SubStyle *st = sub_style(); int px = st->px; float lh = px * 1.35f, y = SB_H - P.subB.v - lh; const char *t = P.styK == 6 || sub_is_arabic() ? "\xd8\xb3\xd8\xaa\xd8\xb8\xd9\x87\xd8\xb1\x20\xd8\xa7\xd9\x84\xd8\xaa\xd8\xb1\xd8\xac\xd9\x85\xd8\xa9\x20\xd9\x87\xd9\x86\xd8\xa7" : T("Subtitles appear here"); /* (Arabic subtitles, or their font being set: the sample in Arabic) */ float tw = text_width(t, px, subw(), 0);
  gfx_opacity_push(a * 0.9f);
  if (st->bo > 0) gfx_rrect(SB_W / 2 - tw / 2 - 16, y - 2, tw + 32, lh + 4, 8, BLACK_A((int)(st->bo * 255)));
  for (int d = 0; d < 8; d++) { static const float ox[8] = { -2, 2, 0, 0, -1, 1, -1, 1 }, oy[8] = { 0, 0, -2, 2, -1, 1, 1, -1 }; text_draw(t, SB_W / 2 + ox[d], y + oy[d] + (lh - text_line_height(px)) / 2, px, subw(), st->ol, 0, 0, ALIGN_CENTER); }
  text_draw(t, SB_W / 2, y + (lh - text_line_height(px)) / 2, px, subw(), st->col, 0, 0, ALIGN_CENTER);
  gfx_opacity_pop();
}
/* seek previews on the TV (core/seekprev): a scrub with the remote shows the picture at its time over the seek pill,
   its neighbours either side smaller and fainter; the pointer over the bar, one picture over its time. Its own keyframe
   sharp, else the nearest made (drawn the same), none yet: the pill alone, as before. How much of the film has its
   pictures shows a moment as a scrub starts ("Previews 64%", "Previews ready") */
/* scrubbing with the remote: the film steps back (a little darker), the picture at the scrub's time large in the middle,
   its two neighbours either side smaller and fainter, sliding out from behind it as it opens; the time under it and
   how far from where the scrub began. In 0.18 s, out 0.45 s (where it was, after the seek is let go). No picture yet
   for that time: the same card waiting, a ring filling with how much of the film has its pictures (as the phone) */
/* the videos whose "Previews ready" has been seen (the store, the last 200, newest last): it is said once a video */
static int ready_seen(const char *vid) { const char *l = store_get("prevReadySeen", ""); size_t n = strlen(vid); if (!n) return 0;
  for (const char *p = l; (p = strstr(p, vid)); p += n) if ((p == l || p[-1] == ' ') && (p[n] == ' ' || !p[n])) return 1; return 0; }
static void ready_mark(const char *vid) { if (!vid || !*vid || ready_seen(vid)) return; const char *l = store_get("prevReadySeen", ""); char b[8192]; snprintf(b, sizeof b, "%s%s%s", l, *l ? " " : "", vid);
  int words = 1; for (const char *p = b; *p; p++) words += *p == ' '; char *q = b; while (words > 200 && (q = strchr(q, ' '))) { q++; words--; } /* (the oldest go) */
  store_set("prevReadySeen", q && words <= 200 ? q : b); }
/* under the controls while the previews are up: the film a little darker, a light vignette (one stretched quad) */
static void prev_shade(void) { if (stripT < 0 || seekK.v <= 0.01f || live_bcast()) return; float k = ease_apply(EASE_OUT, clampf(seekK.v, 0, 1));
  gfx_rect(0, 0, SB_W, SB_H, BLACK_A((int)(56 * k))); bake(); if (vigTex) gfx_alpha_tex(vigTex, 0, 0, SB_W, SB_H, BLACK_A((int)(96 * k))); }
static void prev_strip(double t, double from, float a) {
  if (a <= 0.01f || !prev_enabled() || !sp_active()) return;
  /* a conveyor, one place a press: whatever the film, its length or its previews' spacing, each press slides the row
     exactly one picture over (0.24 s, ease out), the one reaching the middle growing into it, the one leaving it
     shrinking back; nothing else moves or starts again while the scrub goes on. The places are the presses: the middle
     the scrub's time, each neighbour one step before or after it. Presses quicker than the slide catch it up, never
     more than 1.5 places behind. */
  double sl = sp_slot(); if (sl <= 0) sl = 10;
  float k = ease_apply(EASE_OUT, clampf(seekK.v, 0, 1)), cw = fminf(SB_W * 0.40f, 800);
  static float asp = 0.5625f; static double cvFrom, cvAt, lastT, stepT; static int cvN, cvOn;
  double now = now_();
  #define CV_POS() (cvFrom + (cvN - cvFrom) * ease_apply(EASE_OUT, clampf((float)((now - cvAt) / 0.24), 0, 1)))
  if (!cvOn || seekK.v < 0.05f) { cvN = 0; cvFrom = 0; cvAt = now - 1; lastT = t; stepT = atof(store_get("seekStep", "10")); if (stepT < 1) stepT = 10; cvOn = 1; } /* (opening: where it is, no slide in) */
  else if (fabs(t - lastT) > 0.05) { int dir = t > lastT ? 1 : -1; double cur = CV_POS(); stepT = fabs(t - lastT); lastT = t; cvN += dir;
    if (fabs(cvN - cur) > 1.5) cur = cvN - dir * 1.5; cvFrom = cur; cvAt = now; }
  double pos = CV_POS();
  #undef CV_POS
  SpPic fm; if (sp_get_cell(t, -1, 1, &fm) && fm.w > 0) { float r = (float)fm.h / fm.w; asp = r > 0.62f ? 0.62f : r; }
  float ch = cw * asp, cx = SB_W / 2, cy = SB_H * 0.40f + 18 * (1 - k), pitch = cw * 0.89f + 26, rc = 30; /* (the neighbours 0.78 of the middle, 26 px apart) */
  int pc = sp_progress(); if (pc < 0) pc = 0;
  static struct { int i; double at, ct; SpPic pic, was; } seen[12]; /* each place: the picture it shows, and the one it shows less of as a nearer one comes in (0.3 s) */
  if (seekK.v < 0.05f) for (int q = 0; q < 12; q++) seen[q].i = INT32_MIN;
  gfx_opacity_push(a * k); /* (the whole strip, its middle picture and time too, in and out together) */
  int lo = (int)floor(pos) - 2, hi = (int)ceil(pos) + 2, ord[8], no = 0;
  for (int i = lo; i <= hi && no < 8; i++) if (fabs(i - pos) < 1.75) ord[no++] = i;
  for (int x = 0; x < no; x++) for (int y = x + 1; y < no; y++) if (fabs(ord[y] - pos) > fabs(ord[x] - pos)) { int w = ord[x]; ord[x] = ord[y]; ord[y] = w; } /* the farthest first, the middle on top */
  for (int o = 0; o < no; o++) { int i = ord[o]; double ct = t + (i - cvN) * stepT; if (ct < -stepT * 0.5 || ct > dur() + stepT * 0.5) continue; if (ct < 0) ct = 0;
    float d = (float)(i - pos), ad = fabsf(d), g = ad > 1 ? 1 : ad, e = g * g * (3 - 2 * g); /* 0 the middle .. 1 a neighbour */
    float sc = 1 - 0.22f * e, al = ad > 1 ? clampf((1.75f - ad) / 0.75f, 0, 1) : 1;
    int tint = 255 - (int)(78 * e); /* (the neighbours a little darker, never see-through) */
    float w = cw * sc, h = ch * sc, x = cx + d * pitch * (0.75f + 0.25f * k) - w / 2, y = cy - h / 2, r = rc * (1 - 0.2f * e);
    if (x + w < 0 || x > SB_W || al <= 0.005f) continue;
    /* the picture made nearest the place: always a real one (never a loader); a nearer one coming in crossfades over it */
    SpPic m; int has = sp_get_cell(ct, -1, i == cvN, &m) && m.w > 0;
    int q = 0, free_ = -1; for (; q < 12; q++) { if (seen[q].i == i && fabs(seen[q].ct - ct) < 0.5) break; if (free_ < 0 && (seen[q].i == INT32_MIN || abs(seen[q].i - cvN) > 3)) free_ = q; }
    if (!has && (q == 12 || seen[q].pic.w <= 0 || !(sp_get_cell(seen[q].pic.t, 0.05, i == cvN, &m) && m.w > 0 && m.k == seen[q].pic.k))) continue; /* (its picture still being unpacked: the place keeps the one it showed; a place that never had one waits, the middle first) */
    if (q == 12) { q = free_ >= 0 ? free_ : 0; seen[q].i = i; seen[q].ct = ct; seen[q].pic = m; seen[q].at = 0; }
    else if (seen[q].pic.k != m.k && stripHold && now - stripMovedAt < 0.35) { seen[q].was = seen[q].pic; seen[q].pic = m; seen[q].at = now; } /* (a nearer one while the row moves; stopped, what shows stays) */
    else if (seen[q].pic.k != m.k) { SpPic o; if (sp_get_cell(seen[q].pic.t, 0.05, i == cvN, &o) && o.k == seen[q].pic.k) m = o; }
    float pa = seen[q].at == 0 ? 1 : clampf((float)((now - seen[q].at) / 0.3), 0, 1); pa = pa * pa * (3 - 2 * pa);
    gfx_opacity_push(al);
    if (ad < 0.5f) gfx_shadow(x, y + 16 * sc, w, h, r, 56, BLACK_A((int)(170 * (1 - e))));
    gfx_rrect(x, y, w, h, r, RGBA(14, 12, 20, 255)); /* (solid behind: nothing of the film through a picture as it crossfades) */
    uint32_t tc = RGBA(tint, tint, tint, 255);
    if (pa < 0.999f) { SpPic o; if (sp_get_cell(seen[q].was.t, 0.05, 0, &o) && o.k == seen[q].was.k) gfx_image(o.tex, x, y, w, h, r, tc, 0, o.v0, o.u1, o.v1); } /* (asked again each frame: its texture kept, never another picture's) */
    if (pa < 0.999f) gfx_opacity_push(pa); gfx_image(m.tex, x, y, w, h, r, tc, 0, m.v0, m.u1, m.v1); if (pa < 0.999f) gfx_opacity_pop();
    gfx_ring(x, y, w, h, r, 1.2f, WHITE_A((int)(34 * (1 - 0.4f * e))));
    gfx_opacity_pop(); }
  float fx = cx - cw / 2, fy = cy - ch / 2;
  char tt[24]; fmt_time(t, tt, sizeof tt); float ty = fy + ch + 20; (void)fx; text_draw(tt, cx, ty, 42, 800, WHITE_A(250), -0.5f, 0, ALIGN_CENTER); ty += text_line_height(42);
  { double dt = t - from; char dl[24], sg[32]; fmt_time(fabs(dt), dl, sizeof dl); snprintf(sg, sizeof sg, "%s%s", dt < 0 ? "\xe2\x88\x92" : "+", dl); text_draw(sg, cx, ty + 2, 24, 700, WHITE_A(170), 0, 0, ALIGN_CENTER); ty += text_line_height(24) + 4; }
  { static double at90, scrubAt; static int doneHere; if (scrubAt != seekStartAt) { scrubAt = seekStartAt; at90 = pc >= 90 ? 1 : 0; doneHere = pc >= 100 && ready_seen(P.vid); } if (pc >= 90 && !at90) at90 = now; else if (pc < 90) at90 = 0; /* (under 90 %: it stays while the scrub goes on; from 90 %: it goes after a moment, 1.5 s after reaching it at most) */
    if (pc >= 100 && !doneHere) ready_mark(P.vid); /* ("Previews ready" shows once for a video, the scrub it is first seen on; never again after, this run or the next) */
    double goAt = doneHere ? -1e18 : at90 == 0 ? 1e18 : fmax(seekStartAt + 3.5, at90 == 1 ? 0 : at90 + 1.5);
    float ha = clampf(1 - (float)(now - goAt) / 0.6f, 0, 1); if (ha > 0.01f) { /* how much has its pictures ("Previews 64%", "Previews ready"); it leaves with the strip (inside its fade) */
      char n[48], pn[8]; snprintf(pn, sizeof pn, "%d", pc); snprintf(n, sizeof n, "%s", pc >= 100 ? T("Previews ready") : T1("Previews {n}%", "n", pn)); float tw = text_width(n, 28, 700, 0), r = 13, w = 2 * r + 12 + tw + 36, h = 50, gx = cx - w / 2, gy = fy - 40 - h; (void)ty; /* (above the middle picture: clear of the time, the bar and the subtitles) */
      gfx_opacity_push(ha); gfx_rrect(gx, gy, w, h, h / 2, BLACK_A(120)); gfx_ring(gx, gy, w, h, h / 2, 1, WHITE_A(30)); float rx = gx + 18, ry = gy + h / 2;
      gfx_ring(rx, ry - r, 2 * r, 2 * r, r, 3, WHITE_A(45)); int seg = 48; for (int q = 0; q < seg * pc / 100; q++) { float an = -1.5708f + 6.2832f * q / seg; gfx_circle(rx + r + cosf(an) * (r - 1.5f), ry + sinf(an) * (r - 1.5f), 1.8f, ACC2 | 0xff000000u); }
      text_draw(n, rx + 2 * r + 12, ry - text_line_height(28) / 2.0f, 28, 700, WHITE_A(235), 0, 0, ALIGN_LEFT); gfx_opacity_pop(); } }
  gfx_opacity_pop(); }
static void prev_pic(double t, float cx, float bottom, float a) { /* the pointer over the bar: one picture over its time */
  SpPic m; if (a <= 0.01f || !prev_enabled() || !sp_get(t, &m) || m.w <= 0) return; float w = 320, h = w * m.h / m.w; if (h > w * 0.62f) h = w * 0.62f;
  float x = clampf(cx - w / 2, 72, SB_W - 72 - w), y = bottom - 12 - h; gfx_opacity_push(a); gfx_shadow(x, y + 10, w, h, 16, 34, BLACK_A(150)); gfx_image(m.tex, x, y, w, h, 16, WHITE_A(255), 0, m.v0, m.u1, m.v1); gfx_ring(x, y, w, h, 16, 1, WHITE_A(30)); gfx_opacity_pop(); }
/* the CC button of a trailer: the subtitles icon, and when they are off a slash drawn from its top-left (0.26 s) with a gap cut around it */
static void cc_icon(float x, float y, float sz, uint32_t col, uint32_t gap) {
  float k = P.ccS.v, u = sz / 24; icon_draw("p_subs", x, y, sz, col_alpha(col, k > 0.5f && col == WHITE_A(255) ? 0.8f : 1));
  if (k <= 0.001f) return;
  float g = (2.6f + 17.68f * k) * u; gfx_clip_push(x, y, g, g); icon_draw("p_ccgap", x, y, sz, gap); gfx_clip_pop();
  float c = (3.4f + 17.68f * k + 1) * u; gfx_clip_push(x, y, c, c); icon_draw("p_ccslash", x, y, sz, col); gfx_clip_pop();
}
static void draw_osd(void) {
  { float t = (float)now_(); lglass_light(0.018f * sinf(t * 0.157f) + 0.012f * sinf(t * 0.263f + 1.3f) + 0.008f * sinf(t * 0.411f + 4.1f)); } /* the glass's light wandering, slowly and a little */
  float W = SB_W, H = SB_H, bx = 72, rowTop = H - 54 - 94, L25 = text_line_height(25), barTop = rowTop - 30 - L25, trkC = barTop + L25 / 2, ttlBot = barTop - 32;
  bake();
  /* the shades: the bottom for the controls, the top for the clock, the vignette when paused */
  if (P.dim.v > 0.001f) { gfx_opacity_push(P.dim.v); gfx_rrect_v(0, H * 0.44f, W, H * 0.56f * 0.54f, 0, BLACK_A(0), BLACK_A(160)); gfx_rrect_v(0, H * 0.44f + H * 0.56f * 0.54f, W, H * 0.56f * 0.46f, 0, BLACK_A(160), BLACK_A(238)); gfx_opacity_pop(); }
  if (P.dimT.v > 0.001f) { gfx_opacity_push(P.dimT.v); gfx_rrect_v(0, 0, W, 260, 0, BLACK_A(175), BLACK_A(0)); gfx_opacity_pop(); }
  if (P.vig.v > 0.001f) gfx_alpha_tex(vigTex, 0, 0, W, H, BLACK_A((int)(255 * P.vig.v)));
  draw_subs();
  /* paused: who plays in it (Apple TV's InSight), top right under the clock */
  if (P.vig.v > 0.02f && !XR.on && XR.a.v < 0.02f && !P.isTrailer && !live_title() && P.tray.kind <= 0 && strcmp(store_get("insight", "true"), "false")) { const InsightFace *fc; int ep = episodic();
    int nf = insight_get(P.type, P.id, ep ? P.season : -1, ep ? P.episode : -1, &fc); if (nf > 5) nf = 5;
    if (nf > 0) { float cwid = 132, gap = 18, tot = nf * cwid + (nf - 1) * gap, x0 = W - 72 - tot, y0 = 150; gfx_opacity_push(P.vig.v);
      text_draw(T(ep ? "In this episode" : "In this film"), x0, y0, 26, 800, WHITE_A(235), 0, 0, ALIGN_LEFT); key_hint(W - 72, y0 + 15, K_UP, T("X-Ray"), WHITE_A(190), 1); y0 += 46;
      for (int i = 0; i < nf; i++) { float x = x0 + i * (cwid + gap), d = 104, cx = x + (cwid - d) / 2; float asp; GLuint t = img_get(fc[i].photo, 185, 278, &asp);
        gfx_circle(cx + d / 2, y0 + d / 2, d / 2 + 3, WHITE_A(60));
        if (t) { float a2 = asp > 0 && asp < 1 ? asp : 0.667f, v0 = (1 - a2) * 0.2f; gfx_image(t, cx, y0, d, d, d / 2, WHITE_A(255), 0, v0, 1, v0 + a2); } /* (the top of the portrait, square, round) */ else gfx_circle(cx + d / 2, y0 + d / 2, d / 2, WHITE_A(40));
        text_draw(fc[i].name, x + cwid / 2, y0 + d + 12, 21, 700, WHITE_A(240), 0, cwid + 8, ALIGN_CENTER);
        if (fc[i].role[0]) text_draw(fc[i].role, x + cwid / 2, y0 + d + 40, 18, 500, WHITE_A(170), 0, cwid + 8, ALIGN_CENTER); }
      gfx_opacity_pop(); } }
  /* buffering: a quarter of the ring turning */
  if (P.bufA.v > 0.001f) { gfx_opacity_push(P.bufA.v); float cx = W / 2, cy = H / 2 - 62 + 40; gfx_ring(cx - 40, cy - 40, 80, 80, 40, 5, WHITE_A(36));
    float ph = clock_ph(0.9) * 6.2832f; for (int i = 0; i <= 20; i++) { float an = ph - 2.356f + 1.5708f * i / 20; gfx_circle(cx + cosf(an) * 37.5f, cy + sinf(an) * 37.5f, 2.5f, ACC2); }
    xline_draw(&stX[1], status, cx, cy + 40 + 22, 24, WHITE_A(204), 0, ALIGN_CENTER); gfx_opacity_pop(); }
  /* top left, as the film starts, for 6 s (under the peers chip while the controls show it): one glass pill like the peers chip, the age in a small
     accent badge, then what for, each with three small dots for how much; the reasons come in one after the other */
  if (P.pgA.v > 0.001f) { float x = 72 + P.pgY.v, y = 52 + 150 * P.ttlA.v, h = 52, /* etapa-87: under the title, now up top */ t = (float)(now_() - P.pg.at); char ag[40]; if (P.pg.age > 0) snprintf(ag, sizeof ag, "%d+", P.pg.age); else snprintf(ag, sizeof ag, "%s", T("All ages"));
    static float nw[PG_CATS]; static unsigned lg; if (lg != i18n_gen() || !nw[0]) { lg = i18n_gen(); for (int c = 0; c < PG_CATS; c++) nw[c] = text_width(parental_cat_name(c), 22, 600, 0); }
    float bw = text_width(ag, 25, 800, 0) + 22, /* (the age a step above the rest) */ w = 9 + bw + 16; for (int i = 0; i < P.pg.g.n; i++) w += nw[P.pg.g.cat[i]] + 9 + 2 * 10 + 7 + (i + 1 < P.pg.g.n ? 20 : 18); if (!P.pg.g.n) w = 9 + bw + 9; /* (the age alone) */
    gfx_opacity_push(P.pgA.v);
    if (!P.pg.g.n) { /* the age alone: one pill, the official rating beside it when there is one ("TV-MA · 16+") */
      char one[200]; int same = !P.pg.cert[0] || !strcmp(P.pg.cert, ag) || (atoi(P.pg.cert) == P.pg.age && strchr(P.pg.cert, '+')); snprintf(one, sizeof one, same ? "%s" : "%s  ·  %s", same ? ag : P.pg.cert, ag);
      if (P.pg.desc[0]) { size_t l = strlen(one); snprintf(one + l, sizeof one - l, "  ·  %s", P.pg.desc); } /* and what for, as the rating says */
      float ow = text_width(one, 25, 800, 0) + 48; lglass_shell(x, y, ow, h, h / 2, RGBA(10, 12, 22, 150), 1); text_draw(one, x + ow / 2, y + (h - text_line_height(25)) / 2, 25, 800, WHITE_A(250), 0, 0, ALIGN_CENTER);
      gfx_opacity_pop(); }
    else {
    lglass_shell(x, y, w, h, h / 2, RGBA(10, 12, 22, 150), 1);
    float bx = x + 7, by = y + 7; gfx_rrect(bx, by, bw, h - 14, (h - 14) / 2, acc_a(0.35f)); text_draw(ag, bx + bw / 2, by + (h - 14 - text_line_height(25)) / 2, 25, 800, WHITE_A(255), 0, 0, ALIGN_CENTER);
    float rx = bx + bw + 16, ry = y + (h - text_line_height(22)) / 2, cy = y + h / 2;
    for (int i = 0; i < P.pg.g.n; i++) { float k = (t - 0.25f - i * 0.12f) / 0.35f; k = k < 0 ? 0 : k > 1 ? 1 : k; k = 1 - (1 - k) * (1 - k); int c = P.pg.g.cat[i], lv = P.pg.g.lvl[i];
      if (k > 0.004f) { gfx_opacity_push(k); text_draw(parental_cat_name(c), rx, ry, 22, 600, WHITE_A(225), 0, 0, ALIGN_LEFT);
        for (int d = 0; d < 3; d++) gfx_circle(rx + nw[c] + 9 + 3.5f + d * 10, cy, 3.5f, d < lv ? (lv == PG_SEVERE ? RGB(0xf0, 0xb9, 0x4a) : WHITE_A(220)) : WHITE_A(55));
        gfx_opacity_pop(); }
      rx += nw[c] + 9 + 2 * 10 + 7 + 20; }
    gfx_opacity_pop(); } }
  /* top left, with the controls: a torrent's peers and speed */
  if (P.p2pA.v > 0.001f) { gfx_opacity_push(P.p2pA.v); static TMemo p2W; float tw = tm_width(&p2W, P.p2pLine, 24, 600, 0), w = 16 + 10 + 12 + tw + 20 + 4;
    float px = SB_W / 2 - w / 2; /* etapa-87: top centre (the title has the top left) */
    lglass_shell(px, 52, w, 46, 23, RGBA(10, 12, 22, 150), 1); gfx_circle(px + 2 + 16 + 5, 52 + 23, 5, RGB(0x4a, 0xde, 0x80));
    xline_draw(&stX[2], P.p2pLine, px + 2 + 16 + 10 + 12, 52 + (46 - text_line_height(24)) / 2, 24, WHITE_A(255), 0, ALIGN_LEFT); gfx_opacity_pop(); }
  /* top right: the clock, the pages' own (a new minute: only its changed digits crossfade), and when it ends */
  if (P.topR.v > 0.001f) { gfx_opacity_push(P.topR.v); gfx_offset_push(0, P.topY.v);
    app_clock_draw(W - 64, 46, 1);
    double d = dur(); if (d > 0 && !live_bcast()) { time_t t = time(NULL) + (time_t)(d - video_time()); /* (live: nothing ends) */
      static char s[64]; static long sMin = -1; static unsigned sLg; /* made again when its minute or the language changes */
      if ((long)(t / 60) != sMin || sLg != i18n_gen()) { sMin = (long)(t / 60); sLg = i18n_gen(); struct tm tm; localtime_r(&t, &tm); char e[16]; app_fmt_hm(tm.tm_hour, tm.tm_min, e, sizeof e); snprintf(s, sizeof s, "%s", T1("Ends at {t}", "t", e)); text_upper(s, sizeof s); }
      txt_sh(s, W - 64, 46 + text_line_height(38) / 2 + 38 + 14, 20, 600, WHITE_A(178), 1.6f, 0, ALIGN_RIGHT, 128); } /* a quiet line just under the clock's glass, in line with the time */
    gfx_offset_pop(); gfx_opacity_pop(); }
  /* bottom left: the title's art, S:E and its name, the info chips, year · genre · via addon (laid out from the bar up) */
  if (P.ttlA.v > 0.001f) { gfx_opacity_push(P.ttlA.v); gfx_offset_push(P.ttlX.v, P.ttlY.v);
    static char viaSrc[96], mt2[96]; static unsigned viaLg; /* "via addon": made when the addon or the language changes */
    if (viaLg != i18n_gen() || strcmp(viaSrc, P.saddon)) { viaLg = i18n_gen(); snprintf(viaSrc, sizeof viaSrc, "%s", P.saddon); char via[120]; snprintf(via, sizeof via, "%s", P.saddon); char *v = via; while (*v && !isalnum((unsigned char)*v)) v++; snprintf(mt2, sizeof mt2, "%s", *v ? T1("via {a}", "a", v) : ""); }
    const char *mt[3] = { P.isTrailer ? "" : P.year, P.isTrailer || !P.genre[0] ? "" : T(P.genre), P.isTrailer || !mt2[0] ? "" : mt2 };
    int hasMt = mt[0][0] || mt[1][0] || mt[2][0], hasSe = P.isTrailer || P.episode > 0; /* (an anime episode: "E5", no season) */ float L30 = text_line_height(30);
    float y = ttlBot, mtTop = 0, infTop = 0, seTop = 0;
    if (hasMt) { mtTop = y - L25; y = mtTop - 14; }
    if (P.nchip) { infTop = y - 46; y = infTop - 18; }
    (void)y; /* etapa-87: the title moves up top, small (the logo 340 x 88, the name in words 36) */
    float lw, lh; GLuint t = logo_fit(340, 88, &lw, &lh); int nm = name_shown(t), nl = 0; static TMemo nmW; if (nm) { nl = tm_wrap(&nmW, P.name, 900, 2, 36, 800, -1); lh = nl * 36 * 1.05f; }
    float lgTop = 48; gfx_offset_push(-P.ttlX.v, -P.ttlY.v * 0.5f); /* (it comes in from above, not from below) */
    if (hasSe) seTop = lgTop + lh + 12;
    { float py = (hasSe ? seTop + 36 : lgTop + lh) + 18; /* paused: PAUSED, then the episode's (or film's) story */
      if (P.pzA.v > 0.001f) { static UMemo pzU; const char *pz = um_upper(&pzU, T("Paused")); text_draw(pz, bx, py, 22, 700, WHITE_A((int)(199 * P.pzA.v)), 6, 0, ALIGN_LEFT);
        const char *ov = NULL; if (P.videos) { const cJSON *e; cJSON_ArrayForEach(e, P.videos) if (!strcmp(js_str(e, "id", ""), P.vid)) { ov = js_str(e, "overview", js_str(e, "description", NULL)); break; } }
        if ((!ov || !*ov) && !episodic() && P.meta) ov = js_str(P.meta, "description", NULL);
        if (ov && *ov && !P.isTrailer) text_draw_wrapped(ov, bx, py + 40, 900, 4, 25, 400, WHITE_A((int)(220 * P.pzA.v)), 0, ALIGN_LEFT, 36); } }

    if (t) { char sk[80]; snprintf(sk, sizeof sk, "logo:%s", P.id); sx_image(sk, t, bx, lgTop, lw, lh, 0, WHITE_A(255), 0, 0, 1, 1); } else if (nm) text_draw_wrapped(P.name, bx, lgTop, 900, 2, 42, 800, WHITE_A((int)(255 * kf(P.nameAt, .45f, C_LGIN, 0))), -1, ALIGN_LEFT, 42 * 1.05f);
    if (hasSe) { float ty = seTop + (36 - L30) / 2;
      if (P.isTrailer) { static UMemo tgU; const char *tg = um_upper(&tgU, trailer_name()); text_draw(tg, bx, seTop + (36 - text_line_height(22)) / 2, 22, 700, ACC2, 6, 0, ALIGN_LEFT); }
      else { char se[16]; if (P.season >= 0) snprintf(se, sizeof se, "S%d:E%d", P.season, P.episode); else snprintf(se, sizeof se, "E%d", P.episode); float w = txt_sh(se, bx, ty, 27, 800, WHITE_A(255), 0.5f, 0, ALIGN_LEFT, 153);
        if (P.epTitle[0]) { gfx_circle(bx + w + 16 + 3.5f, seTop + 18, 3.5f, RGB(0xe5, 0x1a, 0x2e)); txt_sh(P.epTitle, bx + w + 16 + 7 + 16, ty, 27, 600, WHITE_A(230), 0, 1000, ALIGN_LEFT, 153); } } }
    gfx_offset_pop(); /* (the top's own) */
    if (P.nchip) { float cx = bx, L24 = text_line_height(24);
      for (int i = 0; i < P.nchip; i++) { float k = kf(P.chip[i].at, .42f, C_OUT, P.chip[i].delay); if (P.chip[i].at && now_() - P.chip[i].at < P.chip[i].delay) k = 0; /* the entrance keeps its first frame through the delay */
        if (P.chip[i].sep) { cx += 4; gfx_rect(cx, infTop + 6 + 8 * (1 - k), 2, 34, WHITE_A((int)(115 * k))); cx += 2 + 16; continue; }
        static TMemo chW[8]; int b = P.chip[i].bold; float tw = tm_width(&chW[i], P.chip[i].t, 24, b ? 800 : 600, b ? 0.5f : 0), cy = infTop + 8 * (1 - k);
        float xk = P.chip[i].xAt ? clampf((float)((now_() - P.chip[i].xAt) / 0.26), 0, 1) : 1; xk = 1 - (1 - xk) * (1 - xk) * (1 - xk); if (xk >= 1) P.chip[i].xAt = 0; /* its words changing */
        if (xk < 1) tw = P.chip[i].oldW + (tw - P.chip[i].oldW) * xk;
        float w = 40 + tw + (P.chip[i].icon ? 36 : 0);
        if (k > 0.001f) { gfx_opacity_push(k); chip_bg(P.chip[i].t, cx, cy, w); float tx = cx + 20, ty = cy + (46 - L24) / 2;
          if (P.chip[i].icon) { icon_draw(P.chip[i].icon, tx, cy + 10, 26, WHITE_A(230)); tx += 36; }
          if (xk < 1) { gfx_clip_push(cx, cy, w, 46); text_draw(P.chip[i].old, tx, ty - 6 * xk, 24, b ? 800 : 600, WHITE_A((int)(255 * (1 - xk))), b ? 0.5f : 0, 0, ALIGN_LEFT); text_draw(P.chip[i].t, tx, ty + 6 * (1 - xk), 24, b ? 800 : 600, WHITE_A((int)(255 * xk)), b ? 0.5f : 0, 0, ALIGN_LEFT); gfx_clip_pop(); }
          else text_draw(P.chip[i].t, tx, ty, 24, b ? 800 : 600, WHITE_A(255), b ? 0.5f : 0, 0, ALIGN_LEFT); gfx_opacity_pop(); }
        cx += w + 12; } }
    if (hasMt) { float x = bx; int any = 0; static TMemo dotW; float bw = tm_width(&dotW, "•", 25, 400, 0);
      for (int i = 0; i < 3; i++) { if (!mt[i][0]) continue; if (any) { txt_sh("•", x + 14, mtTop, 25, 400, WHITE_A(128), 0, 0, ALIGN_LEFT, 128); x += 14 + bw + 14; } x += txt_sh(mt[i], x, mtTop, 25, 500, WHITE_A(191), 0, 0, ALIGN_LEFT, 128); any = 1; } }
    gfx_offset_pop(); gfx_opacity_pop(); }
  /* the bar: time, buffered, the intro / recap / credits marks, the progress, the knob, the pointer's time and the seek pill */
  if (P.barA.v > 0.001f && live_title()) { gfx_opacity_push(P.barA.v); gfx_offset_push(0, P.barY.v); /* a channel: no time and no bar, LIVE alone (the Channels button wears the yellow key's colour) */
    const char *lt = T("LIVE"); float lw = text_width(lt, 22, 800, 1) + 44, lh = 10 + text_line_height(22), ly = barTop + (L25 - lh) / 2, pu = 0.5f + 0.5f * sinf((float)now_() * 3.0f);
    gfx_rrect(bx, ly, lw, lh, lh / 2, RGB(0xe0, 0x2b, 0x2b)); gfx_circle(bx + 16, ly + lh / 2, 4.5f + 3 * pu, WHITE_A((int)(80 * (1 - pu)))); gfx_circle(bx + 16, ly + lh / 2, 4.5f, WHITE_A(255)); text_draw(lt, bx + 28, ly + 5, 22, 800, WHITE_A(255), 1, 0, ALIGN_LEFT);
    gfx_offset_pop(); gfx_opacity_pop(); }
  else if (P.barA.v > 0.001f) { gfx_opacity_push(P.barA.v); gfx_offset_push(0, P.barY.v); double d = dur(); double t = P.seekT >= 0 ? P.seekT : video_time(); float p = d > 0 ? (float)(t / d) : 0; if (p < 0) p = 0; if (p > 1) p = 1;
    char c1[16], c2[18]; fmt_time(t, c1, sizeof c1); char r[16]; fmt_time(d - t, r, sizeof r); snprintf(c2, sizeof c2, "-%s", r); int f = P.mode == 1 && P.row == 1, sk = P.skOn; float th = f || sk ? 10 : 6;
    float tx = bx + 132 + 20, tw = W - 2 * bx - 2 * (132 + 20), ty = trkC - th / 2;
    int lv = live_bcast(); if (lv) p = 1; /* live: the bar full, the LIVE badge where the time left was */
    text_draw(c1, bx, barTop, 25, 600, WHITE_A(255), 0, 0, ALIGN_LEFT);
    if (lv) { const char *lt = T("LIVE"); float lw = text_width(lt, 22, 800, 1) + 44, lh = 10 + text_line_height(22), lx = W - bx - lw, ly = barTop + (L25 - lh) / 2;
      gfx_rrect(lx, ly, lw, lh, lh / 2, RGB(0xe0, 0x2b, 0x2b)); gfx_circle(lx + 16, ly + lh / 2, 4.5f, WHITE_A(255)); text_draw(lt, lx + 28, ly + 5, 22, 800, WHITE_A(255), 1, 0, ALIGN_LEFT); }
    else text_draw(c2, W - bx, barTop, 25, 600, WHITE_A(184), 0, 0, ALIGN_RIGHT);
    int chs = chapters_on() && !lv, nseg = chs ? P.nchap : 1, kc = chs ? chap_at(t) : 0;
    for (int sg = 0; sg < nseg; sg++) { /* chapters: the line in pieces, a small gap at each start; the piece being seeked through a little brighter */
      float s0 = sg && chs ? tx + tw * (float)(P.chap[sg].s / d) + 2 : tx - 1, s1 = chs && sg + 1 < nseg ? tx + tw * (float)(P.chap[sg + 1].s / d) - 2 : tx + tw + 1; if (s1 - s0 < 1) continue;
      if (chs) gfx_clip_push(s0, ty - 4, s1 - s0, th + 8);
      gfx_rrect(tx, ty, tw, th, 3, WHITE_A(51)); float be = d > 0 ? (float)(video_buffered_end() / d) : 0; if (be > p) gfx_rrect(tx, ty, tw * (be > 1 ? 1 : be), th, 3, WHITE_A(66));
      if (chs && sg == kc && (f || sk)) gfx_rrect(tx, ty, tw, th, 3, WHITE_A(34));
      if (P.skipsOk && d > 0) { struct { int has; double s, e; } segs[3] = { { P.intro.has, P.intro.s, P.intro.e }, { P.recap.has, P.recap.s, P.recap.e }, { P.outro.has, P.outro.s, P.outro.e } }; for (int i = 0; i < 3; i++) if (segs[i].has) { float a = (float)(segs[i].s / d), w = (float)((segs[i].e - segs[i].s) / d); if (w < 0.004f) w = 0.004f; gfx_rrect(tx + tw * a, ty, tw * w, th, 3, WHITE_A(128)); } }
      gfx_rrect_h(tx, ty, tw * p, th, 3, RGB(0xc8, 0x10, 0x24), RGB(0xe5, 0x1a, 0x2e)); /* etapa-86: the progress in the Library's red */
      if (chs) gfx_clip_pop(); }
    float kx = tx + roundf(tw * p);
    if (P.knA.v > 0.001f && !lv) { gfx_opacity_push(P.knA.v); if (f || sk) gfx_circle(kx, trkC, 15, WHITE_A(255)); else gfx_circle(kx, trkC, 9, WHITE_A(255)); gfx_opacity_pop(); }
    if (P.hvA.v > 0.001f && d > 0) { char ht[16]; fmt_time(P.hvP * d, ht, sizeof ht); float bw = text_width(ht, 24, 700, 0) + 28, bh = 14 + text_line_height(24), bx2 = tx + roundf(tw * P.hvP) - bw / 2, by2 = ty + th - 26 - bh;
      gfx_opacity_push(P.hvA.v); gfx_rrect(bx2, by2, bw, bh, 12, RGBA(14, 12, 22, 219)); gfx_ring(bx2, by2, bw, bh, 12, 1, WHITE_A(36)); text_draw(ht, bx2 + 14, by2 + 7, 24, 700, WHITE_A(255), 0, 0, ALIGN_LEFT); gfx_opacity_pop();
      if (P.seekT < 0) prev_pic(P.hvP * d, bx2 + bw / 2, by2, P.hvA.v); } /* (its picture over it) */
    if (P.skA.v > 0.001f && P.seekTxt[0]) { /* the pill rises and grows from the knob; the number rolls the way it moved */
      int chs2 = chapters_on() && !lv; char cb[64]; const char *cn = ""; float cw = 0, L20 = text_line_height(20);
      if (chs2) { int ci = chap_at(t); if (ci != P.chLast) { P.chLast = ci; P.chAt = now_(); } cn = chap_name(ci, cb, sizeof cb); cw = text_width(cn, 20, 600, 0.2f); if (cw > 520) cw = 520; } /* the chapter, faint, over the time */
      float pw = roll_layout(P.seekTxt, P.seekDir) + 32, ph = 12 + L25, pb = ty + th - 34, s = .88f + .12f * P.skS.v; if (chs2) { if (cw + 36 > pw) pw = cw + 36; ph += L20 + 2; }
      float rr = chs2 ? 18 : 20;
      gfx_opacity_push(P.skA.v * (1 - seekK.v)); gfx_scale_push(s, s, kx, pb); gfx_offset_push(0, 10 * (1 - P.skS.v)); /* (previews showing: the pill steps back, the picture tells the time) */
      gfx_rrect(kx - pw / 2, pb - ph, pw, ph, rr, RGBA(18, 15, 26, 199)); gfx_ring(kx - pw / 2, pb - ph, pw, ph, rr, 1, WHITE_A(31));
      gfx_clip_push(kx - pw / 2, pb - ph, pw, ph);
      if (chs2) { float ca = fminf(1, (float)((now_() - P.chAt) / 0.18)), ce = ca * ca * (3 - 2 * ca); /* a new chapter: its name fades up into place */
        text_draw(cn, kx, pb - ph + 7 + 4 * (1 - ce), 20, 600, WHITE_A((int)(150 * ce)), 0.2f, pw - 36, ALIGN_CENTER); roll_draw(kx, pb - ph + 7 + L20 + 2, L25, WHITE_A(240)); }
      else roll_draw(kx, pb - ph + 6, L25, WHITE_A(240));
      gfx_clip_pop();
      gfx_offset_pop(); gfx_scale_pop(); gfx_opacity_pop();
      } /* (with previews, the pictures in the middle take over: drawn after the controls) */
    gfx_offset_pop(); gfx_opacity_pop(); }
  /* the buttons: one accent pill slides under the focused one and its name fades in */
  if (P.btnA.v > 0.001f) { gfx_opacity_push(P.btnA.v); gfx_offset_push(nudge_x(&P.nudge) * (P.mode == 1 && P.row == 0), P.btnY.v);
    int fi = P.mode == 1 && P.row == 0 ? P.bIdx : -1; if (P.mode == 2 && P.tray.kind) { int ak = P.tray.kind < 0 ? -P.tray.kind : P.tray.kind; fi = btn_index(ak == 1 || ak == 5 ? "eps" : ak == 6 ? "chn" : ak == 2 ? "src" : ak == 3 ? "aud" : "subs"); }
    float fa = P.focA.v * fminf(1, kf(P.focFadeAt, .24f, C_EASE, 0));
    { static float sv, lx; lglass_stretch(&sv, &lx, P.focX.v, app_dt()); /* the lit button: accent glass that stretches as it glides and settles round */
      if (fa > 0.001f) { float w0 = P.focW.v, h0 = P.focH.v, sw = w0 * (1 + 0.16f * sv), sh = h0 * (1 - 0.08f * sv), px = P.focX.v + w0 / 2 - sw / 2, py = P.focY.v + h0 / 2 - sh / 2; gfx_shadow(px, py + 4, sw, sh, sh / 2, 26, RGBA(0xe5, 0x1a, 0x2e, (int)(120 * fa))); gfx_rrect(px, py, sw, sh, sh / 2, RGBA(0xe5, 0x1a, 0x2e, (int)(255 * fa))); /* etapa-86: red, not the glass */
      } }
    if (P.tipA.v > 0.001f && P.tip[0]) { float ta = P.tipA.v * (P.tipAt && now_() - P.tipAt < .08 ? 0 : fminf(1, kf(P.tipAt, .3f, C_EASE, .08f)));
      int cyc = fi >= 0 && !strcmp(P.btns[fi].id, "asp") && !strcmp(P.tip, P.btns[fi].lb); /* a button that cycles: one dot per choice under its name, the lit one gliding on each press */
      if (cyc) { /* the name and its dots centred together in the pill's open part, whatever the name's length */
        float cx = P.focX.v + 70 + (P.focW.v - 70 - 24) / 2; int ai = !strcmp(P.aspect, "fill") ? 1 : !strcmp(P.aspect, "zoom") ? 2 : 0;
        text_draw(P.tip, cx, P.focY.v + 38 - text_line_height(24) / 2.0f - 8, 24, 700, WHITE_A((int)(255 * ta)), 0, 0, ALIGN_CENTER);
        cycle_dots(0, 3, ai, cx, P.focY.v + 38 + text_line_height(24) / 2.0f + 2, 11, 2.5f, WHITE_A((int)(77 * ta)), WHITE_A((int)(255 * ta)), 0); }
      else { float ks = P.focH.v / 76.f; text_draw(P.tip, P.focX.v + 70 * ks, P.focY.v + 38 * ks - text_line_height(24) / 2.0f, 24, 700, WHITE_A((int)(255 * ta)), 0, 0, ALIGN_LEFT); } } /* (the big button's name: past its bigger icon) */
    for (int i = 0; i < P.nbtn; i++) { float sz = P.btns[i].w, x = P.btns[i].x, y = P.btns[i].y; int f = i == fi;
      if (!f) lglass_shell(x, y, sz, sz, sz / 2, RGBA(16, 16, 24, 90), 0.85f); /* glass at rest */
      float isz = P.btns[i].big ? 48 : 34, ix = x + sz / 2 - isz / 2, iy = y + sz / 2 - isz / 2; uint32_t ic = WHITE_A(255);
      if (!strcmp(P.btns[i].id, "cc")) { cc_icon(ix, iy, isz, ic, f ? RGB(0xe5, 0x1a, 0x2e) : RGBA(30, 30, 34, 235)); continue; }
      icon_draw(!strcmp(P.btns[i].id, "pp") ? (paused() ? "p_play" : "p_pause") : P.btns[i].icon, ix, iy, isz, ic);
      if (i >= P.nL && !f && P.btns[i].lb[0]) text_draw(P.btns[i].lb, x + sz / 2, y + sz + 6, 17, 600, WHITE_A(170), 0, sz + 40, ALIGN_CENTER); } /* etapa-87: each one's name under it */
    gfx_offset_pop(); gfx_opacity_pop(); }
  /* the prompts: the skip pill, the credits buttons, the next-up card */
  PG.prompt = 0; const PPrompt *pr = P.prompt.k ? &P.prompt : &P.shown;
  if (P.prA.v > 0.001f && pr->k) { float a = P.prA.v; gfx_opacity_push(a); gfx_offset_push(nudge_x(&P.nudge) * (P.mode == 3), P.prY.v); float pb = P.prB.v, rm = 72 + 40 * fminf(1, fmaxf(0, (440 - pb) / 336)); /* off the edges: further in alone, in line with the buttons when they show */ PG.prompt = a > 0.5f && P.prompt.k; PG.card = 0; PG.npr = 0;
    if (pr->k == 3 && (P.next.has || P.coll.has)) { int nxC = !P.next.has; const char *nxOv = nxC ? "" : P.next.ov, *nxSe = nxC ? P.coll.part : P.next.se, *nxTi = nxC ? P.coll.name : P.next.title, *nxTh = nxC ? P.coll.bg : P.next.thumb, *nxLab = nxC ? T("Next in the collection") : T("Next episode"), *nxKey = nxC ? T("Next film") : T("Next episode"); /* (a film in a collection: the next one) */ int cmp = !strcmp(store_get("nextCard", "large"), "compact"); /* Settings > Playback: a smaller card, without the episode's summary */
      const char *th = nxTh[0] ? nxTh : P.bg; NextCardSpec nc = { nxLab, nxKey, nxSe, nxTi, nxOv, th, cmp ? 1.f : 0.f, -1, 0 };
      if (pr->autoN) { nc.autoK = fill_ease((now_() - pr->t0) / 10); nc.autoLeft = (int)(10 - (now_() - pr->t0) + 0.999); } /* (counting down to it: the bar under its picture, the seconds beside its name) */
      float cx, cy, cw, ch; ui_next_card(W - rm, H - pb, 1, &nc, &cx, &cy, &cw, &ch); /* ui/prompts.c: the same drawing Settings shows small */
      PG.card = 1; PG.prX[0] = cx; PG.prY[0] = cy; PG.prW[0] = cw; PG.prH[0] = ch; }

    else { float x = W - rm; int focus = P.mode == 3 || (P.mode == 0 && !pr->big); for (int i = pr->nbtn - 1; i >= 0; i--) { const char *ic = !strcmp(pr->b[i].id, "next") ? "p_next" : !strcmp(pr->b[i].id, "skip") ? "p_skip" : NULL; float w = ui_pill_width(1, pr->b[i].lb, ic), h = ui_pill_height(1); x -= w; float y = H - pb - h; int f = focus && i == pr->idx;
        int cnt = (pr->k == 1 || pr->k == 2) && pr->autoN; /* skip intro / recap counting down: the accent fills it from the left behind the words, then it skips */
        ui_pill(x, y, w, 1, pr->b[i].lb, ic, f && !cnt, cnt ? fill_ease((now_() - pr->t0) / pr->adur) : -1, pr->autoN && !strcmp(pr->b[i].id, "next") ? fill_ease((now_() - pr->t0) / 10) : -1); /* ui/prompts.c: glass, lit in the accent when focused; counting down, the accent growing from its left end */
        if (i < 2) { PG.prX[i] = x; PG.prY[i] = y; PG.prW[i] = w; PG.prH[i] = h; if (PG.npr < i + 1) PG.npr = i + 1; }
        x -= 16; } }
    gfx_offset_pop(); gfx_opacity_pop(); }
  dly_draw(); pos_draw(); if (P.mode != 6) live_card_draw(); if (hub_player_mode()) hub_draw(); /* (the hub opened from Home is the page's: never drawn again over the picture) */ lglass_light(0);
}
/* a countdown's fill: eases in and out (sine), so it starts and lands softly */
static float fill_ease(double t) { if (t <= 0) return 0; if (t >= 1) return 1; return (float)(0.5 - 0.5 * cos(t * 3.14159265358979)); }
/* ---- the tray: sized by what it shows (never more than 60 % of the width), glass, over the buttons' side ---- */
static float item_w(TItem *it, int ak) {
  if (it->ww > 0) return it->ww; /* measured on the first frame it shows */
  if (it->k == 0) return it->ww = 20 + text_width(it->lb, 24, 400, 0) + (it->spin ? 34 : 0);
  if (it->k == 2) return 336; if (it->k == 3) return 400;
  float tw = text_width(it->lb, 27, 700, 0), sw = it->sub[0] ? text_width(it->sub, 23, 600, 0) : 0; /* (a chip's two lines: the wider one) */
  float w = 20 + (it->tool ? 0 : 36) + (tw > sw ? tw : sw) + (it->vl[0] ? 16 + text_width(it->vl, 22, it->vwt ? it->vwt : 700, 0) : 0) + (it->sw ? 14 + 22 : 0) + 28; (void)ak;
  return it->ww = w < 240 ? 240 : w;
}
/* the "Playing" tag, upper-cased and measured once per language */
static const char *playing_tag(float *nw) { static char now[32]; static float w; static unsigned lg; if (lg != i18n_gen()) { lg = i18n_gen(); snprintf(now, sizeof now, "%s", T("Playing")); text_upper(now, sizeof now); w = 26 + text_width(now, 20, 800, 1); } *nw = w; return now; }
static float item_h(TItem *it) {
  if (it->k == 0) return 36 + text_line_height(24);
  if (it->k == 3) return 196;
  if (it->k == 2) { if (!it->hh) { int n = it->ov[0] ? text_wrap_count(it->ov, 300, 2, 22, 400, 0) : 0; it->hh = 189 + 14 + 30 + (it->sub[0] ? 4 + text_line_height(22) : 0) + (n ? 8 + n * 30 : 0) + 16; } return it->hh; }
  return 22 + text_line_height(27) + 2 + text_line_height(23) + 20; /* a chip: every one as tall as one with two lines */
}
static void draw_card(TItem *it, float x, float y, float w, float h, int f) {
  int on = it->on; uint32_t base = f ? ((it->k == 2 || (it->k == 3 && on) || it->k == 4) ? WHITE_A(18) : 0) : WHITE_A(11); /* rows: .045; focused: the plate shows through (.07 on some kinds) */
  if (it->k == 0) { float tx = x + 10; if (it->spin) { float cx = x + 10 + 11, cy = y + 18 + 13; gfx_ring(cx - 11, cy - 11, 22, 22, 11, 3, WHITE_A(51)); float ph = clock_ph(0.9) * 6.2832f; for (int i = 0; i <= 8; i++) { float an = ph - 2.356f + 1.5708f * i / 8; gfx_circle(cx + cosf(an) * 9.5f, cy + sinf(an) * 9.5f, 1.5f, ACC2); } tx += 34; } text_draw(it->lb, tx, y + 18, 24, 400, WHITE_A(184), 0, 0, ALIGN_LEFT); return; }
  if (base) gfx_rrect(x, y, w, h, 20, base); if (it->k == 3 && on && !f) gfx_ring(x, y, w, h, 20, 2, acc_a(0.7f));
  if (it->k == 2 && it->hid && (spoil_blurable(it->th) || spoil_blurable(P.bg))) { float th = 189; gfx_clip_push(x, y, w, th); spoil_picture(spoil_blurable(it->th) ? it->th : P.bg, x, y, w, th + 20, 20); /* (no still of its own: the show's backdrop blurred) */ gfx_clip_pop(); spoil_mark(x + w / 2, y + th / 2, 54, 0, 0); /* not watched yet: its still soft and dark, its words kept back when asked */
    float ty = y + th + 14; text_draw(it->lb, x + 18, ty + (30 - text_line_height(24)) / 2, 24, 700, WHITE_A(255), 0, w - 36, ALIGN_LEFT); ty += 30; if (it->sub[0]) { text_draw(it->sub, x + 18, ty + 4, 22, 600, WHITE_A(184), 0, w - 36, ALIGN_LEFT); ty += 4 + text_line_height(22); }
    if (it->ov[0] && spoil_mode() < 2) text_draw_wrapped(it->ov, x + 18, ty + 8 + (30 - text_line_height(22)) / 2, w - 36, 2, 22, 400, WHITE_A(191), 0, ALIGN_LEFT, 30); return; }
  if (it->k == 2) { float th = 189; GLuint t = it->th[0] ? img_get_shaped(it->th, (int)w, (int)th + 20, 20, 1) : 0; int noStill = !t && (!it->th[0] || img_shaped_failed(it->th, (int)w, (int)th + 20, 20, 1));
    int upBlur = noStill && it->v && ep_unaired(it->v) && spoil_blurable(P.bg); /* not aired, no still: the show's backdrop blurred, Upcoming on it (as the title page) */
    if (noStill && !upBlur && P.bg[0]) t = img_get_shaped(P.bg, (int)w, (int)th + 20, 20, 1);
    gfx_clip_push(x, y, w, th); if (upBlur) { spoil_picture(P.bg, x, y, w, th + 20, 20); spoil_mark(x + w / 2, y + th / 2, 54, 0, 0); } else ph_picture(t, x, y, w, th + 20, 20, WHITE_A(255)); /* (the eye as on hidden ones) */
    if (upBlur) { static char ut[64]; static float uw; static unsigned lg; if (lg != i18n_gen() || !ut[0]) { lg = i18n_gen(); snprintf(ut, sizeof ut, "%s", T("Upcoming")); uw = text_width(ut, 20, 600, 0.2f) + 46; }
      float uh = 12 + text_line_height(20), ux = x + 12, uy = y + 12; gfx_rrect(ux, uy, uw, uh, uh / 2, RGBA(14, 12, 20, 178)); gfx_rrect_v(ux, uy, uw, uh, uh / 2, WHITE_A(26), WHITE_A(6)); gfx_ring(ux, uy, uw, uh, uh / 2, 1, WHITE_A(46));
      gfx_circle(ux + 19, uy + uh / 2, 4.5f, ACC2); text_draw(ut, ux + 32, uy + 6, 20, 600, WHITE_A(245), 0.2f, 0, ALIGN_LEFT); } /* the title page's tag, a size down */ if (it->v && !t) spoil_reveal_wait(js_str(it->v, "id", "")); if (it->v) spoil_reveal_over(js_str(it->v, "id", ""), it->th, x, y, w, th + 20, 20, x + w / 2, y + th / 2, 54); gfx_clip_pop(); /* rounded at the top only: the lower corners are cut square (just shown clear: its blur fading off) */
    if (on) { float nw; const char *now = playing_tag(&nw); gfx_rrect(x + 12, y + 12, nw, 8 + text_line_height(20), 11, ACC); text_draw(now, x + 12 + 13, y + 12 + 4, 20, 800, ACC_FG, 1, 0, ALIGN_LEFT); }
    float ty = y + th + 14; text_draw(it->lb, x + 18, ty + (30 - text_line_height(24)) / 2, 24, 700, WHITE_A(255), 0, w - 36, ALIGN_LEFT); ty += 30; if (it->sub[0]) { text_draw(it->sub, x + 18, ty + 4, 22, 600, WHITE_A(184), 0, w - 36, ALIGN_LEFT); ty += 4 + text_line_height(22); }
    if (it->ov[0]) text_draw_wrapped(it->ov, x + 18, ty + 8 + (30 - text_line_height(22)) / 2, w - 36, 2, 22, 400, WHITE_A(191), 0, ALIGN_LEFT, 30); return; }
  if (it->k == 3) { gfx_clip_push(x, y, w, h); int n = text_draw_wrapped(it->lb, x + 22, y + 20, w - 44, 2, 24, 700, WHITE_A(255), 0, ALIGN_LEFT, 24 * 1.25f); if (it->ov[0]) text_draw_wrapped(it->ov, x + 22, y + 20 + n * 24 * 1.25f + 8, w - 44, 3, 22, 400, WHITE_A(191), 0, ALIGN_LEFT, 30);
    if (on) { float nw; const char *now = playing_tag(&nw); gfx_rrect(x + w - 16 - nw, y + h - 14 - 8 - text_line_height(20), nw, 8 + text_line_height(20), 11, ACC); text_draw(now, x + w - 16 - nw + 13, y + h - 14 - 4 - text_line_height(20), 20, 800, ACC_FG, 1, 0, ALIGN_LEFT); } gfx_clip_pop(); return; }
  /* a chip (audio): tick, name, its language, value */
  float tx = x + 20; if (!it->tool) { if (on) icon_draw("check", tx + 1, y + h / 2 - 11, 22, col_alpha(ACC2, TM.rowChk.v)); tx += 24 + 12; }
  float right = x + w - 26; if (it->vl[0]) { if (it->vw <= 0) it->vw = text_width(it->vl, 22, it->vwt ? it->vwt : 700, 0); float vw = it->vw; text_draw(it->vl, right, y + h / 2 - text_line_height(22) / 2.0f, 22, it->vwt ? it->vwt : 700, ACC3, 0, 0, ALIGN_RIGHT); right -= vw + 16; }
  if (it->sw) { gfx_circle(right - 11, y + h / 2, 11, it->sw); gfx_ring(right - 22, y + h / 2 - 11, 22, 22, 11, 2, BLACK_A(77)); right -= 22 + 14; }
  float L1 = text_line_height(27), L2 = text_line_height(23);
  if (it->sub[0]) { float ty = y + (h - L1 - 2 - L2) / 2; text_draw(it->lb, tx, ty, 27, 700, WHITE_A(255), 0, right - tx - 4, ALIGN_LEFT); text_draw(it->sub, tx, ty + L1 + 2, 23, 600, WHITE_A(173), 0, right - tx - 4, ALIGN_LEFT); }
  else text_draw(it->lb, tx, y + h / 2 - L1 / 2, 27, 700, WHITE_A(255), 0, right - tx - 4, ALIGN_LEFT);
}
/* the sources: an addon's header (its name, how many, or the spinner while it is asked; plus / minus folds it), its rows
   as tall as their words need (up to 3 lines of name, 8 of words, then an ellipsis), the lit plate gliding under the
   focus; an addon whose header comes into view is asked then */
static void draw_srcs(float ix, float top, float W) {
  (void)W; float RW = SRC_RW, vx = ix + 30, vy = top + 8, vh = SRC_VH; int fi = !P.tray.onBtn && P.tray.kind > 0 && P.tray.zone == 0 && src_focusable(SR.idx) ? SR.idx : -1;
  float want = 0; if (src_focusable(SR.idx)) { want = SR.r[SR.idx].y - 120; float mx = SR.tot - 10 - vh; if (want > mx) want = mx; if (want < 0) want = 0; } mo_to(&SR.y, want, .28f, C_OUT, 0);
  PG.src = 1; PG.srcX = vx; PG.srcTop = vy;
  if (fi >= 0) { SRow *r = &SR.r[fi]; if (SR.plA.v <= 0.001f && !SR.plA.on) { mo_set(&SR.plY, r->y); mo_set(&SR.plH, r->h); } else { mo_to(&SR.plY, r->y, .26f, C_OUT, 0); mo_to(&SR.plH, r->h, .26f, C_OUT, 0); } mo_to(&SR.plA, 1, .18f, C_EASE, 0); }
  else mo_to(&SR.plA, 0, .18f, C_EASE, 0);
  gfx_clip_push(ix, vy, RW + 60, vh); gfx_offset_push(vx, vy - SR.y.v);
  if (SR.plA.v > 0.001f) { float py = SR.plY.v, ph = SR.plH.v; gfx_opacity_push(SR.plA.v); gfx_rrect(0, py, RW, ph, 20, WHITE_A(38)); gfx_clip_push(0, py, 4, ph); gfx_rrect(0, py, RW, ph, 20, ACC2); gfx_clip_pop(); gfx_opacity_pop(); }
  const uint32_t CORAL = RGB(0xff, 0x9a, 0x7a); float LN = text_line_height(SN_PX), LD = text_line_height(SD_PX);
  for (int i = 0; i < SR.n; i++) { SRow *r = &SR.r[i]; float y = r->y, h = r->h; int f = i == fi;
    if (y - SR.y.v > vh || y + h - SR.y.v < 0) continue;
    float al = (r->k == 3 || r->k == 1) && r->g >= 0 ? P.srcs.g[r->g].ia.v : 1; if (al <= 0.004f) continue; gfx_opacity_push(al);
    if (r->k == 5) { /* an addon's name over its streams, and how many */
      char up[120]; snprintf(up, sizeof up, "%s", P.srcs.g[r->g].name); text_upper(up, sizeof up); float ty = y + (h - text_line_height(20)) / 2;
      text_draw(up, 22, ty, 20, 800, WHITE_A(160), 1.6f, RW - 160, ALIGN_LEFT);
      if (P.srcs.g[r->g].st == 2) { char n[12]; snprintf(n, sizeof n, "%d", src_count(r->g)); text_draw(n, RW - 22, ty, 20, 700, WHITE_A(120), 0, 0, ALIGN_RIGHT); } }
    else if (r->k == 1) { gfx_rrect(0, y, RW, h, 20, WHITE_A(9)); ph_text(22, y + 16, (RW - 44) * (r->i ? 0.34f : 0.46f), 1, SN_PX, SN_LH, ALIGN_LEFT); ph_text(22, y + 16 + SN_LH + 4, (RW - 44) * (r->i ? 0.62f : 0.78f), 1, SD_PX, SD_LH, ALIGN_LEFT); }
    else if (r->k == 0) text_draw(T("No streams found."), 22, y + (h - text_line_height(25)) / 2, 25, 400, WHITE_A(191), 0, RW - 44, ALIGN_LEFT);
    else { /* a stream: its facts in the accent and what it is on the right; its release under it; what matters here in coral */
      const Stream *st = &P.srcs.g[r->g].items[r->i]; const BestFacts *bf = &P.srcs.g[r->g].f[r->i]; int on = r->g == srcPG && r->i == srcPI;
      if (bf->out && !f) gfx_opacity_push(0.5f);
      if (!f) gfx_rrect(0, y, RW, h, 20, WHITE_A(11)); if (on && !f && r->k != 6) gfx_ring(0, y, RW, h, 20, 2, acc_a(0.7f));
      float l1 = y + 16 + (SN_LH - LN) / 2, l2 = y + 16 + SN_LH + 4 + (SD_LH - LD) / 2, right = RW - 22;
      if (on && r->k != 6) { float nw; const char *now = playing_tag(&nw); gfx_rrect(right - nw, y + 16 + SN_LH / 2 - (8 + text_line_height(20)) / 2, nw, 8 + text_line_height(20), 11, ACC); text_draw(now, right - nw + 13, y + 16 + SN_LH / 2 - (8 + text_line_height(20)) / 2 + 4, 20, 800, ACC_FG, 1, 0, ALIGN_LEFT); right -= nw + 16; }
      const char *nm = st->name[0] ? st->name : st->addon; char first[200]; { const char *e = strchr(nm, '\n'); snprintf(first, sizeof first, "%.*s", e ? (int)(e - nm) : (int)strlen(nm), nm); }
      if (r->k == 6 || r->k == 7) { /* pinned: what it is, then its facts and size */
        const char *lb = r->k == 6 ? T("Now playing") : T("Best fit for this TV"); icon_draw("play", 22, y + 16 + (SN_LH - 26) / 2, 26, ACC3);
        text_draw(lb, 22 + 36, l1, SN_PX, 700, ACC3, 0.2f, right - 22 - 36 - 10, ALIGN_LEFT);
        char ln[300]; snprintf(ln, sizeof ln, "%s%s%s", bf->facts[0] ? bf->facts : first, bf->meta[0] ? "  \xc2\xb7  " : "", bf->meta); text_draw(ln, 22, l2, SD_PX, 500, WHITE_A(210), 0, RW - 44, ALIGN_LEFT); }
      else {
        float mw = bf->meta[0] ? text_width(bf->meta, SD_PX, 600, 0) : 0; if (mw) text_draw(bf->meta, right, l1 + (LN - LD) / 2, SD_PX, 600, WHITE_A(185), 0, 0, ALIGN_RIGHT);
        text_draw(bf->facts[0] ? bf->facts : first, 22, l1, SN_PX, 700, bf->facts[0] ? ACC3 : WHITE_A(240), 0.2f, right - 22 - (mw ? mw + 24 : 0), ALIGN_LEFT);
        char rl[300]; snprintf(rl, sizeof rl, "%s", bf->rel[0] ? bf->rel : bf->facts[0] ? first : st->addon); text_draw(rl, 22, l2, SD_PX, 400, WHITE_A(155), 0, RW - 44, ALIGN_LEFT); }
      if (bf->warn[0]) { float wy = y + 16 + SN_LH + 4 + SD_LH + 4; icon_draw("minus", 22, wy + (SD_LH - 22) / 2, 22, CORAL); text_draw(bf->warn, 22 + 32, wy + (SD_LH - text_line_height(22)) / 2, 22, 700, CORAL, 0, RW - 44 - 32, ALIGN_LEFT); }
      if (bf->out && !f) gfx_opacity_pop(); }
    gfx_opacity_pop(); }
  gfx_offset_pop(); gfx_clip_pop();
  /* more above or below: the list's edges fade */
  if (SR.y.v > 2) gfx_rrect_v(ix, vy, RW + 60, 36, 0, RGBA(18, 16, 28, 200), RGBA(18, 16, 28, 0));
  if (SR.tot - 10 - SR.y.v > vh + 2) gfx_rrect_v(ix, vy + vh - 36, RW + 60, 36, 0, RGBA(18, 16, 28, 0), RGBA(18, 16, 28, 200));
}
static float CP = 282, CPW = 232; /* the subtitle columns' pitch and their entries' width: wider when there are only two */
static float col_item_h(TItem *it) { if (it->tool && it->sub[0]) { if (it->hh <= 0) it->hh = 24 + 23 * 1.25f + 2 + 26 * text_wrap_count(it->sub, 6 + CPW - 18 - 24, 2, 21, 600, 0); return it->hh; }
  if (it->k == 0) { if (it->hh <= 0) it->hh = 24 + 28 * text_wrap_count(it->lb, CPW - 8, 4, 23, 400, 0); return it->hh; }
  return it->k == 1 ? 58 : it->tool ? 30 + 23 * 1.25f : it->sub[0] ? 24 + 24 * 1.25f + 2 + text_line_height(21) : 30 + 24 * 1.25f; }
static void draw_tray(void) {
  int ak = P.tray.kind < 0 ? -P.tray.kind : P.tray.kind; PG.tray = 0; PG.ntab = 0; PG.nrow = 0; PG.src = 0; if (!ak || P.tray.openA.v <= 0.001f) return;
  float L24 = text_line_height(24), L30 = text_line_height(30), L18 = text_line_height(21); /* (the columns' headings) */
  /* the header: tabs (an episode's sources: its name above them), a title, or nothing */
  float hdH = 0, hdW = 0, tabW[40], tabsW = 0;
  static TMemo tabM[40], ttlM;
  if (P.tray.ntabs) { for (int i = 0; i < P.tray.ntabs; i++) { tabW[i] = TAB_PAD + tm_width(&tabM[i], P.tray.tabs[i], TAB_FS, 700, 0); tabsW += tabW[i] + TAB_GAP; } hdW = tabsW; hdH = 28 + text_line_height(TAB_FS) + 22; if (ak == 5 || ak == 2) { hdH += L24 + 14; hdW = fmaxf(hdW, tm_width(&ttlM, P.tray.title, 24, 800, 0)); } }
  else if (P.tray.title[0]) { hdH = L30 + 18; hdW = tm_width(&ttlM, P.tray.title, 30, 800, 0); }
  float xs[MAXROW], ws[MAXROW], hs[MAXROW], rowH = 0, rowW = 0;
  if (ak != 4 && ak != 2 && ak != 5) for (int i = 0; i < P.tray.n; i++) { ws[i] = item_w(&P.tray.row[i], ak); hs[i] = item_h(&P.tray.row[i]); xs[i] = rowW; rowW += ws[i] + (P.tray.row[i].k == 0 ? 0 : 18); /* notes have no margin */ if (hs[i] > rowH) rowH = hs[i]; }
  int srcs = ak == 2 || ak == 5;
  if (ak == 4) { CP = P.tray.ncols && P.tray.ncols <= 2 ? 340 : 282; CPW = CP - 50; }
  float colH = 6 + (6 + L18 + 14) + COL_LH + 4, listH = ak == 4 ? colH : srcs ? SRC_VH : rowH;
  float W = ak == 4 ? 56 + (P.tray.ncols ? P.tray.ncols : 4) * CP - 10 /* the last column as far from the edge as the first */ : srcs ? SRC_W : fminf(SB_W * 0.6f, 2 + 60 + fmaxf(rowW, hdW)), H = 2 + 22 + hdH + 8 + listH + 8 + 20, x = SB_W - 72 - W, y;
  if (P.tray.ntabs && !srcs && W < tabsW - TAB_GAP + 62) W = fminf(tabsW - TAB_GAP + 62, SB_W - 144); /* the tabs in view where they fit, never wider than the screen (many seasons: the pills scroll); Sources keeps its width (many addons: the pills scroll, the list stays in view) */
  /* every tray takes the size of what is in it, and glides there when that changes (a group folded, a tab switched) */
  { static Mo tH, tW; static int tk; if (tk != ak || P.tray.openA.v < 0.02f) { mo_set(&tH, H); mo_set(&tW, W); tk = ak; } else { if (fabsf(tH.to - H) > 0.5f) mo_to(&tH, H, .26f, C_OUT, 0); if (fabsf(tW.to - W) > 0.5f) mo_to(&tW, W, .26f, C_OUT, 0); } static double lt; double nw = now_(); float fdt = lt ? (float)(nw - lt) : 0; lt = nw; if (fdt > 0.05f) fdt = 0.05f; mo_tick(&tH, fdt); mo_tick(&tW, fdt); H = tH.v; W = tW.v; }
  x = SB_W - 72 - W; y = SB_H - 178 - H;
  PG.tray = P.tray.kind > 0 && P.tray.openA.v > 0.5f; PG.x = x; PG.y = y; PG.w = W; PG.h = H;
  float sc = P.tray.open.v; gfx_opacity_push(P.tray.openA.v); gfx_scale_push(sc, sc, P.tray.ox, P.tray.oy);
  gfx_shadow(x, y + 14, W, H, 28, 56, BLACK_A(150)); /* the tray lifts off the picture */
  gfx_rrect_v(x, y, W, H, 28, RGBA(32, 28, 48, 252), RGBA(14, 12, 22, 252)); /* (nearly opaque: on some TVs the 204 let a bright picture through and the white rows vanished) */
  { gfx_clip_push(x, y, W, 96); gfx_rrect_v(x, y, W, 96, 28, WHITE_A(15), WHITE_A(0)); gfx_clip_pop(); } /* a soft sheen along the top */
  gfx_ring(x, y, W, H, 28, 1, WHITE_A(26)); gfx_ring(x + 1, y + 1, W - 2, H - 2, 27, 1, WHITE_A(9));
  gfx_offset_push(nudge_x(&P.nudge) * (P.mode == 2), nudge_y(&P.nudge) * (P.mode == 2));
  float ix = x + 1, cy = y + 1 + 22;
  if (P.tray.ntabs) { if (ak == 5 || ak == 2) { text_draw(P.tray.title, ix + 30, cy, 24, 800, WHITE_A(209), 0, W - 62, ALIGN_LEFT); cy += L24 + 14; }
    float vw = W - 2 - 60, xt[40], tx = 0; for (int i = 0; i < P.tray.ntabs; i++) { xt[i] = tx; tx += tabW[i] + TAB_GAP; }
    float want = 0; if (P.tray.tab < P.tray.ntabs) { want = xt[P.tray.tab] + tabW[P.tray.tab] / 2 - vw / 2; if (want > tabsW - TAB_GAP - vw) want = tabsW - TAB_GAP - vw; if (want < 0) want = 0; } mo_to(&P.tray.tabsX, want, .25f, C_EASE, 0); /* the chosen tab stays in view */
    float bh = 28 + text_line_height(TAB_FS); gfx_clip_push(ix + 30, cy - 4, vw, bh + 8);
    { float tw0 = tabsW - TAB_GAP - P.tray.tabsX.v; if (tw0 > vw) tw0 = vw; gfx_rrect(ix + 30 - 6, cy - 6, tw0 + 12, bh + 12, (bh + 12) / 2, WHITE_A(12)); gfx_ring(ix + 30 - 6, cy - 6, tw0 + 12, bh + 12, (bh + 12) / 2, 1, WHITE_A(18)); } /* one track for all the tabs: a segmented choice, not a row of boxes */
    /* one pill under the chosen tab, gliding from tab to tab; lit with the accent while the tabs have the focus */
    if (P.tray.tab < P.tray.ntabs) { int tf = P.tray.zone == 1 && !P.tray.onBtn && P.tray.kind > 0;
      if (!TM.tabSeen) { mo_set(&TM.tabX, xt[P.tray.tab]); mo_set(&TM.tabW, tabW[P.tray.tab]); mo_set(&TM.tabF, tf); TM.tabSeen = 1; }
      else { mo_to(&TM.tabX, xt[P.tray.tab], GLIDE_DUR, C_GLIDE, 0); mo_to(&TM.tabW, tabW[P.tray.tab], GLIDE_DUR, C_GLIDE, 0); mo_to(&TM.tabF, tf, .2f, C_EASE, 0); }
      float px = ix + 30 + TM.tabX.v - P.tray.tabsX.v, pw = TM.tabW.v, k = TM.tabF.v;
      if (k < 0.999f) { gfx_rrect(px, cy, pw, bh, bh / 2, WHITE_A((int)(56 * (1 - k)))); gfx_ring(px, cy, pw, bh, bh / 2, 2, WHITE_A((int)(150 * (1 - k)))); }
      if (k > 0.001f) { gfx_opacity_push(k); gfx_rrect(px, cy, pw, bh, bh / 2, ACC); gfx_rrect_h(px, cy, pw, bh, bh / 2, WHITE_A(0), WHITE_A(56));  gfx_opacity_pop(); } }
    float pc = ix + 30 + TM.tabX.v - P.tray.tabsX.v + TM.tabW.v / 2;
    for (int i = 0; i < P.tray.ntabs; i++) { float bx = ix + 30 + xt[i] - P.tray.tabsX.v;
      PG.tabX[i] = bx; PG.tabW[i] = tabW[i]; PG.tabY = cy; PG.tabH = bh; PG.ntab = i + 1;
      float u = 1 - fabsf(pc - (bx + tabW[i] / 2)) / tabW[i]; if (u < 0) u = 0; /* the words under the pill take its colours as it passes */
      text_draw(P.tray.tabs[i], bx + tabW[i] / 2, cy + 14, TAB_FS, 700, col_mix(WHITE_A(224), col_mix(WHITE_A(255), ACC_FG, TM.tabF.v), u), 0, 0, ALIGN_CENTER); }
    gfx_clip_pop();
    /* more pills to one side: that edge fades, as the list's do */
    if (P.tray.tabsX.v > 2) gfx_rrect_h(ix + 30, cy - 4, 48, bh + 8, 0, RGBA(18, 16, 28, 200), RGBA(18, 16, 28, 0));
    if (tabsW - TAB_GAP - P.tray.tabsX.v > vw + 2) gfx_rrect_h(ix + 30 + vw - 48, cy - 4, 48, bh + 8, 0, RGBA(18, 16, 28, 0), RGBA(18, 16, 28, 200));
    cy += bh + 22; }
  else if (P.tray.title[0]) { text_draw(P.tray.title, ix + 30, cy, 30, 800, WHITE_A(255), 0, W - 62, ALIGN_LEFT); cy += L30 + 18; }
  float lstTop = cy;
  /* changing tabs: what was shown slides out the other way and fades, the new one comes in from the side of the move */
  float swa = TM.swA.v, swx = TM.swPend ? -TM.swDir * 28 * (1 - swa) : TM.swDir * 28 * (1 - swa); int sw = swa < 0.999f;
  if (sw) { gfx_opacity_push(swa); gfx_offset_push(swx, 0); }
  if (ak == 4) { /* subtitles: columns side by side (the page's last one runs a little past the tray's edge) */
    gfx_clip_push(ix, lstTop, W - 2, listH + 16);
    /* the focused column's backdrop glides from column to column */
    { int on = P.tray.zone == 2 && P.tray.c < P.tray.ncols; float tx = ix + 22 + 5 + P.tray.c * CP;
      if (on && (!TM.colBSeen || TM.colBA.v <= 0.001f)) { mo_set(&TM.colBX, tx); TM.colBSeen = 1; } else if (on) mo_to(&TM.colBX, tx, GLIDE_DUR, C_GLIDE, 0);
      mo_to(&TM.colBA, on, .2f, C_EASE, 0);
      if (TM.colBA.v > 0.001f) gfx_rrect(TM.colBX.v, lstTop + 8, CP - 10, colH, 22, WHITE_A((int)(10 * TM.colBA.v + .5f))); }
    for (int c = 0; c < P.tray.ncols; c++) { TCol *col = &P.tray.cols[c]; int cur = c == P.tray.c && P.tray.zone == 2; float colX = ix + 22 + 5 + c * CP, colTop = lstTop + 8;
      float near = 1 - fabsf(TM.colBX.v - colX) / CP; if (near < 0) near = 0; near *= TM.colBA.v;
      static UMemo colU[4][2]; const char *h2 = um_upper(&colU[c][0], col->title); text_draw(h2, colX + 18, colTop + 12, 21, 800, col_mix(WHITE_A(184), ACC2, near), 1.8f, CP - 46, ALIGN_LEFT);
      if (col->vl[0]) { const char *v2 = um_upper(&colU[c][1], col->vl); float va = TM.vkC == c && !strcmp(TM.vkU, "dly") ? TM.vkA.v : 1; text_draw(v2, colX + CP - 28, colTop + 12 + 6 * (1 - va), 21, 800, WHITE_A((int)(255 * va)), 0.5f, 0, ALIGN_RIGHT); }
      float clTop = colTop + 6 + 6 + L18 + 14, cinX = colX + 10 + 4, cinY = clTop + 2; int cn = col->n < 300 ? col->n : 300; float iy = 0, ys[300], hh[300];
      for (int i = 0; i < cn; i++) { ys[i] = iy; hh[i] = col_item_h(&col->items[i]); iy += hh[i] + (col->items[i].k == 0 ? 0 : 10); }
      int fi = P.tray.r[c]; if (fi >= cn) fi = -1; float want = 0; if (fi >= 0 && iy > COL_LH - 4) { want = ys[fi] - COL_LH / 2 + hh[fi] / 2; if (want > iy - (COL_LH - 4)) want = iy - (COL_LH - 4); if (want < 0) want = 0; } mo_to(&P.tray.colY[c], -want, .25f, C_EASE, 0);
      PG.colX[c] = cinX + 6; PG.colW = CPW; PG.colTop = clTop; PG.colBot = clTop + COL_LH; PG.nit[c] = cn; for (int i = 0; i < cn; i++) { PG.itY[c][i] = cinY + P.tray.colY[c].v + ys[i]; PG.itH[c][i] = hh[i]; }
      gfx_clip_push(colX + 10, clTop, CP - 30, COL_LH); gfx_offset_push(cinX, cinY + P.tray.colY[c].v);
      int lfa = TM.colA[c].v < 0.999f; if (lfa) { gfx_opacity_push(TM.colA[c].v); gfx_offset_push(0, 10 * (1 - TM.colA[c].v)); } /* what it lists changed: it fades in */
      /* the column's plate glides to its focused item; moving across columns it fades out here and in there */
      int pon = cur && fi >= 0 && !P.tray.onBtn && col->items[fi].k != 0;
      if (pon) { if (P.tray.plA[c].v <= 0.001f && !P.tray.plA[c].on) mo_set(&P.tray.plY[c], ys[fi]); else mo_to(&P.tray.plY[c], ys[fi], .24f, C_OUT, 0); mo_to(&P.tray.plA[c], 1, .2f, C_EASE, 0); } else mo_to(&P.tray.plA[c], 0, .2f, C_EASE, 0);
      if (P.tray.plA[c].v > 0.001f && fi >= 0) { float ph = hh[fi]; gfx_opacity_push(P.tray.plA[c].v); gfx_rrect(6, P.tray.plY[c].v, CPW, ph, 20, WHITE_A(41)); gfx_clip_push(6, P.tray.plY[c].v, 4, ph); gfx_rrect(6, P.tray.plY[c].v, CPW, ph, 20, ACC2); gfx_clip_pop(); gfx_opacity_pop(); }
      for (int i = 0; i < cn; i++) { if (ys[i] + P.tray.colY[c].v > COL_LH || ys[i] + hh[i] + P.tray.colY[c].v < 0) continue; /* only what shows */ TItem *it = &col->items[i]; int f = cur && i == fi && !P.tray.onBtn; float h = hh[i], yy = ys[i];
        if (it->k == 0) { text_draw_wrapped(it->lb, 14, yy + 12, CPW - 8, 4, 23, 400, WHITE_A(184), 0, ALIGN_LEFT, 28); continue; } /* (a note: on up to four lines) */
        if (it->k == 1) { ph_box(6, yy, CPW, 58, 20); continue; } /* still being found: the waiting look */
        if (it->grey) gfx_opacity_push(0.38f); /* (what these subtitles can't take) */
        if (!f) gfx_rrect(6, yy, CPW, h, 20, WHITE_A(18));
        float tx = 6 + (it->tool ? 18 : 14); if (!it->tool) { if (it->spin) { float cx = tx + 12, cy = yy + h / 2; gfx_ring(cx - 11, cy - 11, 22, 22, 11, 3, WHITE_A(51)); float ph = clock_ph(0.9) * 6.2832f; for (int q = 0; q <= 8; q++) { float an = ph - 2.356f + 1.5708f * q / 8; gfx_circle(cx + cosf(an) * 9.5f, cy + sinf(an) * 9.5f, 1.5f, ACC2); } } /* chosen, its lines still loading: the notes' spinner in place of the tick */
          else if (item_on(it)) icon_draw("check", tx + 1, yy + h / 2 - 11, 22, col_alpha(ACC2, i == TM.onIdx[c] ? TM.chkA[c].v : 1)); tx += 24 + 12; }
        /* a value that cycles: its dots under it, the lit one gliding along; a value just changed fades up */
        int dk = !it->tool ? 0 : !strcmp(it->u, "size") ? 1 : !strcmp(it->u, "color") ? 2 : !strcmp(it->u, "bg") ? 3 : 0; float va = TM.vkC == c && it->tool && !strcmp(TM.vkU, it->u) ? TM.vkA.v : 1, vy0 = dk ? -5 : 0;
        float right = 6 + CPW - 18; if (dk == 2 || dk == 3) { swatch_draw(dk - 2, dk == 2 ? (color_idx() < 0 ? 0 : color_idx()) : bg_idx(), right - 13, yy + h / 2, 13); right -= 26 + 10; }
        if (it->vl[0]) { if (it->vw <= 0) it->vw = text_width(it->vl, 22, it->vwt ? it->vwt : 700, 0); float vw = it->vw; text_draw(it->vl, right, yy + h / 2 - text_line_height(22) / 2.0f + vy0 + 6 * (1 - va), 22, it->vwt ? it->vwt : 700, col_alpha(ACC3, va), 0, 0, ALIGN_RIGHT);
          if (dk == 1) cycle_dots(dk, 7, size_idx(), right, yy + h / 2 + 14, 9, 2.5f, WHITE_A(64), ACC3, 1);
          right -= vw + 10; } if (it->sw) { gfx_circle(right - 9, yy + h / 2, 9, it->sw); gfx_ring(right - 18, yy + h / 2 - 9, 18, 18, 9, 2, BLACK_A(77)); right -= 18 + 10; }
        int px = it->tool ? 23 : 24;
        if (it->sub[0] && it->tool) { float ly = yy + 12; text_draw(it->lb, tx, ly, px, 700, WHITE_A(255), 0, right - tx - 4, ALIGN_LEFT); text_draw_wrapped(it->sub, tx, ly + px * 1.25f + 2, 6 + CPW - 18 - tx, 2, 21, 600, WHITE_A(184), 0, ALIGN_LEFT, 26); } /* Auto-sync: what it did, on up to two lines */
        else if (it->sub[0]) { float ly = yy + 12; text_draw(it->lb, tx, ly, px, 700, WHITE_A(255), 0, right - tx - 4, ALIGN_LEFT); text_draw(it->sub, tx, ly + px * 1.25f + 2, 21, 600, WHITE_A(184), 0, 6 + CPW - 18 - tx, ALIGN_LEFT); }
 /* an addon's subtitle: its addon under it */
        else text_draw(it->lb, tx, yy + h / 2 - text_line_height(px) / 2.0f, px, 700, WHITE_A(255), 0, right - tx - 4, ALIGN_LEFT);
        if (it->grey) gfx_opacity_pop(); }
      gfx_offset_pop(); gfx_clip_pop(); if (lfa) { gfx_offset_pop(); gfx_opacity_pop(); } }
    gfx_clip_pop(); }
  else if (srcs) draw_srcs(ix, lstTop, W);
  else { /* a row: the focused card second from the left, a lit plate (and an accent line) behind it */
    gfx_clip_push(ix, lstTop, W - 2, listH + 16);
    float want = P.tray.idx > 0 && P.tray.idx < P.tray.n ? 30 + xs[P.tray.idx - 1] - 8 : 0, mx = 60 + rowW - (W - 2); if (want > mx) want = mx; if (want < 0) want = 0; mo_to(&P.tray.rowX, want, .3f, C_OUT, 0);
    float ox = ix - P.tray.rowX.v, oy = lstTop + 8;
    PG.nrow = P.tray.n; PG.rowY = oy; PG.rowH = rowH; for (int i = 0; i < P.tray.n; i++) { PG.rowX[i] = ox + 30 + xs[i]; PG.rowW[i] = ws[i]; }
    int fi = P.tray.zone == 0 && !P.tray.onBtn ? P.tray.idx : -1;
    if (fi >= 0 && fi < P.tray.n && P.tray.row[fi].k != 0) { if (!P.tray.ringW) { mo_set(&P.tray.ringX, 0); mo_set(&P.tray.ringY, 0); } mo_to(&P.tray.ringX, 30 + xs[fi], .2f, C_OUT, 0); P.tray.ringW = ws[fi]; P.tray.ringH = hs[fi]; mo_to(&P.tray.ringA, 1, .15f, C_EASE, 0); }
    else mo_to(&P.tray.ringA, 0, .15f, C_EASE, 0);
    gfx_offset_push(ox, oy);
    if (P.tray.ringA.v > 0.001f && P.tray.ringW) { float rx = P.tray.ringX.v, rw = P.tray.ringW, rh = P.tray.ringH; gfx_opacity_push(P.tray.ringA.v); gfx_rrect(rx, 0, rw, rh, 20, WHITE_A(33)); gfx_rrect(rx + rw * 0.22f, rh + 5, rw * 0.56f, 4, 2, ACC2); gfx_opacity_pop(); }
    int rf = TM.rowA.v < 0.999f; if (rf) { gfx_opacity_push(TM.rowA.v); gfx_offset_push(0, 10 * (1 - TM.rowA.v)); } /* what it lists changed: it fades in */
    for (int i = 0; i < P.tray.n; i++) { if (30 + xs[i] - P.tray.rowX.v > W || 30 + xs[i] + ws[i] - P.tray.rowX.v < 0) continue; draw_card(&P.tray.row[i], 30 + xs[i], 0, ws[i], hs[i], i == fi); }
    if (rf) { gfx_offset_pop(); gfx_opacity_pop(); }
    gfx_offset_pop(); gfx_clip_pop(); }
  if (sw) { gfx_offset_pop(); gfx_opacity_pop(); }
  gfx_offset_pop(); gfx_scale_pop(); gfx_opacity_pop();
}
void player_draw(void) {
  if (!P.open) return; float a = P.in.v; gfx_opacity_push(a); multi_draw();
  if (P.mode == 6 && !hub_on()) P.mode = 0; /* (the hub closed from elsewhere: the player has its keys again) */
  if (growPending) { extern int app_snapshot_ready(void); /* the move waits for the page's picture (taking it is a slow frame: never one of the move's) */
    if (!P.open || P.closing) growPending = 0;
    else { gfx_hole(growR[0], growR[1], growR[2], growR[3], growRad, 1); gfx_ring(growR[0], growR[1], growR[2], growR[3], growRad, 1.5f, WHITE_A(34));
      if (app_snapshot_ready()) { growPending = 0; growAt = now_(); planeAt = 0; growFrames = growCalls = 0; growWorst = 0; } } }
  if (growAt > 0) { VidPlaneCaps c = video_plane_caps(); double t = now_() - growAt; float kh = grow_k();
    if (!P.open || P.closing || kh >= 1) { growAt = 0; if (P.open && !P.closing) apply_aspect();
      fprintf(stderr, "[grow] %d frames, %d plane calls, worst frame %.0f ms (lead %d ms, every %d ms%s)\n", growFrames, growCalls, growWorst, c.leadMs, c.stepMs, c.cut ? ", cut" : ""); }
    else { float H[4], W[4];
      if (c.cut) { /* a TV whose plane can't move smoothly: a dip to black in the box, one move behind it, then the window opens */
        if (t >= 0.06 && planeSent[2] < SB_W - 1) { float full[4] = { 0, 0, SB_W, SB_H }; plane_step(full, 1, 1); } }
      else { grow_rect(ease_io((t + c.leadMs / 1000.0) / GROW_D), W); plane_step(W, 1, 0); }
      grow_rect(kh, H); float in = 2 * sinf(3.14159f * kh), rad = growRad * (1 - kh); /* (inset a touch mid-move: a pixel of rounding never shows a line) */
#ifdef SB_MAC
      { extern void video_mac_radius(float); video_mac_radius(rad); } /* (the preview's round corners opening as it grows) */
#endif
      gfx_hole(H[0] + in, H[1] + in, H[2] - 2 * in, H[3] - 2 * in, rad, 1); /* the one window it plays in, round-cornered, cut through the page fading round it */
      { float ra = 1 - kh / 0.4f; if (ra > 0) gfx_ring(H[0] + in, H[1] + in, H[2] - 2 * in, H[3] - 2 * in, rad, 1.5f, WHITE_A((int)(34 * ra))); } /* (the box's thin edge, going as it opens) */
      if (c.cut) { float d = t < 0.12 ? (float)(t / 0.12) : 1 - (float)((t - 0.12) / 0.15); if (d > 0) gfx_rrect(H[0] + in, H[1] + in, H[2] - 2 * in, H[3] - 2 * in, rad, BLACK_A((int)(255 * fminf(1, d)))); }
      growFrames++; { float ms = app_dt() * 1000; if (growFrames > 1 && ms > growWorst) growWorst = ms; static int gl = -1; if (gl < 0) gl = getenv("SB_GROWLOG") != NULL; static double lastT; double nt = now_(); if (gl) fprintf(stderr, "[grow] frame %d: dt %.1f ms, real %.1f ms, k %.3f, at %.3f\n", growFrames, ms, (nt - lastT) * 1000, grow_k(), nt - growAt); lastT = nt; } }
  }
  if (shrinkAt > 0) { VidPlaneCaps c = video_plane_caps(); double t = now_() - shrinkAt; float k = shrink_k(), H[4], W[4];
    grow_rect(1 - k, H); float in = 2 * sinf(3.14159f * k), rad = growRad * k;
    if (c.cut) { if (k >= 0.999f) plane_step(growR, 0, 1); }
    else { grow_rect(1 - ease_io((t - c.leadMs / 1000.0) / SHRINK_D), W); plane_step(W, 0, 0); } /* (the picture behind the hole: it shrinks after it, so it always covers it) */
#ifdef SB_MAC
    { extern void video_mac_radius(float); video_mac_radius(rad); }
#endif
    gfx_hole(H[0] + in, H[1] + in, H[2] - 2 * in, H[3] - 2 * in, rad, 1); /* the window shrinking back into the box, the page coming back round it */
    { float ra = (k - 0.6f) / 0.4f; if (ra > 0) gfx_ring(H[0] + in, H[1] + in, H[2] - 2 * in, H[3] - 2 * in, rad, 1.5f, WHITE_A((int)(34 * fminf(1, ra)))); }
    growFrames++; { float ms = app_dt() * 1000; if (growFrames > 1 && ms > growWorst) growWorst = ms; }
    if (k >= 1) { shrinkAt = 0; growAt = 0; plane_step(growR, 0, 1); /* (the box exactly, at the end) */
      fprintf(stderr, "[grow] back into the box: %d frames, %d plane calls, worst frame %.0f ms\n", growFrames, growCalls, growWorst);
#ifdef SB_MAC
      { extern void video_mac_radius(float); video_mac_radius(0); }
#endif
      if (lpk_take_back(P.id)) { keepVideo = 1; finish_close(); return; } close_player(0, 0); }
    return; }
  zap_shade();
  if (P.live && P.artA.v < 0.999f) { prev_shade(); draw_osd(); if (stripT >= 0 && seekK.v > 0.01f && !live_bcast()) prev_strip(stripT, stripFrom, 1); draw_tray(); xr_draw(); } /* (the previews over the controls, under a tray) */
  else if (P.quiet && P.mode != 6) live_card_draw(); /* switching channels, nothing running yet: the corner pill alone */
  else if (P.artA.v < 0.999f) hand_draw();
  draw_art();
  lx_draw();
  gfx_opacity_pop();
}
static void chapters_hook(const char *url, int p2p) { (void)url; (void)p2p; P.chapWant = 1; }
void player_init(void) { prev_available(); /* (the previews' decoder loading in the background from the start: the first film knows it is there) */ video_init(SB_APP_ID); app_play_hook = player_play; player_chapters_hook = chapters_hook; if (getenv("SB_PLAY_ALT")) player_fallback(getenv("SB_PLAY_ALT")); /* development: the fallback of the SB_PLAY stream */
  if (getenv("SB_PLAY_TRSUB")) { TrailerSub t; memset(&t, 0, sizeof t); snprintf(t.url, sizeof t.url, "%s", getenv("SB_PLAY_TRSUB")); snprintf(t.lang, sizeof t.lang, "eng"); snprintf(t.name, sizeof t.name, "English"); snprintf(t.kind, sizeof t.kind, "srt"); player_trailer_subs(&t, 1); } /* development: a caption track for an SB_PLAY trailer */ }

