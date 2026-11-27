/* etapa-89: part of player.c (included there, one translation unit): the subtitles, a show's own picks, auto-sync */
/* ---- subtitles ---- */
static const char *want_sub(void) { return store_get("subsLang", "eng"); }
static int only_mine(const char *lang, int on) { const char *w = store_get("subsLang", "eng"); return on || !*w || !lang || !*lang || strcmp(store_get("subsOnlyMine", "false"), "true") || subs_lang_match(lang, w) || ((!strcmp(w, "pob") || !strcmp(w, "por")) && (subs_lang_match(lang, "pob") || subs_lang_match(lang, "por"))); } /* (Portuguese: Brazil's and Portugal's both) */ /* Settings › Only my language: the others left out of the lists (the one on stays) */
/* Settings > Subtitles > Subtitle preference: Forced only = a film starts with a forced track in that language (only
   the lines in another language and the signs), or none; All subtitles = as always (PR #50) */
static int want_forced(void) { return !strcmp(store_get("subsMode", "all"), "forced"); }
static int emb_forced(const VidTrack *t) { return t->forced || forcedsubs_name(t->name); } /* the file's flag, else the track's name */
static void tag_forced(char *lb, size_t cap, int forced) { /* " · Forced" after a forced track's name, unless the name says it */
  if (!forced || forcedsubs_name(lb)) return; size_t l = strlen(lb); snprintf(lb + l, cap - l, " · %s", T("Forced")); }
/* how high the subtitles sit over the bottom of the picture (resting, the controls away), in % of the screen: 8 as before */
static const int SUB_POS[] = { 3, 8, 13, 18, 23, 28 };
static int pos_idx(void) { int i = atoi(store_get("subsPosition", "1")); return i < 0 || i > 5 ? 1 : i; }
/* the height in % (subsPosPct, set 1 % at a time while watching); before it, one of the six steps above */
#define POS_MIN 2
#define POS_MAX 30
static int pos_pct(void) { int p = atoi(store_get("subsPosPct", "0")); return p >= POS_MIN && p <= POS_MAX ? p : SUB_POS[pos_idx()]; }
static int size_idx(void) { int i = atoi(store_get("subtitleSize", "2")); return i < 0 || i > 6 ? 2 : i; }
static const int SUB_SIZES[] = { 72, 80, 100, 120, 140, 160, 180 };
static const char *SUB_COLORS[] = { "#ffffff", "#ffe600", "#00e5ff" }, *SUB_BGS[] = { "0", "1", ".6" };
/* the lines' brightness (from PR #39, gmarques33): white a little grey, softer on an OLED in a dark room; the colours go
   toward black, never see-through (readable over a bright scene) */
static const char *SUB_BRIGHT[] = { "100", "80", "60", "40" };
static int bright_idx(void) { const char *v = store_get("subsBright", "100"); for (int i = 0; i < 4; i++) if (!strcmp(SUB_BRIGHT[i], v)) return i; return 0; }
static uint32_t col_dim(uint32_t c, float k) { if (k >= 0.999f) return c; return (c & 0xff000000u) | ((uint32_t)(((c >> 16) & 255) * k) << 16) | ((uint32_t)(((c >> 8) & 255) * k) << 8) | (uint32_t)((c & 255) * k); }
static int bg_idx(void) { const char *v = store_get("subsBgStyle", "0"); for (int i = 0; i < 3; i++) if (!strcmp(SUB_BGS[i], v)) return i; return 0; }
static int color_idx(void) { const char *v = store_get("subsColor", "#ffffff"); for (int i = 0; i < 3; i++) if (!strcasecmp(SUB_COLORS[i], v)) return i; return -1; }
static uint32_t hexcol(const char *h) { return 0xff000000u | (unsigned)strtoul(h + 1, NULL, 16); }
static void refresh_tray(int kind) { if (P.tray.kind == kind) render_tray(0); }
static void sync_stop(int keepTimes);
static unsigned cueTimesGen; /* bumped when the lines' times change in place (Auto-sync): their index is made again */
/* The chosen subtitle's progress. A token per choice (an answer for an earlier choice is dropped); while its lines
   load, the tray shows a spinner on it; a file that cannot be read says so. Each choice and its first line on screen
   go to the log ([subs]), so the TV's log tells what happened. */
static struct { unsigned tok; double at, checkAt, waitAt; int wait, tv, mkv, failed, shown, retry, ready; } SL; /* waitAt: since when the reader is behind the playhead */
static char *extAss; static unsigned extAssTok; /* the downloaded subtitle when it is SubStation: the file whole, for styled drawing (valid for SL.tok == extAssTok) */
static int embKind[16];            /* the listed embedded tracks: 1 plain text, 2 SubStation, 0 unknown (the TV's list) */
/* an embedded track's name in the lists: its own title, else its language, else "Track 3" and what it is (some files
   name and tag none of their tracks: a bare "#" said nothing) */
static void emb_label(int i, char *out, size_t cap) {
  const VidTrack *t = &P.emb[i]; const char *ln = lang_name(t->lang); char num[8]; snprintf(num, sizeof num, "%d", i + 1);
  if (t->name[0] && strcmp(t->name, "#")) snprintf(out, cap, "%s", t->name);
  else if (*ln) snprintf(out, cap, "%s", ln);
  else if (t->lang[0] && strcmp(t->lang, "und")) snprintf(out, cap, "%s", t->lang);
  else snprintf(out, cap, "%s", T1("Track {n}", "n", num));
  size_t l0 = strlen(out); tag_forced(out, cap, t->forced);
  if (strlen(out) == l0 && !(t->name[0] && strcmp(t->name, "#")) && i < 16 && embKind[i]) { size_t l = strlen(out); snprintf(out + l, cap - l, " · %s", embKind[i] == 2 ? "ASS" : T("Text")); } /* (the format's own short name: the column is narrow; a forced track says that instead) */
}
static void sub_begin(void) { SL.tok++; SL.at = SL.waitAt = now_(); SL.checkAt = 0; SL.wait = 1; SL.tv = SL.mkv = SL.failed = SL.shown = SL.retry = SL.ready = 0; }
static void sub_reset(void) { unsigned t = SL.tok + 1; memset(&SL, 0, sizeof SL); SL.tok = t; memset(embKind, 0, sizeof embKind); }
static int tv_has_text(int n) { VidTrack t[24]; int k = video_text_tracks(t, 24); for (int i = 0; i < k; i++) if (t[i].idx == n) return 1; return 0; }
static void subs_off(void) { sync_stop(0); snprintf(P.curSub, sizeof P.curSub, "off"); subs_free(P.cues, P.ncues); P.cues = NULL; P.ncues = 0; sub_begin(); SL.wait = 0; video_select_text(-1); if (mkv) mkv_select(mkv, -1); fprintf(stderr, "[subs] off\n"); refresh_tray(4); }
static void set_ext_sub(int list, const SubEntry *e);
/* etapa-71: the translated subtitles (core/subtr) */
static int subtr_still(Cue *c, int n, unsigned tok) { return P.open && P.cues == c && P.ncues == n && SL.tok == tok && !strncmp(P.curSub, "ext:", 4); }
static void subtr_begin(void) {
  const char *l = i18n_lang(); int mine = 0; const char *w = !strncmp(l, "pt", 2) ? "pob" : l;
  for (int i = 0; i < P.nExt && !mine; i++) mine = subs_lang_match(P.ext[i].lang, w) || (!strcmp(w, "pob") && subs_lang_match(P.ext[i].lang, "por"));
  int was = subtr_active(); subtr_start(P.cues, P.ncues, P.pick.lang, mine, video_time(), SL.tok, subtr_still);
  if (subtr_active() && !was) toast(T("Translating these subtitles into your language"), 3000, NULL); }
static void on_sub_cues(int status_, Cue *cues, int n, void *ud) {
  if (!P.open || (unsigned)(uintptr_t)ud != SL.tok || strncmp(P.curSub, "ext:", 4)) { subs_free(cues, n); return; } /* another choice since */
  if (n > 0) { subs_free(P.cues, P.ncues); P.cues = cues; P.ncues = n; SL.wait = 0; free(extAss); extAss = subs_take_script(); extAssTok = SL.tok; fprintf(stderr, "[subs] external: %d lines read in %.1f s\n", n, now_() - SL.at); refresh_tray(4);
    if (!extAss) subtr_begin(); return; } /* (etapa-71: put into the app's language as it plays, when none in it was found) */
  subs_free(cues, n);
  if (status_ <= 0 && !SL.retry) { SL.retry = 1; fprintf(stderr, "[subs] external: no answer, asking once more\n"); subs_load_lang(P.curSub + 4, P.pick.lang, on_sub_cues, (void *)(uintptr_t)SL.tok); return; } /* one network hiccup */
  fprintf(stderr, "[subs] external: failed (HTTP %d, %s)\n", status_, status_ >= 200 && status_ < 300 ? "nothing readable in it" : "no file");
  toast(T("Couldn’t load these subtitles"), 3000, NULL);
  snprintf(P.curSub, sizeof P.curSub, "off"); SL.wait = 0; refresh_tray(4); /* the tray tells the truth: nothing is on */
}
/* a downloaded subtitle is known by the list it came from and its identity there (the same file can be in both lists,
   or twice in one from two addons): only that row is ticked, whatever the lists become after */
static int sub_is(int list, const SubEntry *e) { return list && P.pick.list == list && !strncmp(P.curSub, "ext:", 4) && !strcmp(e->url, P.curSub + 4) && !strcmp(e->addon, P.pick.addon) && !strcmp(e->lang, P.pick.lang); }
static int sub_is_arabic(void);
static int sub_row(int list, const SubEntry *l, int n) { for (int i = 0; i < n; i++) if (sub_is(list, &l[i])) return i; return -1; }
static void set_ext_sub(int list, const SubEntry *e) {
  const char *u = e->url; sync_stop(0); snprintf(P.curSub, sizeof P.curSub, "ext:%s", u); P.pick.list = list; snprintf(P.pick.addon, sizeof P.pick.addon, "%s", e->addon); snprintf(P.pick.lang, sizeof P.pick.lang, "%s", e->lang); subs_free(P.cues, P.ncues); P.cues = NULL; P.ncues = 0; video_select_text(-1); if (mkv) mkv_select(mkv, -1);
  sub_begin(); fprintf(stderr, "[subs] chose external %s\n", u); subs_load_lang(u, e->lang, on_sub_cues, (void *)(uintptr_t)SL.tok); refresh_tray(4);
}
static void set_emb_sub(int n) {
  sync_stop(0); snprintf(P.curSub, sizeof P.curSub, "emb:%d", n); subs_free(P.cues, P.ncues); P.cues = NULL; P.ncues = 0; sub_begin();
  if (P.mkvTracksOk && mkv) { SL.mkv = 1; video_select_text(-1); mkv_select(mkv, n); fprintf(stderr, "[subs] chose embedded #%d: read from the file\n", n); } /* shown once its lines are read */
  else { SL.wait = 0; SL.tv = 1; video_select_text(n); fprintf(stderr, "[subs] chose embedded #%d: drawn by the TV\n", n); }
  refresh_tray(4);
}
/* the reader's progress for the chosen embedded track, a few times a second: ready (the TV lets go if it was drawing
   meanwhile), still reading (a plain text track goes to the TV after 6 s, when the TV lists it), or given up (the TV
   draws it when it can, else the viewer is told) */
/* what the viewer's subtitle style reaches on the lines shown: 0 all of it (plain text drawn here), 1 size, place and
   brightness (styled lines keep their own colours and boxes), 2 nothing (pictures, or lines the TV draws) */
int player_sub_scope(void);
static void sub_check(void) {
  if (!SL.mkv || strncmp(P.curSub, "emb:", 4) || now_() < SL.checkAt) return; SL.checkAt = now_() + .25;
  int n = atoi(P.curSub + 4), st = mkv_track_status(mkv, n), k = -1; for (int i = 0; i < P.nEmb; i++) if (P.emb[i].idx == n) k = i;
  if (st > 0 && SL.wait) { SL.wait = 0; if (SL.tv && !SL.failed) { SL.tv = 0; video_select_text(-1); } if (!SL.ready) fprintf(stderr, "[subs] embedded #%d ready in %.1f s\n", n, now_() - SL.at); SL.ready = 1; refresh_tray(4); }
  else if (!st && !SL.wait && !SL.failed) { SL.wait = 1; SL.waitAt = now_(); refresh_tray(4); } /* after a seek, or the reader fell behind: reading the new spot */
  else if (st < 0 && !SL.failed) { SL.failed = 1; SL.wait = 0;
    if (tv_has_text(n)) { if (!SL.tv) video_select_text(n); SL.tv = 1; fprintf(stderr, "[subs] embedded #%d: the file can't be read here (%s): the TV draws it\n", n, mkv_error(mkv)); }
    else { fprintf(stderr, "[subs] embedded #%d: the file can't be read here (%s)\n", n, mkv_error(mkv)); toast(T("Subtitle extraction failed"), 3000, NULL); snprintf(P.curSub, sizeof P.curSub, "off"); mkv_select(mkv, -1); }
    refresh_tray(4); }
  else if (!st && SL.wait && !SL.tv && now_() - SL.waitAt > 6 && k >= 0 && embKind[k] == 1 && tv_has_text(n)) { SL.tv = 1; video_select_text(n); fprintf(stderr, "[subs] embedded #%d %s: the TV draws it meanwhile\n", n, SL.ready ? "fell behind the playhead (the server slow or busy)" : "still being read"); } /* (mid-film too: lines read fine for an hour, then a busy server held the reader back and they waited, loading, until it caught up) */
}
static int sub_pictures(void) { /* the chosen embedded track is pictures (PGS, VobSub, DVB) */
  if (strncmp(P.curSub, "emb:", 4)) return 0; int n = atoi(P.curSub + 4); for (int i = 0; i < P.nEmb; i++) if (P.emb[i].idx == n) return embKind[i] == 3; return 0; } /* (none known here yet: a picture track is the TV's, and says so as the TV's) */
int player_sub_scope(void) {
  if (asssubs_on()) return 1;
#ifdef SB_ANDROIDTV
  if (!strncmp(P.curSub, "emb:", 4) && SL.tv && sub_pictures()) return 2; /* (Android TV: its own subtitle view takes the colour, outline, brightness and position (video_atv.c), so only picture tracks take nothing) */
#else
  if (!strncmp(P.curSub, "emb:", 4) && SL.tv) return 2; /* (the TV draws them, for good or meanwhile: nothing of ours reaches them) */
#endif
  return 0; }
/* an ASS / SSA subtitle is drawn styled (libass) while the setting says so: an embedded track read from the file, or a
   downloaded file (any source) handed over whole, on its own timeline (the delay and a sync mapped onto it; its plain
   lines stay what the sync works on); plain otherwise, and from the moment renders get too slow here */
static double sync_map(double t, double *rate);
static void ass_sync(void) {
  int n = -1, k = -1, styled = !P.assPlain && strcmp(store_get("subsAssStyle", "styled"), "plain");
  if (!strncmp(P.curSub, "emb:", 4) && SL.mkv && !SL.tv && mkv && styled) { n = atoi(P.curSub + 4); for (int i = 0; i < P.nEmb; i++) if (P.emb[i].idx == n) k = i; }
  int ext = styled && !strncmp(P.curSub, "ext:", 4) && extAss && extAssTok == SL.tok && P.ncues;
  int want = ext || (k >= 0 && embKind[k] == 2);
  static int openN = -1; static const void *openMkv; static unsigned openTok;
  if (!want) { if (asssubs_on()) { asssubs_close(); openN = -1; } return; }
  if (ext) { if (!asssubs_on() || openN != -2 || openTok != SL.tok) { asssubs_open_script(extAss, P.pick.lang); openN = -2; openTok = SL.tok; } }
  else if (!asssubs_on() || openN != n || openMkv != mkv) { asssubs_open(mkv, n, P.url, P.emb[k].lang); openN = n; openMkv = mkv; }
  int slow = asssubs_too_slow();
  if (slow) { P.assPlain = 1; asssubs_close(); openN = -1; if (!ext) mkv_style(mkv, n, 0); if (slow == 1) toast(T("Styled subtitles are too heavy here: shown plain"), 4000, NULL); return; }
  /* the picture's rectangle: fitted inside the screen (Fill and Zoom: the screen) */
  float vx = 0, vy = 0, vw = SB_W, vh = SB_H; int w = video_width(), h = video_height();
  if (w > 0 && h > 0 && !strcmp(P.aspect, "fit")) { float a = (float)w / h; if (a > SB_W / SB_H) { vh = SB_W / a; vy = (SB_H - vh) / 2; } else { vw = SB_H * a; vx = (SB_W - vw) / 2; } }
  VidState vs = video_state(); double t = video_time() - P.subDelay, rate = 1; if (ext) t = sync_map(t, &rate);
  asssubs_set_line(pos_pct() > 8 ? pos_pct() - 8 : 0); /* the Position setting: 8 % (the default) leaves styled lines where the script puts them */
  asssubs_tick(t, rate, vs == VID_PLAYING && !video_settling(), video_fps(), vx, vy, vw, vh, w, h, SUB_SIZES[size_idx()] / 100.0f);
}
static const char *skip_mode(void) { return store_get("skipIntro", "auto"); } /* the intro / recap button: auto, show, off */
static void sync_auto_arm(void);
/* a language's regional variant, where subtitles name one: Brazilian Portuguese (pob), Latin American Spanish (spl);
   "" for the rest. The language match takes either for the other, so the variant asked for is looked for first */
static const char *lang_variant(const char *c) {
  char l[16]; size_t i = 0; for (; c && c[i] && i + 1 < sizeof l; i++) l[i] = (char)tolower((unsigned char)c[i]); l[i] = 0;
  if (!strcmp(l, "pob") || !strcmp(l, "pb") || !strcmp(l, "pt-br") || !strcmp(l, "pt_br") || !strcmp(l, "ptbr")) return "pob";
  if (!strcmp(l, "spl") || !strcmp(l, "es-419") || !strcmp(l, "es-la") || !strcmp(l, "es_la") || !strcmp(l, "es-mx")) return "spl";
  return ""; }
static int variant_ok(const char *have, const char *want) { return !strcmp(lang_variant(have), lang_variant(want)); }
/* ---- a show's own subtitles and audio: picked by hand in one of its episodes (other than what the settings would give),
   kept for the whole show and used in its other episodes; picked again, kept again. TV shows only ---- */
static int show_prefs_on(void) { return (!strcmp(P.type, "series") || !strcmp(P.type, "movie")) && !P.isTrailer && P.id[0]; } /* etapa-87: films keep theirs too */
static const cJSON *show_pref(void) { if (!show_prefs_on()) return NULL; cJSON *a = store_node("showPrefs"); return cJSON_IsObject(a) ? cJSON_GetObjectItemCaseSensitive(a, P.id) : NULL; }
static void show_pref_put(const char *k, const char *v) { /* v NULL: that one forgotten (back to the settings') */
  cJSON *a = store_node("showPrefs"); cJSON *n = cJSON_IsObject(a) ? cJSON_Duplicate(a, 1) : cJSON_CreateObject(); cJSON *o = cJSON_GetObjectItemCaseSensitive(n, P.id); if (!o) { o = cJSON_CreateObject(); cJSON_AddItemToObject(n, P.id, o); }
  cJSON_DeleteItemFromObjectCaseSensitive(o, k); if (v) cJSON_AddStringToObject(o, k, v); if (!cJSON_GetArraySize(o)) cJSON_DeleteItemFromObjectCaseSensitive(n, P.id); store_set_node("showPrefs", n); }
static void show_pref_sub(void) { /* the subtitle on now, chosen by hand */
  P.autoSubDone = 1; /* (a list arriving later never overrides it: Off picked while they load stays off) */
  if (!show_prefs_on()) return; const char *w = want_sub(); char kind[8] = "", lang[24] = "", name[96] = "";
  if (!strcmp(P.curSub, "off")) snprintf(kind, sizeof kind, "off");
  else if (!strncmp(P.curSub, "emb:", 4)) { int n = atoi(P.curSub + 4); for (int i = 0; i < P.nEmb; i++) if (P.emb[i].idx == n) { snprintf(kind, sizeof kind, "emb"); snprintf(lang, sizeof lang, "%s", P.emb[i].lang); snprintf(name, sizeof name, "%s", P.emb[i].name); } }
  else if (!strncmp(P.curSub, "ext:", 4)) { snprintf(kind, sizeof kind, "ext"); snprintf(lang, sizeof lang, "%s", P.pick.lang); snprintf(name, sizeof name, "%s", P.pick.addon); }
  if (!kind[0]) return;
  int asSettings = !strcmp(kind, "off") ? !*w : (*w && lang_match(lang, w)); /* what the settings give anyway: nothing to keep */
  show_pref_put("sk", asSettings ? NULL : kind); show_pref_put("sl", asSettings ? NULL : lang); show_pref_put("sn", asSettings || !name[0] ? NULL : name);
  fprintf(stderr, "[subs] %s for this show\n", asSettings ? "as the settings say" : "kept");
}
/* Settings > Audio's language: "" for Original (the film's own track, left as it starts), else the language to pick */
static const char *aud_setting(void) { const char *v = store_get("defaultAudioTrack", ""); return strcmp(store_get("enableDefaultAudioTrack", "false"), "true") || !strcmp(v, "orig") ? "" : v; }
static void show_pref_aud(const char *lang, const char *name) {
  P.autoAudDone = 1; /* (picked by hand: the automatic pick, still trying while the tracks come, never overrides it) */
  if (!show_prefs_on() || !lang) return; const char *dw = aud_setting();
  int asSettings = *dw && lang_match(lang, dw); show_pref_put("al", asSettings || !*lang ? NULL : lang); show_pref_put("an", asSettings || !name || !*name ? NULL : name);
  fprintf(stderr, "[audio] %s for this show\n", asSettings ? "as the settings say" : "kept");
}
static const char *aud_want(void) { const char *al = js_str(show_pref(), "al", NULL); if (al && *al) return al; return aud_setting(); }
static void auto_sub(void) {
  if (P.autoSubDone || strcmp(P.curSub, "off")) return;
  const cJSON *sp = show_pref(); const char *pk = js_str(sp, "sk", ""), *pn = js_str(sp, "sn", "");
  if (!strcmp(pk, "off")) { P.autoSubDone = 1; return; } /* this show: none, as chosen */
  const char *w = *pk ? js_str(sp, "sl", "") : want_sub(); if (!*w) return;
  again:;
  if (!strcmp(pk, "ext") && (P.nExt || P.nAdx)) { /* this show's file from an addon: the same addon first, the language */
    for (int pass = 0; pass < 2; pass++) { if (pass && (*pn ? P.adxLoading : P.extLoading) && !P.sameWaitOver) { if (!P.sameWaitAt) P.sameWaitAt = now_() + 10; return; } /* (any addon's file in that language only once the list that may hold the same addon's is in, or 10 s on) */
      for (int i = 0; i < P.nAdx; i++) if (lang_match(P.adx[i].lang, w) && (pass || !strcmp(P.adx[i].addon, pn))) { P.autoSubDone = 1; set_ext_sub(2, &P.adx[i]); sync_auto_arm(); return; }
      for (int i = 0; i < P.nExt; i++) if (lang_match(P.ext[i].lang, w) && (pass || !strcmp(P.ext[i].addon, pn))) { P.autoSubDone = 1; set_ext_sub(1, &P.ext[i]); sync_auto_arm(); return; } }
    if (P.extLoading || P.adxLoading) return; } /* (the lists still coming) */
  if (!strcmp(pk, "emb") && *pn) for (int j = 0; j < P.nEmb; j++) if (lang_match(P.emb[j].lang, w) && !strcmp(P.emb[j].name, pn)) { P.autoSubDone = 1; set_emb_sub(P.emb[j].idx); return; } /* the same track by its name */
  int fo = want_forced(), em = -1;
  for (int j = 0; j < P.nEmb; j++) if (lang_match(P.emb[j].lang, w) && (!fo || emb_forced(&P.emb[j])) && (em < 0 || (!fo && emb_forced(&P.emb[em]) && !emb_forced(&P.emb[j])))) em = j; /* (All subtitles: a full track over a forced one, flagged or only named so: an anime's "Songs & Signs" listed first showed no dialogue) */
  if (em >= 0) { P.autoSubDone = 1; set_emb_sub(P.emb[em].idx); return; } /* embedded wins: it is in sync with the file */
  if (!P.embKnown && !P.embWaitOver) { if (!P.embWaitAt) P.embWaitAt = now_() + 8; return; } /* the file's own tracks get 8 s to show up first */
  for (int pass = 0; pass < 2; pass++) { /* the variant asked for first (Brazilian for Brazilian, European for European), then any */
    for (int i = 0; i < P.nExt; i++) if (lang_match(P.ext[i].lang, w) && (!fo || P.ext[i].forced) && (pass || variant_ok(P.ext[i].lang, w))) { P.autoSubDone = 1; set_ext_sub(1, &P.ext[i]); sync_auto_arm(); return; }
    for (int i = 0; i < P.nAdx; i++) if (lang_match(P.adx[i].lang, w) && (!fo || P.adx[i].forced) && (pass || variant_ok(P.adx[i].lang, w))) { P.autoSubDone = 1; set_ext_sub(2, &P.adx[i]); sync_auto_arm(); return; } }
  if (*pk && !P.extLoading && !P.adxLoading && *want_sub() && !lang_match(w, want_sub())) { /* this show's language not in this episode, every list in: the settings' */
    fprintf(stderr, "[subs] no %s for this show here: the settings' %s\n", w, want_sub()); pk = ""; pn = ""; w = want_sub(); goto again; }
}
/* a trailer's captions (IMDb's or YouTube's): handed over with the trailer, read when the CC button turns them on */
static TrailerSub trSubs[TRAILER_MAX_SUBS], trNext[TRAILER_MAX_SUBS]; static int trSubsN, trNextN;
void player_trailer_subs(const TrailerSub *subs, int n) { if (n > TRAILER_MAX_SUBS) n = TRAILER_MAX_SUBS; if (n < 0 || !subs) n = 0; if (n) memcpy(trNext, subs, n * sizeof *subs); trNextN = n; }
static void on_tr_cues(Cue *cues, int n, void *ud) {
  if (!P.open || (unsigned)(uintptr_t)ud != P.trSubTok || strncmp(P.curSub, "ext:", 4)) { subs_free(cues, n); return; }
  subs_free(P.cues, P.ncues); P.cues = cues; P.ncues = n; SL.wait = 0; fprintf(stderr, "[subs] trailer captions: %d lines\n", n);
  if (!n) toast(T("Couldn’t load these subtitles"), 3000, NULL);
}
/* one press on (the language the viewer reads, else the first), one press off */
static void cc_toggle(void) {
  if (!trSubsN) return;
  if (strcmp(P.curSub, "off")) { subs_off(); return; }
  const char *w = want_sub(); int pick = 0; for (int i = 0; i < trSubsN; i++) if (lang_match(trSubs[i].lang, w)) { pick = i; break; }
  snprintf(P.curSub, sizeof P.curSub, "ext:%s", trSubs[pick].url); P.pick.list = 0; subs_free(P.cues, P.ncues); P.cues = NULL; P.ncues = 0; sub_begin();
  P.trSubTok++; trailer_sub_load(&trSubs[pick], on_tr_cues, (void *)(uintptr_t)P.trSubTok);
}
/* the built-in OpenSubtitles list; no answer: asked again 15, 30 and 45 s later */
static void on_subs(SubEntry *subs, int n, int ok, void *ud) {
  if (!P.open || (unsigned)(uintptr_t)ud != P.subReq) { free(subs); return; }
  if (!ok && P.extTry < 3) { free(subs); P.extRetryAt = now_() + (P.extTry + 1) * 15; return; }
  free(P.ext); P.ext = subs; P.nExt = n; P.extLoading = 0; auto_sub(); refresh_tray(4); /* (an empty list too: what waited on it goes on) */
}
/* the addons' lists (the stream's own, every subtitles addon's): each answer replaces the list with all that came so
   far; nothing from anyone: asked again 15, 30 and 45 s later */
static void on_adx(SubEntry *subs, int n, int final, int ok, void *ud) {
  if (!P.open || (unsigned)(uintptr_t)ud != P.adxReq) { free(subs); return; }
  if (final && !ok && !n && P.adxTry < 3 && P.nSubAddons) { free(subs); P.adxRetryAt = now_() + (P.adxTry + 1) * 15; return; }
  free(P.adx); P.adx = subs; P.nAdx = n; P.adxLoading = !final; if (n || final) auto_sub(); refresh_tray(4);
}
static int hashable(const char *u) { return !strncmp(u, "http", 4) && !strstr(u, ".m3u8"); }
static char artName[256], artPoster[512], artBg[512], artLogo[512]; /* the next play's title and pictures as handed over (a phone's) */
static SubEntry castSubs[16]; static int castSubsN = -1; /* the next play's subtitles as a phone sent them (-1: none sent) */
static void load_ext_subs(int n) { if (!P.type[0] || !P.vid[0] || P.isTrailer) return; P.extLoading = 1;
  if (castSubsN >= 0) { int k = castSubsN; castSubsN = -1; SubEntry *l = calloc(16, sizeof *l); memcpy(l, castSubs, sizeof castSubs); P.subReq++; on_subs(l, k, 1, (void *)(uintptr_t)P.subReq); return; } /* cast: the phone's own list */
  if (getenv("SB_PLAY_SUBS")) { SubEntry *l = calloc(16, sizeof *l); int k = 0; char b[4096]; snprintf(b, sizeof b, "%s", getenv("SB_PLAY_SUBS")); /* development: "url,url,..." as the external list */
    for (char *u = strtok(b, ","); u && k < 16; u = strtok(NULL, ",")) { snprintf(l[k].lang, sizeof l[k].lang, "eng"); snprintf(l[k].name, sizeof l[k].name, "English"); snprintf(l[k].url, sizeof l[k].url, "%s", u); l[k].n = k + 1; l[k].from = SUB_FALLBACK; k++; }
    P.subReq++; on_subs(l, k, 1, (void *)(uintptr_t)P.subReq); return; }
  P.extTry = n; P.subReq++; subs_list(P.type, P.vid, on_subs, (void *)(uintptr_t)P.subReq); }
static void load_addon_subs(int n) { if (!P.type[0] || !P.vid[0] || P.isTrailer || getenv("SB_PLAY_SUBS")) return;
  P.adxTry = n; P.adxReq++; P.nSubAddons = subs_addons_for(P.type, P.vid);
  SubQuery q; memset(&q, 0, sizeof q); snprintf(q.type, sizeof q.type, "%s", P.type); snprintf(q.vid, sizeof q.vid, "%s", P.vid); snprintf(q.url, sizeof q.url, "%s", P.url);
  snprintf(q.filename, sizeof q.filename, "%s", P.st.filename); snprintf(q.hash, sizeof q.hash, "%s", P.st.hash); q.size = P.st.size > 0 ? P.st.size : P.size; q.hashable = hashable(P.url);
  SubEntry *own = NULL; int nown = sb_stream_subs(&P.st, &own);
  if (!P.nSubAddons && !nown) { free(own); return; } /* nobody to ask */
  P.adxLoading = 1; subs_find(&q, own, nown, on_adx, (void *)(uintptr_t)P.adxReq); free(own); }
/* the file's subtitle tracks: read from the stream itself when it is a Matroska file (named, styled, delayed like the
   others: listed as soon as its head is read, their lines follow); else the ones the TV's pipeline reports, which it
   draws itself (their numbers are the same: the order among the file's subtitle tracks). Asked every second until
   settled; the tray is redrawn only when the list changes */
static void load_emb_tracks(void) {
  int st = mkv ? mkv_state(mkv) : MKV_FALLBACK; unsigned sig = 0; for (int i = 0; i < P.nEmb; i++) sig = sig * 31 + (unsigned)P.emb[i].idx + 7; sig = sig * 3 + (unsigned)P.nEmb + (unsigned)P.embKnown * 101;
  if (mkv && st != MKV_FALLBACK && !mkv_listed(mkv)) return; /* the file's head is still being read */
  if (mkv && st != MKV_FALLBACK) { MkvTrack t[16]; int n = mkv_tracks(mkv, t, 16), k = 0;
    for (int i = 0; i < n && k < 16; i++) { if (!t[i].text) continue; embKind[k] = strstr(t[i].codec, "ASS") || strstr(t[i].codec, "SSA") ? 2 : 1; VidTrack *v = &P.emb[k++]; memset(v, 0, sizeof *v); snprintf(v->lang, sizeof v->lang, "%s", t[i].lang); snprintf(v->name, sizeof v->name, "%s", t[i].name); v->idx = t[i].n; v->forced = t[i].forced; }
    P.nEmb = k; P.embKnown = 1; P.mkvTracksOk = 1; }
  else { P.nEmb = video_text_tracks(P.emb, 16); memset(embKind, 0, sizeof embKind); P.mkvTracksOk = 0; if (P.nEmb || video_live()) P.embKnown = 1; }
  unsigned sig2 = 0; for (int i = 0; i < P.nEmb; i++) sig2 = sig2 * 31 + (unsigned)P.emb[i].idx + 7; sig2 = sig2 * 3 + (unsigned)P.nEmb + (unsigned)P.embKnown * 101;
  if (sig2 != sig) { fprintf(stderr, "[subs] %d embedded track(s) %s:", P.nEmb, P.mkvTracksOk ? "read from the file" : "from the TV"); for (int i = 0; i < P.nEmb; i++) fprintf(stderr, " #%d %s \"%s\"%s", P.emb[i].idx, P.emb[i].lang, P.emb[i].name, P.emb[i].forced ? " forced" : ""); fprintf(stderr, "\n"); }
  if (!P.autoSubDone) auto_sub(); if (sig2 != sig) refresh_tray(4);
}

/* ---- Auto-sync: the subtitle on (a downloaded one) lined up with the file's own subtitle tracks, read near the
   playhead by the Matroska reader (any language: only when lines are spoken matters). They are waited for while they
   are being read (a torrent brings them in as its pieces arrive), up to 45 s; the matching then runs on a thread of
   its own (core/subsync). The answer applies to the lines' original times (running it again never adds up), the delay
   goes back to 0, Reset undoes it. This TV hands its sound to the screen's own decoder, so there is no audio to match
   against: a file with no text tracks (most HTTP and debrid files) is matched against another subtitle made for this
   file instead, found and read by the job itself; nothing of the kind: nothing to sync against. ---- */
static struct { SubSync *job; int phase; /* 0 idle, 1 waiting for the reference, 2 matching */ int stage; double t0, nextAt, center; char st[160]; double *base; int nbase;
  int quiet, autoWant; unsigned autoTok; double autoAt; /* quiet: the check of a subtitle the app chose by itself, unseen unless it fixes something */
  double f, d; /* the sync applied: line times = original * f + d */ } SY;
/* a film time on the downloaded subtitle's own timeline (styled drawing): the sync applied undone; rate: its seconds per film second */
static double sync_map(double t, double *rate) { if (SY.base && SY.nbase == P.ncues && SY.f > 0) { *rate = 1 / SY.f; return (t - SY.d) / SY.f; } *rate = 1; return t; }
static void sync_stop(int keepTimes) {
  if (SY.job) { subsync_drop(SY.job); SY.job = NULL; } SY.phase = 0; SY.st[0] = 0; SY.quiet = 0;
  if (!keepTimes) { free(SY.base); SY.base = NULL; SY.nbase = 0; }
}
static void sync_end(const char *why) { if (SY.job) { subsync_drop(SY.job); SY.job = NULL; } SY.phase = 0; if (!SY.quiet) snprintf(SY.st, sizeof SY.st, "%s", why); fprintf(stderr, "[subsync] %s%s\n", SY.quiet ? "(by itself) " : "", why); SY.quiet = 0; refresh_tray(4); }
static void sync_undo(void) { /* Reset: the lines' own times again */
  if (SY.base && SY.nbase == P.ncues) { for (int i = 0; i < P.ncues; i++) { P.cues[i].s = SY.base[2 * i]; P.cues[i].e = SY.base[2 * i + 1]; } cueTimesGen++; }
  sync_stop(0);
}
static void sync_run(void) {
  if (SY.phase && SY.quiet) { SY.quiet = 0; snprintf(SY.st, sizeof SY.st, "%s", SY.phase == 2 ? (SY.stage ? T("Looking for a matching subtitle…") : T("Syncing…")) : T("Syncing…")); refresh_tray(4); return; } /* the check the app was making by itself: now shown */
  if (SY.phase) { sync_stop(1); refresh_tray(4); return; } /* pressed again while it runs: it stops */
  if (strncmp(P.curSub, "ext:", 4) || P.isTrailer) return;
  SY.phase = 1; SY.t0 = now_(); SY.nextAt = 0; SY.center = video_time(); snprintf(SY.st, sizeof SY.st, "%s", T("Syncing…")); refresh_tray(4);
  fprintf(stderr, "[subsync] asked at %.0f s\n", SY.center);
}
/* the subtitle's lines with their original times (not the ones a sync before set), for the job to copy */
static Cue *sync_xs(void) {
  Cue *xs = malloc((size_t)(P.ncues ? P.ncues : 1) * sizeof *xs); if (!xs) return NULL; int orig = SY.base && SY.nbase == P.ncues;
  for (int i = 0; i < P.ncues; i++) { xs[i] = P.cues[i]; if (orig) { xs[i].s = SY.base[2 * i]; xs[i].e = SY.base[2 * i + 1]; } }
  return xs;
}
static const char *sync_wait_txt(int stage) { return stage ? T("Looking for a matching subtitle…") : T("Syncing…"); }
/* no text track to match against: every other subtitle listed for this video goes to the job, which keeps those made
   for this file (by hash, carried by the stream, by file name) and asks OpenSubtitles with the file's hash */
static void sync_by_ref(const char *why) {
  int cap = P.nExt + P.nAdx, n = 0; SyncRef *rs = calloc((size_t)(cap ? cap : 1), sizeof *rs); if (!rs) { sync_end(T("Couldn’t sync")); return; }
  for (int l = 1; l <= 2; l++) { const SubEntry *L = l == 1 ? P.ext : P.adx; int nl = l == 1 ? P.nExt : P.nAdx;
    for (int i = 0; i < nl; i++) { const SubEntry *e = &L[i]; if (sub_is(l, e)) continue; SyncRef *r = &rs[n++];
      snprintf(r->url, sizeof r->url, "%s", e->url); snprintf(r->lang, sizeof r->lang, "%s", e->lang); snprintf(r->rel, sizeof r->rel, "%s", e->rel[0] ? e->rel : e->label);
      snprintf(r->from, sizeof r->from, "%s", e->addon); r->hm = e->hm; r->own = e->from == SUB_STREAM; r->fps = e->fps; } }
  SubQuery q; memset(&q, 0, sizeof q); snprintf(q.type, sizeof q.type, "%s", P.type); snprintf(q.vid, sizeof q.vid, "%s", P.vid); snprintf(q.url, sizeof q.url, "%s", P.url);
  snprintf(q.filename, sizeof q.filename, "%s", P.st.filename); snprintf(q.hash, sizeof q.hash, "%s", P.st.hash); q.size = P.st.size > 0 ? P.st.size : P.size; q.hashable = hashable(P.url);
  if (!q.hash[0]) subs_hash_of(P.url, q.hash, &q.size); q.fps = video_fps();
  int canHash = (q.hash[0] || q.hashable) && q.type[0] && q.vid[0];
  fprintf(stderr, "[sync] %s: another subtitle made for this file as the reference (%d listed, hash %s)\n", why, n, q.hash[0] ? q.hash : canHash ? "to work out" : "unknown");
  if (!n && !canHash) { free(rs); sync_end(T("Nothing to sync against")); return; }
  Cue *xs = sync_xs(); if (!xs) { free(rs); sync_end(T("Couldn’t sync")); return; }
  SY.job = subsync_start_ref(xs, P.ncues, SY.center, &q, P.curSub + 4, P.pick.list ? P.pick.lang : "", rs, n); free(xs); free(rs); /* (it keeps its own copies) */
  if (!SY.job) { sync_end(T("Couldn’t sync")); return; }
  SY.phase = 2; SY.stage = 1; if (!SY.quiet) snprintf(SY.st, sizeof SY.st, "%s", sync_wait_txt(1)); refresh_tray(4);
}
static void sync_auto_arm(void) { if (!P.nEmb && !P.isTrailer) { SY.autoWant = 1; SY.autoTok = SL.tok; SY.autoAt = now_(); } } /* (a file with subtitles of its own has them chosen instead) */
/* the subtitle the app chose by itself at the start, for a file with no subtitles of its own: checked once its lines are
   in and the lists have answered (or 15 s on), quietly; lined up only when clearly out (half a second or more, or the
   wrong speed) and the viewer has set no delay, then said in a toast */
static void sync_auto_tick(void) {
  if (!SY.autoWant || SY.phase) return;
  if (SL.tok != SY.autoTok || strncmp(P.curSub, "ext:", 4) || P.isTrailer || P.subDelay != 0) { SY.autoWant = 0; return; } /* another choice since, or a delay of the viewer's */
  if (!P.ncues || SL.wait || now_() - SY.autoAt < 3 || ((P.extLoading || P.adxLoading) && now_() - SY.autoAt < 15)) return;
  SY.autoWant = 0; SY.quiet = 1; SY.phase = 1; SY.t0 = now_(); SY.nextAt = 0; SY.center = video_time();
  fprintf(stderr, "[sync] checking the subtitle chosen at the start, by itself (at %.0f s)\n", SY.center);
}
static void sync_tick(void) {
  sync_auto_tick(); if (!SY.phase) return;
  if (SY.phase == 2) { SyncResult r; if (!subsync_poll(SY.job, &r)) { int sg = subsync_stage(SY.job); if (sg != SY.stage) { SY.stage = sg; if (!SY.quiet) { snprintf(SY.st, sizeof SY.st, "%s", sync_wait_txt(sg)); refresh_tray(4); } } return; }
    subsync_drop(SY.job); SY.job = NULL;
    if (r.st == SYNC_NOREF) { sync_end(T("Nothing to sync against")); return; }
    if (r.st == SYNC_NOSUB) { sync_end(T("Couldn’t sync")); return; }
    if (r.st != SYNC_OK) { sync_end(T("Not sure: unchanged")); return; }
    double at = video_time(), sh = at * (r.f - 1) + r.d; /* the shift where it is watched (a speed fix moves it along the film) */
    if (SY.quiet && (P.subDelay != 0 || (fabs(sh) < 0.5 && fabs(r.f - 1) < 0.0005))) { fprintf(stderr, "[sync] (by itself) %s: left as it is\n", P.subDelay != 0 ? "a delay was set meanwhile" : "already in sync"); sync_end(""); return; }
    if (!SY.base || SY.nbase != P.ncues) { free(SY.base); SY.base = malloc((size_t)(P.ncues ? P.ncues : 1) * 2 * sizeof *SY.base); SY.nbase = SY.base ? P.ncues : 0;
      for (int i = 0; i < SY.nbase; i++) { SY.base[2 * i] = P.cues[i].s; SY.base[2 * i + 1] = P.cues[i].e; } }
    if (!SY.base) { sync_end(T("Couldn’t sync")); return; }
    for (int i = 0; i < P.ncues; i++) { P.cues[i].s = SY.base[2 * i] * r.f + r.d; P.cues[i].e = SY.base[2 * i + 1] * r.f + r.d; } cueTimesGen++; SY.f = r.f; SY.d = r.d;
    P.subDelay = 0;
    char n[24]; snprintf(n, sizeof n, "%s%.1f", sh >= 0 ? "+" : "", sh); char st[160]; snprintf(st, sizeof st, "%s %s%s%s", T("Synced"), T1("{n} s", "n", n), fabs(r.f - 1) > 0.001 ? "  ·  " : "", fabs(r.f - 1) > 0.001 ? T("speed fixed") : "");
    int q = SY.quiet; SY.quiet = 0; sync_end(st); toast(q ? T1("Subtitles synced {n} s", "n", n) : SY.st, 3000, NULL); return; }
  if (now_() < SY.nextAt) return; SY.nextAt = now_() + 0.5;
  /* waiting for the file's own lines near the playhead */
  int st = mkv ? mkv_state(mkv) : MKV_FALLBACK, listed = mkv && st != MKV_FALLBACK && mkv_listed(mkv);
  if (st == MKV_FALLBACK) { sync_by_ref("not a file whose tracks can be read"); return; }
  MkvTrack tr[16]; int nt = listed ? mkv_tracks(mkv, tr, 16) : 0, ntext = 0;
  double lo = SY.center - SYNC_BEHIND_S, hi = SY.center + SYNC_AHEAD_S; int best = -1, bestN = 0;
  for (int i = 0; i < nt; i++) { if (!tr[i].text) continue; ntext++; int c = mkv_lines_in(mkv, tr[i].n, lo, hi, NULL, 0, NULL, 0); if (c > bestN) { bestN = c; best = tr[i].n; } }
  if (listed && !ntext) { sync_by_ref("no text track in the file"); return; } /* the file has no text tracks at all */
  if (bestN >= 12) {
    int cap = bestN < 3000 ? bestN : 3000, tcap = cap * 96; MkvLine *ml = malloc((size_t)cap * sizeof *ml); char *tb = malloc((size_t)tcap); SyncLine *rl = malloc((size_t)cap * sizeof *rl);
    Cue *xs = sync_xs();
    if (!ml || !tb || !rl || !xs) { free(ml); free(tb); free(rl); free(xs); sync_end(T("Couldn’t sync")); return; }
    int k = mkv_lines_in(mkv, best, lo, hi, ml, cap, tb, tcap); if (k > cap) k = cap;
    for (int i = 0; i < k; i++) { rl[i].s = ml[i].s; rl[i].e = ml[i].e; rl[i].t = ml[i].t; }
    fprintf(stderr, "[subsync] reference: track #%d, %d lines near %.0f s after %.1f s\n", best, bestN, SY.center, now_() - SY.t0);
    SY.job = subsync_start(xs, P.ncues, rl, k, SY.center); free(ml); free(tb); free(rl); free(xs); /* (it keeps its own copies) */
    if (!SY.job) { sync_end(T("Couldn’t sync")); return; }
    SY.phase = 2; return; }
  /* its own tracks are the best reference by far: while they are being read keep waiting, up to 45 s */
  double limit = listed && st == MKV_HARVESTING ? 45 : 12;
  if (now_() - SY.t0 > limit) { fprintf(stderr, "[subsync] reference: %d lines near %.0f s after %.0f s\n", bestN, SY.center, now_() - SY.t0);
    if (ntext) sync_end(T("Still reading the film’s subtitles: try again in a minute")); else sync_by_ref("the file's tracks were not read in time"); }
}

