#include "core/multi.h"
#include "screens/player.h"
#include "core/traktwatch.h"
#include "screens/board.h"
#include "core/scrobble.h"
#include "core/insight.h"
#include "core/subtr.h"
#include "core/organize.h"
#include "core/tracking.h"
#include "core/live.h"
#include "screens/livepeek.h"
#include "screens/livehub.h"
#include "ui/spoil.h"
#include "ui/sound.h"
#include "core/app.h"
#include "core/keys.h"
#include "core/i18n.h"
#include "core/json.h"
#include "core/net.h"
#include "core/store.h"
#include "core/subs.h"
#include "core/forcedsubs.h"
#include "core/video.h"
#include "ui/shared.h"
#include "ui/gfx.h"
#include "ui/text.h"
#include "ui/img.h"
#include "ui/icon.h"
#include "ui/widgets.h"
#include "ui/prompts.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include <time.h>
#include "core/p2p.h"
#include "core/waitlines.h"
#include "platform/plat.h"
#include "ui/xline.h"
#include "core/prevspike.h"
#include "core/seekprev.h"
#include "core/mkvsubs.h"
#include "core/best.h"
#include "core/trailer.h"
#include "core/tmdb.h"
#include "screens/diary.h"
#include "core/audorig.h"
#include "core/parental.h"
#include "ui/glass.h"
#include "screens/splash.h"
#include "core/rating.h"
#include "core/hproxy.h"
#include "core/mp4chap.h"
#include "core/coll.h"
#include "core/addons.h"
#include "core/subsync.h"
#include "core/prevsrc.h"
#include "ui/subfonts.h"
#include "ui/asssubs.h"

/* ---- the stream's text, read for the info chips: resolution, HDR, audio ---- */
static void lower(const char *s, char *out, size_t cap) { size_t i = 0; for (; s[i] && i + 1 < cap; i++) out[i] = (char)tolower((unsigned char)s[i]); out[i] = 0; }
static int has_word(const char *s, const char *w) { const char *p = s; size_t n = strlen(w); while ((p = strstr(p, w))) { int a = p == s || !isalnum((unsigned char)p[-1]), b = !isalnum((unsigned char)p[n]); if (a && b) return 1; p += n; } return 0; }
static const char *res_of(const char *label) {
  char s[640]; lower(label, s, sizeof s);
  if (strstr(s, "2160p") || has_word(s, "4k") || has_word(s, "uhd")) return "4K"; if (strstr(s, "1440p") || has_word(s, "2k")) return "1440p"; if (strstr(s, "1080p") || has_word(s, "fhd")) return "1080p";
  if (strstr(s, "720p")) return "720p"; if (strstr(s, "576p") || strstr(s, "480p") || has_word(s, "sd")) return "SD"; return "";
}
static const char *res_of_video(int w, int h) { if (!w || !h) return ""; if (w >= 3200 || h >= 1800) return "4K"; if (w >= 2400 || h >= 1300) return "1440p"; if (w >= 1700 || h >= 950) return "1080p"; if (w >= 1150 || h >= 650) return "720p"; return "SD"; }
static void hdr_of(const char *label, char *out, size_t cap) {
  char s[640]; lower(label, s, sizeof s); for (char *c = s; *c; c++) if (*c == '.' || *c == '_') *c = ' ';
  int dv = has_word(s, "dv") || has_word(s, "dovi") || strstr(s, "dolby vision"); out[0] = 0;
  if (dv) snprintf(out, cap, "DV");
  const char *h = strstr(s, "hdr10+") || strstr(s, "hdr10 plus") || has_word(s, "hdr10p") ? "HDR10+" : has_word(s, "hdr10") ? "HDR10" : (has_word(s, "hdr") && !dv) ? "HDR" : has_word(s, "hlg") ? "HLG" : "";
  if (*h) { size_t l = strlen(out); snprintf(out + l, cap - l, "%s%s", l ? " · " : "", h); }
}
static void audio_of(const char *label, char *out, size_t cap) {
  char s[640]; lower(label, s, sizeof s); for (char *c = s; *c; c++) if (*c == '.' || *c == '_') *c = ' ';
  const char *au = "";
  if (strstr(s, "truehd")) au = "TrueHD"; else if (strstr(s, "dts-x") || strstr(s, "dts x") || strstr(s, "dtsx")) au = "DTS:X"; else if (strstr(s, "dts-hd ma") || strstr(s, "dts-hd-ma") || strstr(s, "dts hd ma") || strstr(s, "dts-ma") || strstr(s, "dtshd ma")) au = "DTS-HD MA";
  else if (strstr(s, "dts-hd") || strstr(s, "dts hd") || strstr(s, "dtshd")) au = "DTS-HD"; else if (has_word(s, "dts")) au = "DTS";
  else if (has_word(s, "ddp") || strstr(s, "dd+") || strstr(s, "e-ac-3") || strstr(s, "e-ac3") || strstr(s, "eac3") || strstr(s, "dolby digital plus")) au = "EAC3"; else if (has_word(s, "dd") || strstr(s, "ac-3") || has_word(s, "ac3") || strstr(s, "dolby digital")) au = "AC3";
  else if (has_word(s, "aac") || strstr(s, "aac2")) au = "AAC"; else if (has_word(s, "opus")) au = "Opus"; else if (has_word(s, "flac")) au = "FLAC"; else if (has_word(s, "mp3")) au = "MP3";
  char ch[8] = ""; for (const char *p = s; *p; p++) if (p[0] >= '1' && p[0] <= '9' && (p[1] == '.' || p[1] == ' ') && (p[2] == '0' || p[2] == '1') && !isdigit((unsigned char)p[3]) && (p == s || (!isdigit((unsigned char)p[-1]) && p[-1] != '.'))) { snprintf(ch, sizeof ch, "%c.%c", p[0], p[2]); break; }
  int atmos = strstr(s, "atmos") != NULL;
  if (*au && atmos) snprintf(out, cap, "%s Atmos%s%s", au, ch[0] ? " " : "", ch); else if (!*au && atmos) snprintf(out, cap, "Atmos"); else if (*au) snprintf(out, cap, "%s%s%s", au, ch[0] ? " " : "", ch); else out[0] = 0;
}
static void fmt_time(double s, char *out, size_t cap) { int t = s < 0 ? 0 : (int)s, h = t / 3600, m = (t % 3600) / 60, x = t % 60; if (h) snprintf(out, cap, "%d:%02d:%02d", h, m, x); else snprintf(out, cap, "%d:%02d", m, x); }
static const char *lang_name(const char *c) { const char *n = subs_lang_name(c); return n ? T(n) : ""; }
#define lang_match subs_lang_match

/* ---- motion: the page's CSS transitions (duration, cubic-bezier, delay), retargeted from where they are ---- */
static const float C_EASE[4] = { .25f, .1f, .25f, 1 }, C_OUT[4] = { .2f, .8f, .2f, 1 }, C_SEEK[4] = { .23f, 1, .32f, 1 }, C_TRIN[4] = { .2f, .9f, .25f, 1 },
  C_TROUT[4] = { .4f, 0, 1, 1 }, C_LGIN[4] = { .4f, 0, .2f, 1 }, C_INOUT[4] = { .42f, 0, .58f, 1 };
static float bez(const float *c, float x) {
  if (x <= 0) return 0; if (x >= 1) return 1; if (!c) return x;
  float t = x; for (int i = 0; i < 8; i++) { float u = 1 - t, b = 3 * u * u * t * c[0] + 3 * u * t * t * c[2] + t * t * t - x, d = 3 * u * u * c[0] + 6 * u * t * (c[2] - c[0]) + 3 * t * t * (1 - c[2]);
    if (fabsf(b) < 1e-4f || fabsf(d) < 1e-6f) break; t -= b / d; if (t < 0) t = 0; if (t > 1) t = 1; }
  float u = 1 - t; return 3 * u * u * t * c[1] + 3 * u * t * t * c[3] + t * t * t;
}
typedef struct { float from, to, v, t, dur; const float *c; int on; } Mo;
static void mo_set(Mo *m, float v) { m->from = m->to = m->v = v; m->on = 0; }
static void mo_to(Mo *m, float to, float dur, const float *c, float delay) {
  if (m->to == to && (m->on || m->v == to)) return;
  m->from = m->v; m->to = to; dur *= ANIM_SPEED; delay *= ANIM_SPEED; m->t = -delay; m->dur = dur > 0 ? dur : 1e-4f; m->c = c; m->on = 1; if (dur <= 0 && delay <= 0) { m->v = to; m->on = 0; }
}
static void mo_tick(Mo *m, float dt) { if (!m->on) return; m->t += dt; if (m->t < 0) return; float x = m->t / m->dur; if (x >= 1) { m->v = m->to; m->on = 0; return; } m->v = m->from + (m->to - m->from) * bez(m->c, x); }
/* a keyframe entrance started at `at` (0 = none running): 0..1 */
static float kf(double at, float dur, const float *c, float delay) { if (!at) return 1; return bez(c, (float)((app_now() - at - delay * ANIM_SPEED) / (dur * ANIM_SPEED))); }

/* ---- state ---- */
#define MAXBTN 16 /* etapa-92: 11 now (a series with -10 / +10 and Other player): 8 ran over and closed the app */
#define MAXROW 64
typedef struct { int k; /* 0 note 1 skel 2 ep 3 src 4 chip; ls: a subtitle row's list, 1 OpenSubtitles 2 the addons' */ char lb[200], sub[120], ov[300], th[512], vl[48]; int on, spin, tool, hid, grey; int i, ls; const cJSON *v; Stream *st; char u[1024]; float d, hh, ww, vw; uint32_t sw; int vwt; } TItem; /* vwt: the value's weight (0: 700), a font row's in its own font */ /* hh, ww, vw: measured on first draw (items are made fresh, zeroed) */
typedef struct { char title[64], vl[32]; TItem *items; int n; } TCol;
#define SRC_G 32 /* addons the sources tray lists (each its tab; it was 16, more were left out) */
static float fill_ease(double t);
typedef struct { int k; /* 0 none 1 intro 2 recap 3 outro */ int big, nbtn, idx, autoN; double t0, adur; struct { const char *id; char lb[48]; double to; int main; } b[2]; } PPrompt;
static struct {
  int open, closing, live, failed, started; double openedAt;
  char type[16], id[SB_ID], vid[SB_VID], name[256], logo[512], bg[512], poster[512], sname[600], saddon[96], url[SB_URL_MAX], alt[SB_URL_MAX]; int altTried, p2pRetried; double lastPos; int quiet, adopt, isTrailer, isHls, liveStream, hlsVod; /* liveStream: its HLS playlist has no end (a live broadcast); hlsVod: it has an end (a replay) */
  cJSON *meta; const cJSON *videos; int season, episode; char epTitle[256], year[8], genre[64];
  struct { int has; char vid[SB_VID], se[32], title[256], thumb[512], ov[600]; } next; int sim; double startAt;
  struct { int has, tried; double at; char id[16], name[160], poster[400], bg[400], part[64]; } coll; /* a film in a collection: the next one */
  int mode; /* 0 video 1 osd 2 tray 3 prompt */ int row; /* 0 btns 1 bar */ int bIdx;
  struct { const char *id; const char *icon; char lb[48]; int big, nx; float x0, x, y, w, lw; Mo dx; } btns[MAXBTN]; int nbtn, nL;
  /* the page's classes as motions: the title block, bar and buttons, the clock, the shades, the paused label, the spinner,
     the loading line, the seek pill, the knob, the prompt, the subtitles' lift */
  Mo in, artA, ttlA, ttlX, ttlY, barA, barY, btnA, btnY, topR, topY, dim, dimT, vig, pzA, bufA, lnA, skA, skS, knA, prA, prY, prB, subB, ccS, pgA, pgY, pwA, pwF;
  struct { ParGuide g; int age, certAge, guideIn, certIn, asked; unsigned sess; double at, askAt; char cert[24], desc[120]; } pg; /* the viewer notice: an age and what for, as the picture comes, gone in 5 s */
  Mo posA; double posAt; int styK; /* the subtitle style card (mode 5): its showing, when the value last changed, which one (0 position, 1 size, 2 colour, 3 background, 4 brightness) */
  Mo dlyA, dlyR; int dlyF, dlyKind; double dlyAt, audDelay; float speed; /* dlyKind: 0 the subtitles' delay, 1 the sound's (audDelay, s); speed: 0 = normal */
  /* the delay card (mode 4): its showing, its focus ring's place, the focused button, when the value last changed */
  double hideAt; int osd;
  /* the info chips: kept per place, each with its own entrance */
  struct { char t[100], old[100]; const char *icon; int bold, sep; double at, xAt; float delay, oldW; } chip[8]; int nchip; /* old, xAt, oldW: the words before a change, crossfading */
  double seekSnapAt, seekSnapT; int seekScrub; /* seekScrub: the target is a scrub of the viewer's (keys, the pointer), the only seek ever snapped */ /* (SB_SEEKSNAP: when the last seek was sent, for its time to a picture) */
  double seekT, seekCommitAt, lastKeyAt; int seekDir, rep, skm, skOn; char seekTxt[48]; double seekTxtUntil;
  /* the focus pill under the buttons and its name */
  Mo focX, focY, focW, focH, focA, tipA; char tip[48]; double tipAt, focFadeAt; int focSeen;
  int ldd, exitFade; char trName[128], p2pLine[160]; Mo p2pA; struct { int on, held, target, shown, ns, etaShown, made, slips; double heldAt, holdT, repAt, t0, eta, gainAt, lastF, shownAt, shownF, sAt[40], sF[40]; } pw; /* shown: the wait known to be needed (its line, bar and Play now up); sAt/sF: the progress every 0.5 s, for the pace of the last 15 s */ struct { int on, v; double t0; } ps; /* ps: the seek previews from a lighter copy (core/prevsrc); v 0 not checked, 1 checking, 2 the same cut */ /* pw: previews first (the setting): the film loaded and held on its first frame behind the art while most of its previews are made */
  
  /* prompts (shown: the one drawn, kept while it fades out) */ PPrompt prompt, shown; int dismissed[4];
  struct { int has; double s, e; } intro, recap, outro; int skipsAsked, skipsOk;
  /* the skip sources being asked (merged by rank when they have answered) */
  struct { unsigned tok; int pending, sent, nsrc, isEp, se, en, chapWait; double quickAt, endAt, d; struct { char url[440]; int kind, tries, live; double retryAt; } job[3]; struct { int rank; struct { int has; double s, e; } g[3]; } src[4]; } sk;
  /* next episode from the closest source */
  int nextTried; struct { unsigned tok; int done, has; double bs, until; char vid[SB_VID], info[700], url[SB_URL_MAX], addon[96], binge[96]; Stream best; } nx;
  int ended; double endGoAt; double metaRetryAt;
  /* the paused screensaver, the file size, the picture size */
  double liveT0, steadyAt; /* the first frame came / playing steadily since */ double vidAt; /* the stream asked for (a channel with no picture 12 s on: its next stream) */
  int ssOn; double ssAt, ssCheckAt, lastAct; double size; char sizeTxt[24]; unsigned sizeTok; int httpSt, sizeAns, canNext; double errAt; int lastVw, lastVh; double aspRefreshAt;
  /* the pointer: the time bubble over the bar */ int hvOn, barDown; float hvP; Mo hvA;
  int extTry, embKnown, embWaitOver, sameWaitOver; double extRetryAt, embWaitAt, sameWaitAt; /* sameWait: a show's addon file, its addon's list awaited (10 s at most) */
  /* the stream as its addon gave it (headers, group, file facts); the subtitles addons installed for this video, the
     language chosen in their tab */
  Stream st; int nSubAddons; char subLang[24]; unsigned ytTok;
  /* the addons' subtitles (the stream's own, then each subtitles addon's), apart from the built-in list in ext */
  SubEntry *adx; int nAdx, adxLoading, adxTry; double adxRetryAt; unsigned adxReq;
  /* tray */ struct { int kind; /* 0 none 1 eps 2 src 3 aud 4 subs 5 epsrc */ char tabs[40][48]; int ntabs, tab, seasonList[40], season; int zone; /* 0 row 1 tabs 2 cols */ TItem row[MAXROW]; int n, idx, onBtn; TCol cols[4]; int ncols, c, r[4]; char title[300], epvid[SB_VID]; int backIdx, backTab; Mo open, openA; float ox, oy; Mo rowX, tabsX, colY[4], ringX, ringY, ringA, plY[4], plA[4]; float ringW, ringH; int loading; int armed; } tray;
  /* the sources tray's lists: one per addon serving the video (in the user's order), all asked as it opens; each stream
     read once as it comes (best_facts), the best fit for this TV among them all */
  struct { char vid[SB_VID]; struct { char name[96]; Stream *items; BestFacts *f; int n, st, nf, came; unsigned tok; Mo ia; } g[SRC_G]; int ng, on, bg, bi; } srcs; /* st: 0 not asked, 1 asked (or came, its streams still being read: nf of n, came set), 2 shown, 3 came empty; ia: its rows fading in as they come; bg/bi: the best fit (-1 none) */
  /* subs */ SubEntry *ext; int next_; int nExt, extLoading; char curSub[1100]; struct { int list; char addon[64], lang[24]; } pick; Cue *cues; int ncues; double subDelay; VidTrack emb[16]; int nEmb; int autoSubDone; unsigned subReq; unsigned trSubTok; int locDone;
  int curAud; int autoAudDone; int didResume;
  /* sound this TV can't decode (DTS, TrueHD): when the DTS track started playing, the codecs refused on this stream (error
     200), what was said */ struct { double playAt; char bad[4][16]; int nbad, said, gaveUp, switches;
    /* the sound converted on this device (hproxy): 0 no, 1 opening, 2 playing it; the file's audio track converted, the
       moment to come back to, since when, the picture is HEVC; a first load held back until the file's tracks are read */
    int conv, convOrd, hevc; double convAt, convT0, okAt, deferUntil; char outDesc[24]; /* outDesc: what the TV gets ("AC3 5.1"), for Stats for nerds */
    /* the same conversion for a Dolby Vision profile 7 picture: dv the picture goes out as 8.1, copyA the audio track as
       it is, dvTried started once already, dvSeen what the TV showed was checked */
    int dv, copyA, dvTried, dvSeen; double dvAt; } dts; /* dv: the Dolby Vision profile going through (7 as 8.1; 8, 5 as they are), 0 none */
  struct { double s; char nm[96]; } chap[64]; int nchap; int chLast; double chAt; int chapGen, mp4Asked; double chapReadyAt; /* the file's chapters (nm "" when only numbered); the one the pill last named, when it changed */
  int epState; double played, playTick, pzSince; /* the series' place: 0 not decided, 1 this episode is it, 2 an earlier one (it becomes the place once watched a while), 3 a special (never); played: seconds really watched */
  int assPlain; /* styled subtitles were too slow here: plain for the rest of this film */ double lastSave; int marked; char aspect[8]; const char *res; char infoKey[256]; int pausedShown; double pausedAt;
  /* torrents: resolved by the engine first; the file's own subtitles and chapters from the stream itself */
  int isP2p, resolving; double posTick; int mkvTracksOk, chapWant;
  Nudge nudge; double nameAt;
} P;
static MkvSession *mkv;            /* the subtitle reader of the stream playing (outside P: P starts over per source) */
static unsigned p2pTok;
static void close_player(int goNext, int goSim);
static int closeColl; static Item closeCollIt; /* closing for the next film of a collection: its page */
static void show_osd(int keepFocus);
static void hide_osd(void);
static void render_tray(int fresh);
static void tray_origin(void);
static double now_(void) { return app_now(); }
/* where a looping motion (a spinner, the loading line) is in its turn: worked out in double precision, as a float clock
   steps visibly once the app has been running a while */
static float clock_ph(double period) { return (float)fmod(now_() / period, 1.0); }
int player_open(void) { return P.open; }
int player_art_full(void) { return P.open && P.artA.v > 0.98f; } /* the loading art covers the screen */
float player_cover(void) { return P.open ? P.in.v : 0; }
/* the page under the player: while it covers it only partly, and while a logo flies between them over the art
   (the page's copy is the one that flies back) */
static double growAt, shrinkAt; static int keepVideo, adoptFull; /* adoptFull: taken full screen from behind All channels (no grow, no shrink) */
/* a preview growing to the screen (lead-and-cover): the page round it fades to black (never see-through: the plane
   can't show past the hole), the TV is told the picture's place ahead of the hole (leadMs, one call every stepMs,
   each edge only outward while it grows, inward while it shrinks) so the picture always covers the hole, and the
   hole is inset a little mid-move; the page's picture is taken before the move starts (growPending) */
static int growPending; static float growRad = 26, planeSent[4], growR[4]; /* growR: the box it grows from */ static double planeAt; static int growFrames, growCalls; static float growWorst;
#define GROW_D 0.62
#define SHRINK_D 0.55
static void grow_reverse(void); static float grow_k(void); static void finish_close(void);
int player_page_under(void) { return P.open && (P.in.v < 0.999f || (sx_flying() && !P.live) || growPending || growAt > 0 || shrinkAt > 0); } /* (and while a preview grows to the screen, or back into its box) */ /* (and while a preview grows to the screen) */
int player_live(void) { extern int app_bg_video(void); return (P.open && P.live && !P.closing && P.artA.v < 0.02f) || app_bg_video(); } /* (or the boards' background trailer) */
static int paused(void) { return video_state() == VID_PAUSED; }
static int awayHold; /* (#69) a film paused because the app went behind another app: held paused until the app is in front, or a play is asked (a phone's) */
int player_paused(void) { return P.open && !P.closing && P.live && paused(); } /* (the app's screensaver may come over a paused film) */
static double dur(void) { return video_duration(); }
/* a live broadcast (a channel, a match): no end to count down to, so the bar says LIVE instead of the time left */
/* live by its type: a channel always; a match or an event (sport, events) while its HLS playlist has not said it ends */
static int live_kind(void) { return !strcmp(P.type, "tv") || !strcmp(P.type, "channel") || ((!strcmp(P.type, "sport") || !strcmp(P.type, "events")) && P.isHls && !P.hlsVod); }
static int live_bcast(void) { double d = dur(); return P.live && (P.liveStream || !(d > 0) || d > 1e7 || live_kind()); }
static int live_title(void) { return P.liveStream || live_kind(); } /* known before it plays: nothing kept, nothing resumed */
/* whether an HLS stream is live: its playlist (the first variant of a master one) has no end. Read once as it starts;
   the TV's own length for a live stream is its window's, so a broadcast looked like a short film (no LIVE, a seek
   bar, a place in Continue Watching, a resume into a moment already gone) */
static unsigned liveTok; static char liveBase[SB_URL_MAX]; static int liveDepth;
static void on_live_pl(int st, const unsigned char *b, size_t n, void *ud) {
  (void)n; if ((unsigned)(uintptr_t)ud != liveTok || !P.open || st != 200 || !b) return; const char *s = (const char *)b;
  const char *inf = strstr(s, "#EXT-X-STREAM-INF");
  if (inf && liveDepth < 2) { const char *l = strchr(inf, '\n'); while (l && (l[1] == '#' || l[1] == '\r' || l[1] == '\n')) l = strchr(l + 1, '\n'); /* the variant under it */
    if (!l) return; l++; char v[2048]; size_t k = strcspn(l, "\r\n"); if (k >= sizeof v) return; memcpy(v, l, k); v[k] = 0;
    char abs[SB_URL_MAX]; if (strstr(v, "://")) snprintf(abs, sizeof abs, "%s", v);
    else { const char *q = strpbrk(liveBase, "?#"), *e = q ? q : liveBase + strlen(liveBase), *sl = e; while (sl > liveBase && sl[-1] != '/') sl--; snprintf(abs, sizeof abs, "%.*s%s", (int)(sl - liveBase), liveBase, v); }
    snprintf(liveBase, sizeof liveBase, "%s", abs); liveDepth++; net_get(abs, on_live_pl, ud); return; }
  if (!strstr(s, "#EXTM3U")) return;
  int vod = strstr(s, "#EXT-X-ENDLIST") || strstr(s, "PLAYLIST-TYPE:VOD");
  if (vod) P.hlsVod = 1; /* (a replay of a match: a film again) */
  else { P.liveStream = 1; video_set_live(1); fprintf(stderr, "[player] a live broadcast (its playlist has no end)\n"); }
}
/* a live broadcast that ends or errors while it plays: loaded again, at its live edge, instead of closing; six times
   in two minutes at most, then as any stream */
#define LIVE_RELOADS 6
static double liveRe[LIVE_RELOADS];
static int live_reload(const char *why) {
  if (!P.live || P.isTrailer || P.failed || !live_title()) return 0;
  double now = now_(); int old = 0, recent = 0;
  for (int i = 0; i < LIVE_RELOADS; i++) { if (now - liveRe[i] < 120) recent++; if (liveRe[i] < liveRe[old]) old = i; }
  if (recent >= LIVE_RELOADS) { fprintf(stderr, "[player] live: %s again, %d reloads in 2 min: giving up\n", why, recent); return 0; }
  liveRe[old] = now; fprintf(stderr, "[player] live: %s, back to its live edge (%d/%d)\n", why, recent + 1, LIVE_RELOADS);
  video_load(P.altTried && P.alt[0] ? P.alt : P.url); if (P.liveStream) video_set_live(1); P.errAt = 0;
  return 1;
}
static void live_probe(void) { if (!P.isHls || P.isTrailer) return; liveDepth = 0; snprintf(liveBase, sizeof liveBase, "%s", P.url); net_get(P.url, on_live_pl, (void *)(uintptr_t)++liveTok); }

/* ---- the art screen: the title's logo breathing over its cover, a loading line, a status ---- */
static char status[400];
/* a line of status words: it fades and slides a few px as it comes and goes, and crossfades when its words change */
static XLine stX[3];
/* a light line over the loading text, sparingly (core/waitlines: what fits the moment, at most 3 a wait, each only once
   the real text has stayed the same 4 s and only while it is a calm one; never once the picture runs, never on a
   failure, never over a number, an addon or a reason) */
static unsigned loadGen; /* bumps with each new stream (prepare) */
static struct { int binge; char vid[SB_VID]; int firstToday, film, resume; unsigned gen; } WA; /* the viewer's moment, in memory only */
static int calm_step(const char *st, WlStep *step) { /* the calm texts a line may cover, and the wait they are */
  if (!*st || !strcmp(st, T("Finding the closest source\xe2\x80\xa6")) || !strcmp(st, T("Tuning in\xe2\x80\xa6"))) { *step = WL_FIND; return 1; } /* (a chosen stream or a channel starting: humour only) */
  if (!strcmp(st, T("Choosing the best stream"))) { *step = WL_CHOOSE; return 1; }
  if (!strcmp(st, T("Finding peers\xe2\x80\xa6"))) { *step = WL_PEERS; return 1; }
  if (!strcmp(st, T("Getting the file list\xe2\x80\xa6"))) { *step = WL_FILES; return 1; }
  if (!strcmp(st, T("Waiting for the next pieces\xe2\x80\xa6"))) { *step = WL_PIECES; return 1; }
  return 0;
}
static void moment(void) { /* once a wait: a film, a resume, the nth episode in a row, the first episode today (one local date in the store) */
  if (WA.gen == loadGen) return; WA.gen = loadGen;
  int series = !strcmp(P.type, "series"); WA.film = !strcmp(P.type, "movie"); WA.resume = P.startAt > 1; WA.firstToday = 0;
  if (!series) { if (WA.film) { WA.binge = 0; WA.vid[0] = 0; } return; }
  if (strcmp(WA.vid, P.vid)) { WA.binge = WA.vid[0] ? WA.binge + 1 : 1; snprintf(WA.vid, sizeof WA.vid, "%s", P.vid); /* (another source of the same episode: not one more) */
    time_t tt = time(NULL); struct tm lt; localtime_r(&tt, &lt); char d[16]; strftime(d, sizeof d, "%Y-%m-%d", &lt);
    if (strcmp(store_get("wlDay", ""), d)) { WA.firstToday = 1; store_set("wlDay", d); } }
}
static const char *loading_line(const char *st) {
  static WlSlot slot; moment();
  WlStep step = WL_FIND; int calm = !P.live && !P.failed && calm_step(st, &step);
  return xline_wait(&slot, loadGen, st, calm, step, WA.resume ? WL_ACT_RESUME : WA.firstToday ? WL_ACT_FIRSTTODAY : WA.film ? WL_ACT_FILM : WL_ACT_NONE, WA.binge);
}
/* a status stays long enough to be read (1.2 s and a little more per letter, 3 s at most) before the next one takes its
   place; a failure, the same words with other digits (a counter) and a new play's first words come at once */
static char statusNext[400]; static double statusAt; static int statusPend;
static double status_hold(void) { double h = status[0] ? 1.2 + 0.035 * strlen(status) : 0; return h > 3 ? 3 : h; }
static void status_apply(const char *t) { snprintf(status, sizeof status, "%s", t); statusAt = now_(); statusPend = 0; }
static void set_status(const char *t) { const char *n = t ? t : ""; if (!strcmp(n, status)) { statusPend = 0; return; }
  if (P.failed || !P.open || P.live || wl_same_but_digits(n, status) || now_() - statusAt >= status_hold()) { status_apply(n); return; } /* (held only on the loading screen: never into the film) */
  snprintf(statusNext, sizeof statusNext, "%s", n); statusPend = 1; }
static void status_tick(void) { if (statusPend && (P.live || now_() - statusAt >= status_hold())) status_apply(statusNext); }
/* a heavy film (a remux, 4K, HDR or Dolby Vision, over 12 GB): its seek previews from a lighter copy of it, once
   that copy is proven the same cut; until then (25 s at most) none of the film's own are read, after that the film's
   own make them and a copy proven later still takes over */
static int film_heavy(void) { char t[2048], l[2048]; best_stream_info(&P.st, t, sizeof t); size_t i = 0; for (; t[i] && i < sizeof l - 1; i++) l[i] = (char)tolower((unsigned char)t[i]); l[i] = 0;
  char hd[32]; best_hdr_of(t, hd, sizeof hd); const char *rs = best_res_of(t); int res = rs ? atoi(rs) : 0;
  return hd[0] || res >= 1440 || strstr(l, "2160") || strstr(l, "remux") || P.st.size > 12e9 || P.size > 12e9; }
static int ps_wanted(void) { double fs = P.st.size > 0 ? P.st.size : P.size; return fs >= 20e9 && prev_enabled() && prev_available() && !P.adopt && !P.isTrailer && !P.isHls && !P.isP2p && !strncmp(P.url, "http", 4) && !live_title(); } /* (a film of 20 GB or more, a remux: a copy 2.5x lighter makes its pictures once proven the same cut; any other its own) */
static MkvSession *prev_src_pick(void) {
  if (!P.ps.on || !mkv) return mkv;
  MkvSession *cs = ps_session();
  if (P.ps.v == 2) { if (cs) return cs; P.ps.v = 0; }
  int calm = P.pw.on || paused() || (video_state() == VID_PLAYING && video_buffered_end() - video_time() >= 10); /* (the check reads a few of the film's own keyframes: never while it starts or buffers) */
  if (P.ps.v == 0 && cs && calm && mkv_key_count(mkv) >= 20 && mkv_key_count(cs) >= 20) { if (sp_verify_start(mkv, cs)) { P.ps.v = 1; fprintf(stderr, "[player] checking the lighter copy is the same cut\n"); } }
  if (P.ps.v == 1) { double off = 0; int r = sp_verify_state(&off); if (r == 1) { sp_source_offset(off); P.ps.v = 2; return cs; } if (r == -1 || r == -2) { P.ps.v = 0; ps_next(); } }
  if ((ps_busy() || P.ps.v == 1 || (cs && P.ps.v == 0)) && now_() - P.ps.t0 < 25) return NULL; /* (the copy coming or being checked: the film's own wait) */
  return mkv; }
/* previews first (Settings > Playback, Off or 50 % to 100 %): a direct or debrid film starts once that much of it has
   its seek previews; it loads meanwhile and waits on its first frame behind the art, so OK (Play now) or the share
   reached starts it at once. The time left is estimated from the pace so far. It gives up (the film starts) when no
   picture has come for 20 s, the film has none, or after 4 minutes. */
static int pw_target(void) { const char *v = store_get("seekPreviewsWait", "80"); /* (on by default: 80 %) */ if (!strcmp(v, "true")) return 70; int n = atoi(v); return n >= 50 && n <= 100 ? n : 0; }
static int pw_wanted(void) { return pw_target() > 0 && prev_enabled() && prev_available() && !P.adopt && !P.isTrailer && !P.isHls && !P.isP2p && !strncmp(P.url, "http", 4) && !live_title(); }
__attribute__((weak)) void video_start_silent(int on) { (void)on; } /* (a platform without it: as before) */
static void pw_end(const char *why) { if (!P.pw.on) return; P.pw.on = 0; video_start_silent(0); video_set_volume(100, 300); /* (its sound comes up softly as it starts) */ fprintf(stderr, "[player] previews first: %d %% of %d %% in %.1f s (%s), the film starts\n", sp_progress() < 0 ? 0 : sp_progress(), P.pw.target, now_() - P.pw.t0, why);
  status_apply(""); if (P.pw.held) { if (fabs(video_time() - P.pw.holdT) > 1) { fprintf(stderr, "[player] previews first: back to %.1f s where it waited (it had moved to %.1f s)\n", P.pw.holdT, video_time()); video_seek(P.pw.holdT); } video_play(); } } /* (it starts where it waited, never further on) */
static void pw_tick(void) { if (!P.pw.on) return; VidState pv = video_state(); double now = now_(), el = now - P.pw.t0, pf = sp_progress_f(); int tg = P.pw.target;
  if (P.pw.held && video_settling()) P.pw.holdT = video_time(); /* (the hold follows a jump made while it waits: the resume, which on webOS comes with the same "playing" that set the hold; before, the film went back to 0 s as it started) */
  if (!P.pw.held && pv == VID_PLAYING) { video_pause(); P.pw.held = 1; P.pw.heldAt = now; P.pw.holdT = video_time(); P.pw.repAt = now; fprintf(stderr, "[player] previews first: the film's first frame ready after %.1f s, held at %.1f s\n", el, P.pw.holdT); }
  else if (P.pw.held && now - P.pw.repAt > 0.5 && (pv == VID_PLAYING || video_time() > P.pw.holdT + 0.4)) { video_pause(); P.pw.repAt = now; if (pv != VID_PLAYING && !video_settling()) P.pw.holdT = video_time(); /* (a pause that landed late: it waits there, not paused again every 0.5 s) */ if (P.pw.slips++ < 5) fprintf(stderr, "[player] previews first: the film moved on (%.1f s, its pause not taken): paused again\n", video_time()); } /* (a pause the TV ignored while still starting: never a film running behind the art) */
  if (pv == VID_ERROR || P.failed) { P.pw.on = 0; video_start_silent(0); return; }
  int making = sp_active() || sp_source_on(NULL);
  if (ps_busy() || P.ps.v == 1) P.pw.gainAt = now; /* (a copy still being found or checked counts as coming) */
  { int mk = sp_made() + sp_source_made(); if (mk > P.pw.made) { P.pw.made = mk; P.pw.gainAt = now; } } if (pf > P.pw.lastF + 0.001) { P.pw.lastF = pf; P.pw.gainAt = now; } /* (a picture made counts, even where the share does not move) */
  if (making && pf >= tg) { pw_end(P.pw.shown ? "the share reached" : "already made: no wait"); return; }
  if (making && el > 3 && sp_done()) { pw_end("nothing more to make in the background"); return; } /* (kept on this device: straight in, nothing shown) */
  if (el >= 240) { pw_end("4 minutes"); return; }
  { double d = dur(); if (P.liveStream || (P.pw.held && (d >= 1e7 || (now - P.pw.heldAt > 8 && !(d > 0))))) { /* (live streams never have previews: at once when the TV says its length is endless; with no length at all, 8 s in, when a film's would be known) */ pw_end("a live stream: it has no previews to wait for"); return; } } /* (#81: a broadcast not known as one before it played, its first frame in and no length) */
  if (el > 20 && !P.pw.made && pf <= 0 && now - P.pw.gainAt > 20) { pw_end("no preview made in 20 s"); return; } /* (a lighter copy still looked for counts as coming: gainAt) */ /* (#81: none at all, this TV or this file: the film starts; they still come as it plays) */
  if (el > 10 && !making && !ps_busy() && P.ps.v != 1) { pw_end("no previews for this film"); return; }
  if (el > 12 && now - P.pw.gainAt > 45) { pw_end("no new picture for 45 s"); return; }
  if (!P.pw.shown) { if (!making || pf <= 0 || !P.pw.held) return; /* (once its first pictures are made: never a long 0 %; until then the usual loading screen) */ P.pw.shown = 1; P.pw.shownAt = now; P.pw.shownF = pf; P.pw.ns = 0; fprintf(stderr, "[player] previews first: %.1f %% made, waiting for %d %%\n", pf, tg); } /* (only once it is known a wait is needed: the film ready, its pictures counted) */
  if (!P.pw.ns || now - P.pw.sAt[(P.pw.ns - 1) % 40] >= 0.5) { int i = P.pw.ns++ % 40; P.pw.sAt[i] = now; P.pw.sF[i] = pf; }
  /* the time left from the pace of the last 15 s (the first pictures and the film's own loading left out), shown once
     that pace has held 6 s; it settles and only goes up when clearly wrong */
  double rate = 0; { int n = P.pw.ns < 40 ? P.pw.ns : 40, last = (P.pw.ns - 1) % 40; for (int q = 0; q < n; q++) { int i = (P.pw.ns - 1 - q) % 40; if (now - P.pw.sAt[i] > 15) break; if (now - P.pw.sAt[i] >= 4) rate = (P.pw.sF[last] - P.pw.sF[i]) / (P.pw.sAt[last] - P.pw.sAt[i]); } }
  { static double etaAt; if (rate > 0 && now - P.pw.shownAt >= 10 && pf - P.pw.shownF >= 2 && now - etaAt >= 0.5) { /* (once the pace has held 10 s and 2 points: the first, slow pictures never set it) */ double left = (tg - pf) / rate; /* (twice a second: the last estimate counted down, pulled a quarter towards the new one; up only when 20 % off) */
      if (P.pw.eta <= 0) P.pw.eta = left; else { double e = P.pw.eta - (now - etaAt), to = left < e || left > e * 1.2 ? left : e; P.pw.eta = e * 0.75 + to * 0.25; } etaAt = now; P.pw.etaShown = 1; } }
  char n[8], line[300]; snprintf(n, sizeof n, "%d", pf > 0 && pf < 1 ? 1 : (int)pf); snprintf(line, sizeof line, "%s", T1("Getting the seek bar\xe2\x80\x99s previews ready\xe2\x80\xa6 {n}%", "n", n));
  if (P.pw.etaShown && P.pw.eta > 0 && P.pw.eta <= 900) { double e = P.pw.eta; /* (over 15 minutes: no number at all, never an absurd one) */ char m[16], tl[80]; /* about 5 s, 10 s, ... 55 s; then minutes */
    if (e < 60) { snprintf(m, sizeof m, "%d", (int)(ceil(e / 5) * 5)); snprintf(tl, sizeof tl, "%s", T1("about {n} s left", "n", m)); }
    else { snprintf(m, sizeof m, "%d", (int)ceil(e / 60)); snprintf(tl, sizeof tl, "%s", T1("about {n} min left", "n", m)); }
    size_t l = strlen(line); snprintf(line + l, sizeof line - l, "  \xc2\xb7  %s", tl); }
  status_apply(line); }
static void fail(const char *t) { if (live_title() && !P.isTrailer && P.open && !P.closing && live_failover(P.id, P.st.url[0] ? P.st.url : P.url)) { fprintf(stderr, "[player] %s: the channel's next stream instead\n", t); video_unload(); P.live = 0; P.quiet = 1; return; } /* a channel: another of its streams first */
  fprintf(stderr, "[player] %s | %s %s %s\n", t, P.type, P.id, P.vid); P.failed = 1; if (live_title()) live_reach_set(P.id, -1); /* (a channel that would not play: off air) */ video_unload(); P.live = 0;
  P.canNext = P.st.url[0] && strcmp(P.type, "other") && !P.isTrailer; if (P.canNext) best_skip(P.st.url); /* OK picks the best of the others */
  char s[400]; snprintf(s, sizeof s, "%s  ·  %s", t, P.canNext ? T("OK: try the next stream  ·  Back: close") : T("Press Back to close")); set_status(s); }
/* why a stream did not play, in words: what its server answered, the torrent's peers, then the TV's own codes
   ("202 Media Not Found; 203 AV Type Not Founded") read with what the stream's text says it is */
static int tv_code(const char *tv, int lo, int hi) { for (const char *p = tv; p && *p; p = strstr(p, "; "), p = p ? p + 2 : NULL) { int k = atoi(p); if (k >= lo && k <= hi) return 1; } return 0; }
static const char *fail_reason(void) {
  const char *tv = video_error(); char lab[1100]; snprintf(lab, sizeof lab, "%s %s", P.sname, P.st.filename); for (char *c = lab; *c; c++) *c = (char)tolower((unsigned char)*c);
  const TvCaps *c = best_caps(); int http = !P.isP2p && !strncmp(P.url, "http", 4) && P.sizeTok;
  fprintf(stderr, "[player] why: tv \"%s\", %s %d, peers \"%s\"\n", tv, P.isP2p ? "torrent" : "http", P.httpSt, P.p2pLine);
  if (http && P.httpSt == 0) return T("The stream’s server didn’t answer");
  if (http && (P.httpSt == 401 || P.httpSt == 403)) return T("The stream’s server refused it: check your debrid account");
  if (http && (P.httpSt == 404 || P.httpSt == 410)) return T("The stream’s server can’t find this file");
  if (http && P.httpSt == 429) return T("The stream’s server is busy: try again in a moment");
  if (http && P.httpSt >= 500) return T("The stream’s server has a problem");
  if (http && P.httpSt >= 400) return T("The stream’s server didn’t send the video");
  /* the TV read the file and refused its format (201 / 203 its codec, 204 "Fail To Demultiplex" its container): said as that
     even on a torrent with few peers, which only waits for data, it does not end in these */
  int fmtRefused = tv_code(tv, 201, 201) || tv_code(tv, 203, 204) || strstr(tv, "Codec") || strstr(tv, "Demultiplex");
  if (P.isP2p && !fmtRefused && (!P.p2pLine[0] || atoi(P.p2pLine) < 2)) return T("Not enough peers yet");
  if (strstr(tv, "Network") || tv_code(tv, 300, 399)) return T("The connection to the stream dropped");
  int hevc = has_word(lab, "x265") || has_word(lab, "h265") || strstr(lab, "h.265") || strstr(lab, "h 265") || has_word(lab, "hevc"), ten = strstr(lab, "10bit") || strstr(lab, "10 bit") || strstr(lab, "10-bit") || has_word(lab, "hdr") || has_word(lab, "hdr10");
  char hdr[64]; best_hdr_of(P.sname, hdr, sizeof hdr); const char *res = best_res_of(P.sname);
  /* the format itself is blamed only when the TV said it refused it: a stream that failed otherwise (its server, the
     network, one that never sent a picture) is never put down to its codec by its name (a 10-bit HEVC that plays in
     another app was told "can't play HEVC 10-bit") */
  if (fmtRefused) {
    if (c && has_word(lab, "av1") && !c->av1) return T("This TV can’t play AV1 video");
    if (c && hevc && ten && !c->hdr) return T("This TV can’t play HEVC 10-bit video");
    if (c && strstr(hdr, "DV") && !strstr(hdr, "HDR") && !c->dv) return T("This TV can’t play Dolby Vision video");
    if (c && res && !strcmp(res, "4K") && c->maxRes && c->maxRes < 2160) return T("This TV can’t play 4K video");
    int hdrS = has_word(lab, "hdr") || has_word(lab, "hdr10") || strstr(lab, "hdr10+") || has_word(lab, "hlg");
    if (c && hdrS && !c->hdr) return T("This TV can’t show HDR video"); }
  if (video_audio_unsupported() || (c && ((strstr(lab, "truehd") && !c->truehd) || ((has_word(lab, "dts") || strstr(lab, "dts-hd") || strstr(lab, "dts-x")) && !c->dts))))
    if (tv_code(tv, 200, 200) || tv_code(tv, 203, 203) || video_audio_unsupported()) return T("This TV can’t play this video’s sound (DTS or TrueHD)");
  if (fmtRefused) return T("The video format isn’t supported by this TV");
  if (tv_code(tv, 202, 202)) return T("The TV couldn’t open the video");
  return T("Playback error");
}

/* ---- library / progress ---- */
/* episodes: a series, or any title whose metadata lists videos and where a video (not the title itself) plays (anime
   and channel addons give their own types: "anime", "tv"...; their ids may be "kitsu:123:5", with no season) */
static int episodic(void) {
  if (!strcmp(P.type, "series")) return 1;
  if (!strcmp(P.type, "movie") || !strcmp(P.type, "other") || !P.vid[0] || !strcmp(P.vid, P.id)) return 0;
  const cJSON *v = P.meta ? js_arr(P.meta, "videos") : NULL; return v && cJSON_GetArraySize(v) > 1;
}
static Item lib_item(void) { Item it; memset(&it, 0, sizeof it); snprintf(it.id, sizeof it.id, "%s", P.id); snprintf(it.type, sizeof it.type, "%s", P.type); snprintf(it.name, sizeof it.name, "%s", P.name); snprintf(it.poster, sizeof it.poster, "%s", P.poster); snprintf(it.background, sizeof it.background, "%s", P.bg); snprintf(it.logo, sizeof it.logo, "%s", P.logo); return it; }
/* the credits are reached: the file's or the show's own credits time when known, else the last stretch (about 2 minutes
   of an episode, 5 of a film) or 95 %: from there the title counts as watched */
static int at_credits(void) {
  double d = dur(), t = video_time(); if (!(d > 0) || live_title()) return 0;
  if (P.outro.has && P.outro.s > d * 0.5 && t >= P.outro.s) return 1;
  double tail = episodic() ? fmin(120, d * 0.1) : fmin(300, d * 0.06); return d - t <= tail || t / d > 0.95;
}
/* Continue Watching keeps a series at the latest episode watched: a newer one (or the same) takes the place as soon as
   it plays; an earlier one only once it is really watched (5 minutes, or 40 % of a short one), and then the episodes
   after it are unwatched (watching again from there); a special (season 0) never moves it */
#define BACK_S 300.0
static void ep_decide(void) {
  if (P.epState) return; if (!P.videos && P.vid[0] && strcmp(P.vid, P.id) && P.played < 20) return; /* (the episode list still coming: up to 20 s of playback, then as before) */
  P.epState = 1; if (!episodic() || !P.videos) return;
  int sn, sp; int rn = lib_ep_rank(P.videos, P.vid, &sn); if (sn == 0) { P.epState = 3; return; }
  cJSON *l = lib_get(P.id); const char *cur = js_str(js_obj(l, "state"), "video_id", ""); if (!*cur || !strcmp(cur, P.vid)) return;
  int rp = lib_ep_rank(P.videos, cur, &sp); if (rn >= 0 && rp >= 0 && sp != 0 && rn < rp) { P.epState = 2; fprintf(stderr, "[lib] an earlier episode: the series stays at its place until this one is watched a while\n"); }
}
static void ep_back_check(void) {
  if (P.epState != 2 || P.played < fmin(BACK_S, dur() > 0 ? dur() * 0.4 : BACK_S)) return;
  P.epState = 1; cJSON *l = lib_get(P.id); int n = l ? lib_unwatch_after(l, P.videos, P.vid) : 0;
  fprintf(stderr, "[lib] watching again from this episode: it is the series' place now, %d later episode(s) unwatched\n", n);
}
static void save_progress(int push) { double d = dur(); if (!P.id[0] || P.isTrailer || d <= 0 || live_title() || getenv("SB_PLAY_NOSAVE")) return; /* (a live broadcast: no place in Continue Watching, nothing to resume) */ /* (SB_PLAY_NOSAVE: development plays leave the library alone) */
  ep_decide(); if (P.epState != 1) return; /* an earlier episode not watched long enough yet, or a special: the series keeps its place */
  int ep = episodic() && P.next.has && P.next.vid[0];
  if (P.marked && !ep) return; /* watched to the credits, nothing after it: out of Continue Watching (marking it set its place to the start) */
  Item it = lib_item();
  /* an episode watched to its credits with the next one out: the series waits at the start of the next one, as the
     official apps keep it (video_id the next, offset 1), so it stays in Continue Watching on the next episode */
  if (ep && (P.marked || P.ended || at_credits())) lib_set_progress(P.id, &it, P.next.vid, 1, 0, push);
  else lib_set_progress(P.id, &it, P.vid, video_time() * 1000, d * 1000, push);
  if (push) fprintf(stderr, "[lib] place kept: %s at %.0f of %.0f s\n", P.vid, video_time(), d);
  P.lastSave = now_(); }
static void mark_watched(void) {
  if (P.isTrailer || P.marked || live_title() || getenv("SB_PLAY_NOSAVE")) return; P.marked = 1; Item it = lib_item(); cJSON *l = lib_get(P.id); if (!l) l = lib_new_item(&it);
  if (episodic() && P.videos && strcmp(P.vid, P.id)) { snprintf(it.videoId, sizeof it.videoId, "%s", P.vid); lib_episode_watched(&it, P.videos); } /* it and the aired ones before it; the show's place on to the next (a special: itself) */
  else if (!strcmp(P.type, "series") && P.vid[0] && strcmp(P.vid, P.id)) { snprintf(it.videoId, sizeof it.videoId, "%s", P.vid); lib_episode_watched(&it, NULL); } /* its episode list not here yet: from the cached one (never as a film) */
  else lib_set_movie_watched(l, 1);
  fprintf(stderr, "[lib] watched (%s)\n", P.ended ? "the end" : "the credits");
}
static char resumeMsg[80];
static void try_resume(void) {
  if (P.didResume || P.isTrailer) return; P.didResume = 1;
  if (live_title()) return; /* a broadcast plays from where it is now */
  if (P.startAt < 0) return; /* "Watch from the beginning" */
  if (P.startAt > 0) { video_seek(P.startAt); return; } /* another source: same moment */
  { float tp = lib_trakt_resume(P.id, P.vid); double d = dur(); if (tp > 0 && d > 0) { double sec = tp * d; /* Trakt's pause, the newer (another app, another device) */
      if (sec > 5 && sec < d - 30) { video_seek(sec); char ft[32]; fmt_time(sec, ft, sizeof ft); snprintf(resumeMsg, sizeof resumeMsg, "%s%s", T("Resuming from "), ft); fprintf(stderr, "[trakt] resumed at Trakt's pause, %.0f %%\n", tp * 100); } return; } }
  cJSON *l = lib_get(P.id); const cJSON *st = js_obj(l, "state"); if (!st) return;
  if (!strcmp(js_str(st, "video_id", ""), P.vid) && js_num(st, "timeOffset", 0) > 5000) { double sec = js_num(st, "timeOffset", 0) / 1000, d = js_num(st, "duration", 0) / 1000;
    if (!d || sec < d - 30) { video_seek(sec); char ft[32]; fmt_time(sec, ft, sizeof ft); snprintf(resumeMsg, sizeof resumeMsg, "%s%s", T("Resuming from "), ft); } } /* said as the picture comes up, not while it loads behind the art */
}

#include "screens/player_subs.inc" /* etapa-89: the subtitles, a show's own picks, auto-sync */
/* ---- audio ---- */
static void set_audio(int i) { P.curAud = i; video_select_audio(i); P.infoKey[0] = 0; }
/* ---- sound this TV can't decode: DTS where it was seen failing (or not yet seen playing), TrueHD always (passed
   through to a receiver only), and any codec the TV refused on this stream (error 200: the picture plays on) ---- */
static int aud_kind(const char *codec) { /* 1 DTS, 2 TrueHD, 0 another; the TV's names ("dts") and Matroska's ("A_DTS") alike */
  char c[32]; lower(codec, c, sizeof c); return strstr(c, "dts") ? 1 : (strstr(c, "truehd") || strstr(c, "mlp")) ? 2 : 0; }
__attribute__((weak)) int video_audio_plays(int idx) { (void)idx; return -1; } /* (a platform that can't say: the codec rules below) */
static int aud_bad(const VidTrack *t) {
  { int pl = video_audio_plays(t->idx); if (pl >= 0) { if (!pl) return 1; for (int i = 0; i < P.dts.nbad; i++) if (t->codec[0] && !strcasecmp(P.dts.bad[i], t->codec)) return 1; return 0; } } /* the device says (Android TV: ExoPlayer's own decoders, FFmpeg and passthrough): TrueHD or DTS that plays there is no worse than any other; a codec seen failing in this film still is */
  int k = aud_kind(t->codec), dm = best_dts_mode(); if (k == 2 || (k == 1 && (dm == 1 || (dm == 0 && best_learned("dts") == 0) || getenv("SB_DTS_CONVERT")))) return 1; /* DTS: Convert always; Auto once the TV was seen failing it; Off left to the TV */ /* (SB_DTS_CONVERT: development, a TV that plays DTS converts it anyway) */
  for (int i = 0; i < P.dts.nbad; i++) if (t->codec[0] && !strcasecmp(P.dts.bad[i], t->codec)) return 1;
  return 0;
}
/* the TV's list, its codecs and languages filled from the file's own tracks (same order) where the TV leaves them
   out (a dub it does not name shows by its language, not as Audio 2); every list of the audio tracks reads this one */
static int aud_list(VidTrack *ts, int max) {
  int n = video_audio_tracks(ts, max); if (!n || !mkv_listed(mkv)) return n;
  MkvAudio ma[16]; int m = mkv_audio_tracks(mkv, ma, 16);
  if (m == n) for (int i = 0; i < n; i++) {
    if (!ts[i].codec[0]) { snprintf(ts[i].codec, sizeof ts[i].codec, "%s", ma[i].codec); if (!ts[i].channels) ts[i].channels = ma[i].channels; }
    if ((!ts[i].lang[0] || !strcmp(ts[i].lang, "und")) && ma[i].lang[0] && strcmp(ma[i].lang, "und")) snprintf(ts[i].lang, sizeof ts[i].lang, "%s", ma[i].lang); }
  return n;
}
static void play_av(char *hdr, size_t hc, char *codec, size_t cc, char *chs, size_t sc); /* (below, by the info line) */
static void aud_label(const VidTrack *t, char *out, size_t cap) { /* "AC3 5.1" */
  char c[16]; size_t i = 0; const char *s = t->codec; if (!strncmp(s, "A_", 2)) s += 2; for (; s[i] && i + 1 < sizeof c; i++) c[i] = (char)toupper((unsigned char)s[i]); c[i] = 0;
  int ch = t->channels; snprintf(out, cap, "%s%s", c, ch == 6 ? " 5.1" : ch == 8 ? " 7.1" : ch == 2 ? " 2.0" : ch == 1 ? " 1.0" : "");
}
/* Original (Settings > Audio): the track in the title's original language (TMDB's), when the file has one and it is
   not the one playing; the file's own choice otherwise. Asked while the tracks come, until it is known */
static int aud_original_mode(void) { return !strcmp(store_get("defaultAudioTrack", "orig"), "orig") || strcmp(store_get("enableDefaultAudioTrack", "false"), "true"); }
static void auto_audio_original(void) {
  if (P.isTrailer || !aud_original_mode()) { P.autoAudDone = 1; return; }
  const char *ol = tmdb_orig_lang(P.id); if (!ol) return; /* (still asked: again in a moment) */
  if (!*ol) { P.autoAudDone = 1; fprintf(stderr, "[audio] Original: the title's language isn't known, the file's own track\n"); return; }
  VidTrack ts[16]; int n = aud_list(ts, 16); if (!n) return; /* (the tracks not listed yet) */
  AudInfo ai[16]; MkvAudio ma[16]; int m = mkv && mkv_listed(mkv) ? mkv_audio_tracks(mkv, ma, 16) : 0;
  for (int i = 0; i < n; i++) { ai[i].lang = ts[i].lang; ai[i].name = ts[i].name[0] ? ts[i].name : m == n ? ma[i].name : ""; ai[i].def = m == n ? ma[i].def : 0; ai[i].bad = aud_bad(&ts[i]); }
  int pick = aud_pick_original(ol, ai, n, subs_lang_match); P.autoAudDone = 1;
  if (pick < 0) { fprintf(stderr, "[audio] Original: no %s track in this file, its own stays\n", ol); return; }
  if (ts[pick].idx == video_audio_current()) { fprintf(stderr, "[audio] Original: %s already plays (%d of %d)\n", ol, pick + 1, n); return; }
  set_audio(ts[pick].idx); fprintf(stderr, "[audio] Original: the %s track (%d of %d: %s)\n", ol, pick + 1, n, ai[pick].name); }
static void auto_audio(void) {
  if (P.autoAudDone) return; const char *want = aud_want(), *an = js_str(show_pref(), "an", ""); if (!*want) { auto_audio_original(); return; } /* (this show's own, else the settings'; neither: Original) */
  VidTrack ts[16]; int n = aud_list(ts, 16); if (!n) return; int pick = -1;
  if (*an) for (int i = 0; i < n; i++) if (lang_match(ts[i].lang, want) && !strcmp(ts[i].name, an) && !aud_bad(&ts[i])) { pick = i; break; } /* the same track by its name */
  if (pick >= 0) { P.autoAudDone = 1; set_audio(ts[pick].idx); fprintf(stderr, "[audio] this show's track: %s\n", an); return; }
  for (int i = 0; i < n; i++) if (lang_match(ts[i].lang, want) && (pick < 0 || (aud_bad(&ts[pick]) && !aud_bad(&ts[i])))) pick = i; /* in that language, one whose sound plays here first */
  const char *dw = aud_setting();
  if (pick < 0 && *dw && !lang_match(dw, want)) { for (int i = 0; i < n; i++) if (lang_match(ts[i].lang, dw) && (pick < 0 || (aud_bad(&ts[pick]) && !aud_bad(&ts[i])))) pick = i; /* this show's not in this episode: the settings' */
    if (pick >= 0) fprintf(stderr, "[audio] no %s for this show here: the settings' %s\n", want, dw); }
  if (pick < 0 && strcmp(want, "eng")) for (int i = 0; i < n; i++) if (lang_match(ts[i].lang, "eng") && (pick < 0 || (aud_bad(&ts[pick]) && !aud_bad(&ts[i])))) pick = i; /* not in the file: English, when the file has it */
  if (pick >= 0) { P.autoAudDone = 1; set_audio(ts[pick].idx); fprintf(stderr, "[audio] %s track chosen: %s (%d of %d)\n", want, ts[pick].lang, pick + 1, n); }
}
/* No track this TV plays: the file is converted on the TV itself (video copied, the sound as Dolby Digital 5.1) and
   played from a local HLS stream through the same pipeline, from the same moment; the subtitles still read the file.
   Not for a stream that isn't a plain http file, nor HEVC where this TV showed it can't play it from HLS. tvIdx: the
   track in the TV's list (the same order as the file's), -1 the first DTS / TrueHD one. 1 started */
static int dv7_wanted(void);
static int dts_convert(int tvIdx, const char *why) {
  if (P.isTrailer || P.isHls || strncmp(P.url, "http", 4) || !hls_conv_available()) return 0;
  char vc[32] = ""; int hevc = mkv_video_codec(mkv, vc, sizeof vc) ? strstr(vc, "HEVC") != NULL : has_word(P.sname, "hevc") || has_word(P.sname, "x265") || has_word(P.sname, "h265");
  if (hevc && best_learned("hlsHevcOk") == 0) { fprintf(stderr, "[dts] no conversion: HEVC from local HLS failed on this TV before\n"); return 0; }
  int ord = tvIdx; MkvAudio ma[16]; int m = mkv_audio_tracks(mkv, ma, 16);
  if (ord >= m || (ord >= 0 && !aud_kind(ma[ord].codec))) ord = -1; /* (the TV's list and the file's disagree: the first DTS / TrueHD one) */
  P.dts.conv = 1; P.dts.convOrd = ord; P.dts.hevc = hevc; P.dts.convT0 = now_(); P.dts.convAt = P.live ? video_time() : P.startAt;
  P.dts.dv = dv7_wanted(); P.dts.copyA = 0; P.dts.dvTried = 1; /* a profile 7 picture: 8.1 in the same run */
  hls_conv_start(P.url, ord, (mkv_has_index(mkv) == 0 ? HC_NOINDEX : 0) | (P.dts.dv == 7 ? HC_DV81 : 0));
  fprintf(stderr, "[dts] path c: converting audio track %d (%s, %s%s) at %.0f s\n", ord, why, hevc ? "HEVC" : vc[0] ? vc : "video", P.dts.dv == 7 ? ", Dolby Vision 7 as 8.1" : P.dts.dv ? ", Dolby Vision kept" : "", P.dts.convAt);
  toast(T("Converting the sound for this TV…"), 4000, NULL);
  return 1;
}
/* ---- Dolby Vision profile 7 (Blu-ray remuxes: a base layer, an enhancement layer and the RPU in one track). A TV
   with Dolby Vision shows such a file as HDR10 only; converted here to single-layer profile 8.1 (the enhancement
   layer dropped, each RPU rewritten, the base layer as it is) it shows as Dolby Vision, through the same pipeline from
   local HLS, with the audio track copied (converted when this TV can't decode it). Only for an http file whose head
   says profile 7, on a TV with Dolby Vision, with the setting on, and unless this TV has shown that HEVC from local
   HLS fails or that the converted picture still shows as HDR10. Anything that fails plays the file as before.
   A profile 8 or 5 picture in Matroska (web copies) goes the same way with its RPU as it is: the TV showed it as HDR10 ---- */
static int dv7_wanted(void) {
  if (P.isTrailer || P.isHls || strncmp(P.url, "http", 4) || !hls_conv_available()) return 0;
  if (!strcmp(store_get("dv7Convert", "true"), "false") || best_learned("hlsHevcOk") == 0 || best_learned("dv81Hls") == 0) return 0;
  const TvCaps *c = best_caps(); if (!c || !c->dv) return 0;
  int el = 0, p = mkv_dovi(mkv, &el); return p == 7 || ((p == 8 || p == 5) && !el) ? p : 0; /* 8 and 5 (web copies) too: from a Matroska file the TV showed them as HDR10 */
}
static int aud_copyable(const char *codec) { /* the tracks the stream can carry as they are */
  static const char *const OK[] = { "A_AC3", "A_EAC3", "A_AAC", "A_DTS", "A_TRUEHD", "A_MPEG/L3", "A_MPEG/L2" };
  for (int i = 0; i < 7; i++) if (!strncmp(codec, OK[i], strlen(OK[i]))) return 1; return 0;
}
static int mkv_aud_bad(const MkvAudio *a) { VidTrack v; memset(&v, 0, sizeof v); snprintf(v.codec, sizeof v.codec, "%s", a->codec); return aud_bad(&v); }
/* the track that goes with the converted picture: the default-audio language, else the file's default, else the first;
   one this TV can't decode gives way to one it plays in the same language, else it is converted. -1 none fits */
static int dv7_audio(int *copy) {
  MkvAudio ma[16]; int m = mkv_audio_tracks(mkv, ma, 16), pick = -1; if (!m) return -1;
  const char *want = aud_want();
  for (int i = 0; i < m && pick < 0; i++) if (*want && lang_match(ma[i].lang, want)) pick = i;
  for (int i = 0; i < m && pick < 0; i++) if (ma[i].def) pick = i;
  if (pick < 0) pick = 0;
  if (mkv_aud_bad(&ma[pick]) || !aud_copyable(ma[pick].codec))
    for (int i = 0; i < m; i++) if (!mkv_aud_bad(&ma[i]) && aud_copyable(ma[i].codec) && !strcasecmp(ma[i].lang, ma[pick].lang)) { pick = i; break; }
  if (!mkv_aud_bad(&ma[pick]) && aud_copyable(ma[pick].codec)) { *copy = 1; return pick; }
  if (aud_kind(ma[pick].codec)) { *copy = 0; return pick; } /* DTS / TrueHD this TV can't decode: as Dolby Digital */
  return -1;
}
static int conv_flags(int ord) { /* another audio track while converting */
  MkvAudio ma[16]; int m = mkv_audio_tracks(mkv, ma, 16), copy = P.dts.dv && ord >= 0 && ord < m && !mkv_aud_bad(&ma[ord]) && aud_copyable(ma[ord].codec);
  return (mkv_has_index(mkv) == 0 ? HC_NOINDEX : 0) | (P.dts.dv == 7 ? HC_DV81 : 0) | (copy ? HC_COPYAUDIO : 0);
}
static int dv7_convert(const char *why) {
  int prof = P.dts.conv || P.dts.dvTried ? 0 : dv7_wanted(); if (!prof) return 0;
  P.dts.dvTried = 1; int copy = 1, ord = dv7_audio(&copy);
  if (ord < 0) { fprintf(stderr, "[dv] profile %d, but no audio track can go with it: played as it is\n", prof); return 0; }
  P.dts.conv = 1; P.dts.convOrd = ord; P.dts.dv = prof; P.dts.copyA = copy; P.dts.hevc = 1; P.dts.convT0 = now_(); P.dts.convAt = P.live ? video_time() : P.startAt;
  hls_conv_start(P.url, ord, (mkv_has_index(mkv) == 0 ? HC_NOINDEX : 0) | (prof == 7 ? HC_DV81 : 0) | (copy ? HC_COPYAUDIO : 0));
  fprintf(stderr, "[dv] profile %d (%s): the picture %s, audio track %d %s, from %.0f s\n", prof, why, prof == 7 ? "as 8.1" : "as it is, its Dolby Vision kept", ord, copy ? "copied" : "as Dolby Digital", P.dts.convAt);
  return 1;
}
/* A stream whose text says Dolby Vision from a Blu-ray (a remux, FEL / MEL) or in a Matroska file, on a TV with Dolby Vision:
   the first load waits for the file's head (a moment), so its picture starts converted instead of switching after a second */
static int dv7_defer_wanted(void) {
  if (P.isTrailer || P.isHls || strncmp(P.url, "http", 4) || !hls_conv_available()) return 0;
  if (!strcmp(store_get("dv7Convert", "true"), "false") || best_learned("hlsHevcOk") == 0 || best_learned("dv81Hls") == 0) return 0;
  const TvCaps *c = best_caps(); if (!c || !c->dv) return 0;
  char l[1100]; snprintf(l, sizeof l, "%s %s", P.sname, P.st.filename); for (char *q = l; *q; q++) *q = (char)tolower((unsigned char)*q);
  int dv = has_word(l, "dv") || has_word(l, "dovi") || strstr(l, "dolby vision") || has_word(l, "dolbyvision");
  return dv && (strstr(l, ".mkv") || strstr(l, "mkv") || strstr(l, "remux") || strstr(l, "bluray") || strstr(l, "blu-ray") || has_word(l, "bdmv") || has_word(l, "fel") || has_word(l, "mel") || has_word(l, "p7") || has_word(l, "dvp7")); /* (a Matroska web copy too: profile 8 goes the same way) */
}
/* while converting: the stream once it is ready, then whether the TV plays it (HEVC in HLS: learned once) */
static void dts_conv_tick(void) {
  if (P.dts.conv == 1) {
    char url[256], why[160]; HlsConvInfo ci; int st = hls_conv_poll(url, sizeof url, why, sizeof why, &ci);
    if (!st) return;
    if (st < 0 && P.dts.dv && P.dts.copyA) { /* the picture only: the file as it is, nothing said */
      P.dts.conv = 0; P.dts.dv = 0; fprintf(stderr, "[dv] the converter can't take this file (%s): played as it is\n", why);
      if (video_state() == VID_IDLE) { P.live = 0; P.liveT0 = 0; P.didResume = 0; video_load(P.url); }
      return; }
    if (st < 0 && P.dts.dv) { /* the picture couldn't be converted: the sound alone */
      fprintf(stderr, "[dv] the converter can't take this picture (%s): the sound alone\n", why); P.dts.dv = 0; hls_conv_start(P.url, P.dts.convOrd, conv_flags(P.dts.convOrd)); return; }
    if (st < 0) { P.dts.conv = 0; P.dts.gaveUp = 1; fprintf(stderr, "[dts] conversion failed: %s\n", why); toast(T("This TV can’t play this video’s sound (DTS or TrueHD)"), 6000, NULL);
      if (video_state() == VID_IDLE) { P.live = 0; P.liveT0 = 0; P.didResume = 0; video_load(P.url); } /* the picture as before, without its sound */
      return; }
    P.dts.conv = 2; P.dts.hevc = strstr(ci.vcodec, "hevc") != NULL; P.dts.convT0 = now_(); P.dts.okAt = 0;
    { const char *fc = strstr(ci.path, "E-AC3") || strstr(ci.path, "EAC3") ? "E-AC3" : strstr(ci.path, "AC3") ? "AC3" : strstr(ci.path, "AAC") ? "AAC" : ""; char chs[8] = ""; if (ci.outCh >= 3) snprintf(chs, sizeof chs, " %d.1", ci.outCh - 1); else if (ci.outCh > 0) snprintf(chs, sizeof chs, " %d.0", ci.outCh);
      snprintf(P.dts.outDesc, sizeof P.dts.outDesc, "%s%s", fc, *fc ? chs : ""); } /* (what the TV gets: "AC3 5.1") */
    double at = P.live && video_has_media() ? video_time() : P.dts.convAt; if (at < 1) at = P.startAt;
    P.live = 0; P.liveT0 = 0; P.didResume = 0; P.startAt = at > 1 ? at : P.startAt; P.curAud = -1; P.infoKey[0] = 0;
    fprintf(stderr, "[dts] playing the converted stream (%s, %s; %s) from %.0f s\n", ci.indexed ? "segments" : "one stream", ci.vcodec, ci.path, at);
    P.dts.dvSeen = 0; P.dts.dvAt = 0;
    if (ci.indexed) video_load(url); else video_load_timed(url, ci.dur);
    return;
  }
  if (P.dts.conv == 2 && P.dts.hevc && best_learned("hlsHevcOk") < 0) { /* HEVC in local HLS: plays here or not, learned once */
    if (video_state() == VID_PLAYING && video_live()) { if (!P.dts.okAt) P.dts.okAt = now_(); else if (now_() - P.dts.okAt >= 3) best_learn("hlsHevcOk", 1); } else P.dts.okAt = 0; }
  if (P.dts.conv == 2 && P.dts.dv && !P.dts.dvSeen) { /* the converted picture: Dolby Vision on screen, or HDR10 all the same (then not converted again here) */
    if (video_state() == VID_PLAYING && video_live()) { if (!P.dts.dvAt) P.dts.dvAt = now_();
      else if (now_() - P.dts.dvAt >= 3) { char h[32]; if (video_hdr(h, sizeof h)) { P.dts.dvSeen = 1; int yes = strcasestr(h, "dolby") != NULL;
          fprintf(stderr, "[dv] the TV shows the converted picture as %s\n", h); if (yes || strcasestr(h, "hdr")) best_learn("dv81Hls", yes); }
        else if (now_() - P.dts.dvAt >= 15) P.dts.dvSeen = 1; } } /* (this TV doesn't say) */
    else P.dts.dvAt = 0; }
}
static void dts_conv_failed(const char *why) { /* the converted stream didn't play: today's reason, and HEVC from HLS is not tried again here */
  fprintf(stderr, "[dts] the converted stream didn't play (%s)\n", why);
  video_unload(); /* first: the TV let go of the stream, so what it still asks of it (gone once the run stops) is not taken for this attempt's error */
  if (P.dts.dv) { /* the converted picture: not converted again on this TV; from here the file as it is, or (DTS) the sound alone converted */
    best_learn("dv81Hls", 0); hls_conv_stop(); P.dts.dv = 0;
    double at = P.lastPos > 1 ? P.lastPos : P.dts.convAt; P.startAt = at > 1 ? at : P.startAt; P.live = 0; P.liveT0 = 0; P.didResume = 0; P.curAud = -1;
    if (P.dts.copyA) { P.dts.conv = 0; video_load(P.url); return; }
    P.dts.conv = 1; P.dts.convAt = P.startAt; P.dts.convT0 = now_(); hls_conv_start(P.url, P.dts.convOrd, conv_flags(P.dts.convOrd));
    fprintf(stderr, "[dts] the sound alone converted from %.0f s\n", P.startAt); return; }
  if (P.dts.hevc) best_learn("hlsHevcOk", 0);
  hls_conv_stop(); P.dts.conv = 0; P.dts.gaveUp = 1; fail(T("This TV can’t play this video’s sound (DTS or TrueHD)"));
}
/* Once a second while playing: the track the TV plays is checked against what this TV decodes. DTS still unknown here
   is left to the TV (3 s of play without an error 200 teaches "yes", the error "no"); a track that can't play gives way
   to one that does, in the language of the default-audio setting, else the same language, else the first; with none,
   the reason is said once and the picture plays on */
static void dts_tick(void) {
  if (P.dts.conv) { dts_conv_tick(); return; }
  if (!P.dts.dvTried && mkv_listed(mkv) && dv7_convert("read in the file's head")) return;
  if (P.isTrailer || P.dts.gaveUp) return;
  VidTrack ts[16]; int n = aud_list(ts, 16); if (!n) return; /* only after the TV listed them: choosing earlier stalls it */
  int cur = P.curAud >= 0 ? P.curAud : video_audio_current(), ci = 0; for (int i = 0; i < n; i++) if (ts[i].idx == cur) ci = i;
  VidTrack *t = &ts[ci]; int kind = aud_kind(t->codec), unsup = video_audio_unsupported();
  if (kind == 1 && best_dts_mode() == 2) return; /* DTS sound Off: the TV does with it whatever it can, nothing here steps in */
  if (unsup) { if (kind == 1 && !best_dts_mode()) { best_learn("dts", 0); fprintf(stderr, "[dts] auto: this TV refused DTS, converting from now on\n"); } int known = 0; for (int i = 0; i < P.dts.nbad; i++) if (!strcasecmp(P.dts.bad[i], t->codec)) known = 1;
    if (!known && t->codec[0] && P.dts.nbad < 4) snprintf(P.dts.bad[P.dts.nbad++], sizeof P.dts.bad[0], "%s", t->codec); }
  if (kind == 1 && !unsup && !best_dts_mode() && best_learned("dts") < 0 && video_state() == VID_PLAYING) { if (!P.dts.playAt) P.dts.playAt = now_(); else if (now_() - P.dts.playAt >= 3) { best_learn("dts", 1); fprintf(stderr, "[dts] auto: this TV plays DTS itself\n"); } }
  else P.dts.playAt = 0;
  if (!unsup && !aud_bad(t)) return;
  const char *want = aud_want();
  int pick = -1, score = -1;
  for (int i = 0; i < n && P.dts.switches < 4; i++) { if (i == ci || aud_bad(&ts[i])) continue; /* (tracks the TV doesn't name: a few tries at most) */
    int sc = (*want && lang_match(ts[i].lang, want)) ? 2 : (t->lang[0] && !strcasecmp(ts[i].lang, t->lang)) ? 1 : 0; if (sc > score) { score = sc; pick = i; } }
  const char *what = kind == 2 ? "TrueHD" : "DTS";
  if (pick >= 0) {
    char lb[32], msg[160]; aud_label(&ts[pick], lb, sizeof lb);
    snprintf(msg, sizeof msg, "%s", kind == 2 ? T1("Playing the {t} track: this TV can’t play TrueHD", "t", lb) : T1("Playing the {t} track: this TV can’t play DTS", "t", lb));
    fprintf(stderr, "[dts] path b: %s track %d (%s) can't play here%s, track %d (%s %s) instead\n", what, ci, t->codec, unsup ? " (error 200)" : "", pick, ts[pick].codec, ts[pick].lang);
    P.autoAudDone = 1; set_audio(ts[pick].idx); P.dts.switches++; /* (the automatic pick, still retrying, never switches back to the track that can't play) */ if (!P.dts.said) { P.dts.said = 1; toast(msg, 4000, NULL); }
    return;
  }
  if (dts_convert(ci, unsup ? "error 200" : "known")) return;
  P.dts.gaveUp = 1;
  fprintf(stderr, "[dts] path c: only %s here (track %d, %s)%s: no track this TV plays\n", what, ci, t->codec, unsup ? ", error 200" : "");
  toast(T("This TV can’t play this video’s sound (DTS or TrueHD)"), 6000, NULL);
}

/* ---- skips: intro / recap / credits. Sources, best first, merged per segment: the file's own chapters (a reader
   hooks in below), TheIntroDB and IntroDB by IMDb id, AniSkip for anime from Kitsu-based addons. A segment that
   doesn't fit this file's length is dropped and the next source's is used. One source with something useful
   gives the others 2.5 s more; whatever answered by 14 s is used. API answers are kept 6 h; errors never ---- */
void (*player_chapters_hook)(const char *url, int p2p);
enum { SK_TIDB, SK_IDB, SK_KITSU, SK_ANI, SK_ARM }; /* ARM: an anime known by its IMDb id, mapped to its MyAnimeList season (arm.haglund.dev) */
static struct { char url[440]; double at; char *body; } skCache[120]; static int skCacheN; static unsigned skTok;
static const char *sk_cached(const char *u) { for (int i = 0; i < skCacheN; i++) if (!strcmp(skCache[i].url, u)) return now_() - skCache[i].at < 6 * 3600 ? skCache[i].body : NULL; return NULL; }
static void sk_keep(const char *u, const char *body) {
  int i = 0; for (; i < skCacheN; i++) if (!strcmp(skCache[i].url, u)) break;
  if (i == skCacheN) { if (skCacheN < 120) skCacheN++; else { free(skCache[0].body); memmove(skCache, skCache + 1, sizeof skCache[0] * 119); i = 119; skCache[i].body = NULL; } }
  free(skCache[i].body); snprintf(skCache[i].url, sizeof skCache[i].url, "%s", u); skCache[i].body = strdup(body); skCache[i].at = now_();
}
static int sk_fits(int k, double s0, double e0) { double d = P.sk.d; if (!d) return 1; if (s0 >= d || (k != 2 && e0 > d + 30)) return 0; /* (credits "to the end", asked before the length was known, end later: trimmed to it) */
  if (k == 2 && P.sk.isEp && d < 4200 && d - s0 > 600) return 0; /* an episode's credits starting over 10 min before its end: not its credits */
  return k == 2 ? s0 >= d * 0.5 : s0 < d * 0.4 && e0 - s0 >= 3 && e0 - s0 <= 420; }
static void sk_seg(int *has, double *s, double *e, double a, double b) { *has = a >= 0 && b > a; if (*has) { *s = a; *e = b; } }
static void sk_add(int rank, const int has[3], const double ss[3], const double ee[3]) {
  if (P.sk.nsrc >= 4) return; int i = P.sk.nsrc++; P.sk.src[i].rank = rank; int any = 0;
  for (int k = 0; k < 3; k++) { P.sk.src[i].g[k].has = has[k]; P.sk.src[i].g[k].s = ss[k]; P.sk.src[i].g[k].e = ee[k]; any |= has[k]; }
  for (int j = i; j > 0 && P.sk.src[j - 1].rank > P.sk.src[j].rank; j--) { __typeof__(P.sk.src[0]) t = P.sk.src[j]; P.sk.src[j] = P.sk.src[j - 1]; P.sk.src[j - 1] = t; }
  if (any && !P.sk.quickAt) P.sk.quickAt = now_() + 2.5;
}
static void sk_finish(void) {
  if (P.sk.sent) return; P.sk.sent = 1; P.sk.tok = 0; double d = P.sk.d; int has[3] = { 0 }; double ss[3] = { 0 }, ee[3] = { 0 };
  for (int k = 0; k < 3; k++) for (int j = 0; j < P.sk.nsrc; j++) { const __typeof__(P.sk.src[0].g[0]) *g = &P.sk.src[j].g[k]; if (!g->has || !sk_fits(k, g->s, g->e)) continue;
      if (k == 2 && has[2] && (P.sk.src[j].rank == 0 || g->s <= ss[2])) continue; /* the credits: the file's own chapter, else the latest the sources give (two sources can be minutes apart for one episode: the card comes no earlier than the latest) */
      has[k] = 1; ss[k] = g->s; ee[k] = g->e; if (k != 2 || P.sk.src[j].rank == 0) break; }
  if (has[2] && d && ee[2] > d) ee[2] = d;
  /* credits learned from this show's other episodes: how long before the end they start */
  if (episodic()) { char lk[96]; snprintf(lk, sizeof lk, "creditsOff_%s", P.id);
    if (has[2] && d && d - ss[2] > 10 && d - ss[2] < 600) { char v[16]; snprintf(v, sizeof v, "%d", (int)(d - ss[2] + 0.5)); store_set(lk, v); }
    else if (!has[2] && d > 600) { double off = atof(store_get(lk, "0")); if (off > 90) off = 90; /* (another episode's: never earlier than with no credits known at all) */ if (off > 10) { has[2] = 1; ss[2] = d - off; ee[2] = d; } } }
  fprintf(stderr, "[player] skips from %d sources: intro %.0f-%.0f recap %.0f-%.0f credits %.0f-%.0f\n", P.sk.nsrc, has[0] ? ss[0] : -1, ee[0], has[1] ? ss[1] : -1, ee[1], has[2] ? ss[2] : -1, ee[2]);
  if (!has[0] && !has[1] && !has[2]) return;
  P.intro.has = has[0]; P.intro.s = ss[0]; P.intro.e = ee[0]; P.recap.has = has[1]; P.recap.s = ss[1]; P.recap.e = ee[1]; P.outro.has = has[2]; P.outro.s = ss[2]; P.outro.e = ee[2]; P.skipsOk = 1;
}
static void sk_done(void) { if (--P.sk.pending <= 0) sk_finish(); }
static void sk_ask(int i);
static double sk_ms(const cJSON *a, const char *k, double nul) { const cJSON *v = cJSON_GetObjectItemCaseSensitive(a, k); return !v || cJSON_IsNull(v) ? nul : cJSON_IsNumber(v) ? v->valuedouble / 1000 : -1; }
static double sk_sec(const cJSON *o, const char *k, double nul) { const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, k); return !v || cJSON_IsNull(v) ? nul : cJSON_IsNumber(v) ? v->valuedouble : cJSON_IsString(v) ? atof(v->valuestring) : -1; }
/* one source's answer (j NULL: nothing usable) */
static void sk_answer(int i, const cJSON *j) {
  int kind = P.sk.job[i].kind, has[3] = { 0 }; double ss[3] = { 0 }, ee[3] = { 0 }, d = P.sk.d;
  if (kind == SK_TIDB) {
    if (!j || (P.sk.isEp && cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(j, "episode")) && ((int)js_num(j, "season", -1) != P.sk.se || (int)js_num(j, "episode", -1) != P.sk.en))) { sk_done(); return; } /* another episode: not ours */
    static const char *K[3] = { "intro", "recap", "credits" };
    for (int k = 0; k < 3; k++) { const cJSON *l = js_arr(j, K[k]), *a = l ? cJSON_GetArrayItem(l, 0) : NULL; if (!cJSON_IsObject(a)) continue; double s0 = sk_ms(a, "start_ms", 0); sk_seg(&has[k], &ss[k], &ee[k], s0, sk_ms(a, "end_ms", k == 2 ? (d ? d : s0 + 600) : -1)); }
    sk_add(1, has, ss, ee);
  } else if (kind == SK_IDB) {
    if (!j) { sk_done(); return; }
    static const char *K[3] = { "intro", "recap", "outro" };
    for (int k = 0; k < 3; k++) { const cJSON *o = js_obj(j, K[k]); if (!o) continue; double s0 = sk_sec(o, "start_sec", -1); sk_seg(&has[k], &ss[k], &ee[k], s0, sk_sec(o, "end_sec", k == 2 ? (d ? d : s0 + 600) : -1)); }
    sk_add(2, has, ss, ee);
  } else if (kind == SK_ARM) { /* the season's MyAnimeList entry (the TVDB season, as Cinemeta numbers them), then its skip times */
    const cJSON *e, *pick = NULL; cJSON_ArrayForEach(e, j) { if (strcmp(js_str(e, "media", "TV"), "TV")) continue; int ts = (int)js_num(e, "thetvdb-season", -1); if (ts == P.sk.se) { pick = e; break; } if (!pick && P.sk.se <= 1) pick = e; }
    int mal = pick ? (int)js_num(pick, "myanimelist", 0) : 0; if (mal <= 0) { sk_done(); return; }
    P.sk.job[i].kind = SK_ANI; P.sk.job[i].tries = 0; snprintf(P.sk.job[i].url, sizeof P.sk.job[i].url, "https://api.aniskip.com/v2/skip-times/%d/%d?types=op&types=ed&types=recap&types=mixed-op&types=mixed-ed&episodeLength=0", mal, P.sk.en); sk_ask(i); return;
  } else if (kind == SK_KITSU) { /* the MyAnimeList id, then its skip times (the same job) */
    const cJSON *dt = js_arr(j, "data"), *m0 = dt ? cJSON_GetArrayItem(dt, 0) : NULL; const char *mal = js_str(js_obj(m0, "attributes"), "externalId", NULL);
    if (!mal) { sk_done(); return; }
    char enc[64]; size_t o = 0; for (const char *c = mal; *c && o + 4 < sizeof enc; c++) { if (isalnum((unsigned char)*c) || strchr("-_.~", *c)) enc[o++] = *c; else o += snprintf(enc + o, sizeof enc - o, "%%%02X", (unsigned char)*c); } enc[o] = 0;
    P.sk.job[i].kind = SK_ANI; P.sk.job[i].tries = 0; snprintf(P.sk.job[i].url, sizeof P.sk.job[i].url, "https://api.aniskip.com/v2/skip-times/%s/%d?types=op&types=ed&types=recap&types=mixed-op&types=mixed-ed&episodeLength=0", enc, P.sk.en); sk_ask(i); return;
  } else {
    const cJSON *r = js_bool(j, "found", 0) ? js_arr(j, "results") : NULL, *x;
    if (r) cJSON_ArrayForEach(x, r) { const cJSON *iv = js_obj(x, "interval"); const char *t = js_str(x, "skipType", ""); int hs; double a, b; sk_seg(&hs, &a, &b, sk_sec(iv, "startTime", -1), sk_sec(iv, "endTime", -1)); if (!hs) continue;
      int k = (!strcmp(t, "op") || !strcmp(t, "mixed-op")) ? 0 : (!strcmp(t, "ed") || !strcmp(t, "mixed-ed")) ? 2 : !strcmp(t, "recap") ? 1 : -1; if (k >= 0 && !has[k]) { has[k] = 1; ss[k] = a; ee[k] = b; } }
    sk_add(3, has, ss, ee);
  }
  sk_done();
}
/* a real answer, or a real "nothing for this title" (kept too); anything else is asked again twice, 1.5 s apart */
static void on_sk(int st, const unsigned char *body, size_t len, void *ud) {
  (void)len; unsigned v = (unsigned)(uintptr_t)ud; int i = v & 3; if (!P.open || !P.sk.tok || v >> 2 != P.sk.tok || !P.sk.job[i].live) return;
  cJSON *j = st > 0 ? net_json(body) : NULL; const cJSON *err = cJSON_GetObjectItemCaseSensitive(j, "error");
  int real = cJSON_IsObject(j) && !err && (cJSON_GetObjectItemCaseSensitive(j, "imdb_id") || cJSON_GetObjectItemCaseSensitive(j, "tmdb_id") || cJSON_GetObjectItemCaseSensitive(j, "found") || (P.sk.job[i].kind == SK_KITSU && js_arr(j, "data")));
  if (P.sk.job[i].kind == SK_ARM && cJSON_IsArray(j)) real = cJSON_GetArraySize(j) > 0;
  int miss = P.sk.job[i].kind == SK_ARM && cJSON_IsArray(j) && !real; if (cJSON_IsString(err)) { char e[96]; lower(err->valuestring, e, sizeof e); miss = strstr(e, "not found") != NULL; }
  if (!real && !miss && ++P.sk.job[i].tries < 3) { cJSON_Delete(j); P.sk.job[i].retryAt = now_() + 1.5; return; }
  if (real || miss) sk_keep(P.sk.job[i].url, real ? (const char *)body : "{}");
  P.sk.job[i].live = 0; sk_answer(i, real ? j : NULL); cJSON_Delete(j);
}
static void sk_ask(int i) {
  const char *c = sk_cached(P.sk.job[i].url);
  if (c) { cJSON *j = cJSON_Parse(c); P.sk.job[i].live = 0; int ok = (cJSON_IsObject(j) || cJSON_IsArray(j)) && cJSON_GetArraySize(j) > 0; sk_answer(i, ok ? j : NULL); cJSON_Delete(j); return; }
  P.sk.job[i].live = 1; P.sk.job[i].retryAt = 0; net_get(P.sk.job[i].url, on_sk, (void *)(uintptr_t)((P.sk.tok << 2) | (unsigned)i));
}
static void sk_job(const char *url, int kind) { int i = 0; while (i < 3 && P.sk.job[i].url[0]) i++; if (i >= 3) return; snprintf(P.sk.job[i].url, sizeof P.sk.job[i].url, "%s", url); P.sk.job[i].kind = kind; P.sk.pending++; sk_ask(i); }
static void sk_tick(void) {
  if (!P.sk.tok || P.sk.sent) return; double n = now_();
  for (int i = 0; i < 3; i++) if (P.sk.job[i].live && P.sk.job[i].retryAt && n >= P.sk.job[i].retryAt) sk_ask(i);
  if (!P.sk.sent && ((P.sk.quickAt && n >= P.sk.quickAt) || n >= P.sk.endAt)) sk_finish();
}
static void load_skips(void) {
  if (P.isTrailer || P.skipsAsked || !P.id[0]) return; P.skipsAsked = 1;
  const char *ts = getenv("SB_PLAY_SKIPS"); /* development: "intro:s-e,recap:s-e,outro:s-e" instead of the sources (the page's skipsForTest) */
  if (ts) { for (const char *c = ts; c && *c; c = strchr(c, ',') ? strchr(c, ',') + 1 : NULL) { double a = 0, b = 0; if (sscanf(strchr(c, ':') ? strchr(c, ':') + 1 : c, "%lf-%lf", &a, &b) < 2) continue;
      __typeof__(P.intro) *g = !strncmp(c, "intro", 5) ? &P.intro : !strncmp(c, "recap", 5) ? &P.recap : &P.outro; g->has = 1; g->s = a; g->e = b; } P.skipsOk = 1; return; }
  memset(&P.skOn, 0, sizeof P.skOn); P.sk.tok = ++skTok & 0x3fffffff; if (!P.sk.tok) P.sk.tok = ++skTok; P.sk.d = (int)(dur() + 0.5); P.sk.endAt = now_() + 14; P.sk.pending = 1; /* 1 = still adding jobs */
  /* 0. the file's own chapters, when a reader is there */
  if (player_chapters_hook && !P.isHls) { P.sk.chapWait = 1; P.sk.pending++; player_chapters_hook(P.url, P.isP2p); }
  char u[440]; const char *vid = P.vid; int ne = 0; const char *c = vid; while ((c = strchr(c, ':'))) { ne++; c++; }
  if (!strncmp(vid, "kitsu:", 6)) { const char *a = vid + 6; const char *b = strchr(a, ':'); P.sk.en = b ? atoi(b + 1) : 1; char kid[32]; snprintf(kid, sizeof kid, "%.*s", b ? (int)(b - a) : (int)strlen(a), a);
    if (atoi(kid) > 0) { snprintf(u, sizeof u, "https://kitsu.io/api/edge/anime/%d/mappings?filter%%5BexternalSite%%5D=myanimelist%%2Fanime", atoi(kid)); sk_job(u, SK_KITSU); } }
  const char *imdb = !strncmp(P.id, "tt", 2) ? P.id : !strncmp(vid, "tt", 2) ? vid : NULL;
  if (imdb) { char im[24]; int k = 2; while (k < 22 && isdigit((unsigned char)imdb[k])) k++; snprintf(im, sizeof im, "%.*s", k, imdb);
    P.sk.isEp = !strcmp(P.type, "series") && ne >= 2; if (P.sk.isEp) { const char *l = strrchr(vid, ':'), *pp = l - 1; while (pp > vid && *pp != ':') pp--; P.sk.se = atoi(pp + 1); P.sk.en = atoi(l + 1); }
    if (P.sk.isEp) snprintf(u, sizeof u, "https://api.theintrodb.org/v3/media?imdb_id=%s&season=%d&episode=%d", im, P.sk.se, P.sk.en); else snprintf(u, sizeof u, "https://api.theintrodb.org/v3/media?imdb_id=%s", im); sk_job(u, SK_TIDB);
    if (P.sk.isEp) snprintf(u, sizeof u, "https://api.introdb.app/segments?imdb_id=%s&season=%d&episode=%d", im, P.sk.se, P.sk.en); else snprintf(u, sizeof u, "https://api.introdb.app/segments?imdb_id=%s&is_movie=true", im); sk_job(u, SK_IDB);
    if (P.sk.isEp && P.meta) { int an = 0; const cJSON *g; cJSON_ArrayForEach(g, js_arr(P.meta, "genres")) if (cJSON_IsString(g) && (!strcasecmp(g->valuestring, "Anime") || !strcasecmp(g->valuestring, "Animation"))) an = 1;
      const char *co = js_str(P.meta, "country", ""); if (an && (strstr(co, "Japan") || strstr(co, "Jap"))) an = 2; else { const cJSON *g2; cJSON_ArrayForEach(g2, js_arr(P.meta, "genres")) if (cJSON_IsString(g2) && !strcasecmp(g2->valuestring, "Anime")) an = 2; }
      if (an == 2) { snprintf(u, sizeof u, "https://arm.haglund.dev/api/v2/imdb?id=%s", im); sk_job(u, SK_ARM); } } } /* (an anime with an IMDb id: AniSkip through its MyAnimeList season) */
  if (P.sk.tok) sk_done();
}
/* the chapters a reader found in this file: the best skip source ("Opening", "Recap", "End Credits"...); n <= 0: none */
static int ch_word(const char *n, const char *const *w) {
  for (; *w; w++) { size_t l = strlen(*w); for (const char *p = n; (p = strstr(p, *w)); p++) { int a = p == n || !(isalnum((unsigned char)p[-1]) || p[-1] == '_'), b = !(isalnum((unsigned char)p[l]) || p[l] == '_'); if (a && b) return 1; } }
  return 0;
}
void player_chapters(const char *url, const PlayerChapter *list, int n) {
  if (!P.open || !P.sk.chapWait || P.sk.sent || strcmp(url, P.url)) return; P.sk.chapWait = 0;
  P.nchap = 0; P.chLast = -1; /* every chapter, for the seek bar: its start, and its name unless it is only a number */
  for (int i = 0; i < n && P.nchap < 64; i++) { if (list[i].s < 0) continue; int at = P.nchap; while (at > 0 && P.chap[at - 1].s > list[i].s) { P.chap[at] = P.chap[at - 1]; at--; }
    P.chap[at].s = list[i].s; char nm[128]; snprintf(nm, sizeof nm, "%s", list[i].name); char *a = nm; while (*a == ' ') a++; char *e = a + strlen(a); while (e > a && e[-1] == ' ') *--e = 0;
    char lw[128]; lower(a, lw, sizeof lw); const char *q = lw; static const char *const NUMW[] = { "chapter", "chapitre", "kapitel", "capítulo", "capitulo", "capitolo", "hoofdstuk", "chap", NULL };
    for (int k = 0; NUMW[k]; k++) if (!strncmp(q, NUMW[k], strlen(NUMW[k]))) { q += strlen(NUMW[k]); break; } while (*q == ' ' || *q == '.' || *q == '-') q++;
    const char *dg = q; while (isdigit((unsigned char)*dg) || *dg == ':' || *dg == '.') dg++; snprintf(P.chap[at].nm, sizeof P.chap[at].nm, "%s", *a && *dg ? a : ""); /* (only a number, or a time: unnamed) */
    P.nchap++; }
  if (P.nchap) fprintf(stderr, "[player] %d chapters\n", P.nchap);
  static const char *const IN[] = { "intro", "opening", "op", "title sequence", "main title", "main titles", "theme song", "générique", "generique", "genérique", "génerique", "vorspann", NULL };
  static const char *const RE[] = { "recap", "previously", "résumé", "resume", "résume", "resumé", "rückblick", "ruckblick", "summary", NULL };
  static const char *const CR[] = { "credit", "credits", "end credits", "endcredits", "ending", "ed", "outro", "closing", "end title", "end titles", "générique de fin", "generique de fin", "abspann", NULL };
  int has[3] = { 0 }; double ss[3] = { 0 }, ee[3] = { 0 };
  for (int i = 0; i < n; i++) { char nm[128]; lower(list[i].name, nm, sizeof nm); if (list[i].e <= list[i].s) continue;
    const char *q = nm; static const char *const NUM[] = { "chapter", "chapitre", "kapitel", "capítulo", "capitulo", "capitolo", "hoofdstuk", NULL };
    for (int k = 0; NUM[k]; k++) if (!strncmp(q, NUM[k], strlen(NUM[k]))) { q += strlen(NUM[k]); break; }
    while (*q == ' ' || *q == '\t') q++; if (*q) { const char *d = q; while (isdigit((unsigned char)*d)) d++; if (d > q && !*d) continue; } /* "Chapter 3": no name */
    if (strstr(nm, "cold open") || strstr(nm, "teaser")) continue;
    int k = ch_word(nm, IN) && !has[0] ? 0 : ch_word(nm, RE) && !has[1] ? 1 : ch_word(nm, CR) && !has[2] ? 2 : -1;
    if (k < 0) continue; has[k] = 1; ss[k] = list[i].s; ee[k] = list[i].e;
    if (k == 2 && i + 1 < n) { char nx[128]; lower(list[i + 1].name, nx, sizeof nx); const char *pc = strstr(nx, "post"); /* credits, then a preview / post-credits scene: stop there */
      if (strstr(nx, "preview") || strstr(nx, "next episode") || strstr(nx, "next time") || strstr(nx, "stinger") || strstr(nx, "bonus") || (pc && (!strncmp(pc + 4, "credits", 7) || (pc[4] && !strncmp(pc + 5, "credits", 7))))) ee[k] = list[i + 1].s; } }
  if (n > 0) sk_add(0, has, ss, ee);
  sk_done();
}

/* ---- buttons ---- */
static void build_btns(void) {
  const char *fid = P.nbtn && P.bIdx < P.nbtn ? P.btns[P.bIdx].id : NULL; /* the focus stays on the same button when the row changes */
  P.nbtn = 0; int i = 0;
  #define B(ID, IC, LB, BIG) do { if (i >= MAXBTN) break; P.btns[i].id = ID; P.btns[i].icon = IC; snprintf(P.btns[i].lb, 48, "%s", LB); P.btns[i].big = BIG; P.btns[i].nx = 0; i++; } while (0)
  if (live_title() && live_catalogs() > 0) B("chn", "wn_live", T("Channels"), 1); else B("pp", "p_pause", "", 1); /* a channel: its big button is the channels (play / pause on the remote's key) */
  if (!live_title() && !live_bcast()) { B("rw", "p_rw", T("Back 10 s"), 0); B("ff", "p_ff", T("Forward 10 s"), 0); } /* etapa-87 */
  if (!P.isTrailer && P.next.has) { B("nxt", "p_next", T("Next episode"), 0); P.btns[i - 1].nx = 1; }
  if (!P.isTrailer && !live_title()) { B("beg", "restart", T("Play from the beginning"), 0); P.btns[i - 1].nx = 1; } /* films and episodes alike (a broadcast has no beginning) */
  P.nL = i;
  if (!P.isTrailer && episodic() && P.videos && cJSON_GetArraySize(P.videos)) B("eps", "p_eps", T("Episodes"), 0);
  if (!P.isTrailer && !live_title()) B("src", "p_src", T("Sources"), 0); /* (a channel: the working stream is found for it) */
  if (P.isTrailer) { if (trSubsN) B("cc", "p_subs", T("Subtitles"), 0); } /* a trailer's captions: one press on, one off */
  else if (!live_title()) B("subs", "p_subs", T("Subtitles"), 0); /* (a live channel: none to choose) */
  if (!P.isTrailer) B("aud", "p_aud", T("Audio"), 0);
  if (live_title() && live_catalogs() > 0 && multi_available()) B("mv", "wn_live", !strncmp(i18n_lang(), "pt", 2) ? (multi_on() ? "Uma tela" : "Multi-tela") : (multi_on() ? "One screen" : "Multi-screen"), 0);
#ifdef SB_ANDROIDTV
  if (!P.isTrailer && !live_title() && !strncmp(P.url, "http", 4)) B("ext", "play", T("Other player"), 0); /* etapa-90 */
#endif
  const char *am = !strcmp(P.aspect, "fill") ? "Fill the screen" : !strcmp(P.aspect, "zoom") ? "Zoom" : "Original"; B("asp", "p_asp", T(am), 0);
  #undef B
  P.nbtn = i; if (P.bIdx >= i) P.bIdx = 0; if (fid) for (int k = 0; k < i; k++) if (!strcmp(P.btns[k].id, fid)) P.bIdx = k;
  for (int k = 0; k < i; k++) { P.btns[k].lw = P.btns[k].lb[0] ? text_width(P.btns[k].lb, 24, 700, 0) + 22 : 0; P.btns[k].w = P.btns[k].big ? 94 : 78; }
  /* where they sit before any slides aside (76 / 92 plus a 1 px edge): the left group from the left, the right group from the right */
  float x = 72; for (int k = 0; k < P.nL; k++) { if (P.btns[k].nx) x += 8; P.btns[k].x0 = x; P.btns[k].y = SB_H - 54 - 94 + (94 - P.btns[k].w) / 2; x += P.btns[k].w + (P.btns[k].big ? 20 : 0); }
  x = SB_W - 92; for (int k = i - 1; k >= P.nL; k--) { x -= P.btns[k].w; P.btns[k].x0 = x; P.btns[k].y = SB_H - 54 - 94 - 4; x -= 50; } /* etapa-87: room for each name under it */
  for (int k = 0; k < i; k++) { P.btns[k].x = P.btns[k].x0 + P.btns[k].dx.v; }
}
static int btn_index(const char *id) { for (int i = 0; i < P.nbtn; i++) if (!strcmp(P.btns[i].id, id)) return i; return -1; }

/* ---- OSD ---- */
static void arm_hide(void) { P.hideAt = now_() + 5; }
static void chips_in(void);
static void show_osd(int keepFocus) {
  if (P.mode == 0) { P.mode = 1; if (!keepFocus) { P.row = 0; P.bIdx = 0; } }
  int was = P.osd; P.skm = 0; P.osd = 1; arm_hide();
  if (!was) chips_in(); /* the info chips come in again, one after the other */
}
static void hide_osd(void) { P.mode = 0; P.osd = 0; extern void app_pointer_rest(void); app_pointer_rest(); }
/* playing again: a live broadcast paused more than 3 s joins where the broadcast is now (it went on from the paused
   moment, no longer live); anything else goes on from where it stopped */
static void play_on(void) { awayHold = 0; /* (asked to play: a phone's, behind, too) */ if (live_bcast() && P.pzSince > 0 && now_() - P.pzSince > 3) { P.pzSince = 0; video_live_rejoin(); } else video_play(); }
static void toggle(void) { if (paused()) play_on(); else { video_pause(); save_progress(1); } }

/* ---- seeking: presses add up and commit 1.2 s after the last one ---- */
/* ---- chapters on the seek bar (Settings: Chapters): gaps at each start, the one under the knob named in the seek
   pill, a seek landing close to a start lands on it, Up / Down while seeking go from start to start ---- */
static int chapters_on(void) { return P.nchap >= 2 && !P.isTrailer && !live_title() && dur() > 0 && strcmp(store_get("showChapters", "true"), "false"); }
static int chap_at(double t) { int k = 0; for (int i = 0; i < P.nchap; i++) if (P.chap[i].s <= t + 0.25) k = i; return k; }
static const char *chap_name(int i, char *buf, size_t cap) { if (P.chap[i].nm[0]) return P.chap[i].nm; double cs = P.chap[i].s; /* unnamed: the opening, recap or credits it starts, else its number */
  if (P.intro.has && fabs(cs - P.intro.s) < 6) return T("Opening"); if (P.recap.has && fabs(cs - P.recap.s) < 6) return T("Recap"); if (P.outro.has && fabs(cs - P.outro.s) < 6) return T("End credits");
  char n[8]; snprintf(n, sizeof n, "%d", i + 1); snprintf(buf, cap, "%s", T1("Chapter {n}", "n", n)); return buf; }
static void seek_text(void) {
  int dd = (int)(P.seekT - video_time() + (P.seekT >= video_time() ? 0.5 : -0.5)), a = dd < 0 ? -dd : dd; char m[16], s[16]; snprintf(m, sizeof m, "%d", a / 60); snprintf(s, sizeof s, "%d", a % 60);
  if (a >= 60) snprintf(P.seekTxt, sizeof P.seekTxt, "%s%s%s%s", dd >= 0 ? "+" : "-", T1("{m}m", "m", m), a % 60 ? " " : "", a % 60 ? T1("{s}s", "s", s) : ""); else snprintf(P.seekTxt, sizeof P.seekTxt, "%s%s", dd >= 0 ? "+" : "-", T1("{s}s", "s", s));
}
static void chap_jump(int dir) { /* to the next chapter's start, or back to this one's (the one before, within its first 3 s) */
  double d = dur(), base = P.seekT >= 0 ? P.seekT : video_time(); int k = chap_at(base), to = -1;
  if (dir > 0) { if (k + 1 < P.nchap) to = k + 1; } else to = base - P.chap[k].s > 3 ? k : (k > 0 ? k - 1 : 0);
  if (to < 0) { nudge_start(&P.nudge, K_UP); return; }
  double n = now_(); P.seekT = P.chap[to].s < d - 1 ? P.chap[to].s : d - 1; P.seekDir = dir; P.rep = 0; P.lastKeyAt = n; P.seekScrub = 0;
  if (P.mode == 0) P.skm = 1; else arm_hide();
  seek_text(); P.skOn = 1; P.seekTxtUntil = 0; P.seekCommitAt = n + 1.6;
}
static double seekStartAt; static Tween seekK; static double stripT = -1, stripFrom; static int stripHold; static double stripMovedAt; /* stripHold: the strip up (a scrub, or just after one); stripMovedAt: its time last changed */ /* (the previews' strip: its motion, the time it shows, where the scrub began) */
static void seek_by(int dir) {
  if (live_bcast()) return; /* a broadcast: no moving in it (its window is the TV's, not ours) */
  double d = dur(); if (d <= 0) return; double n = now_(); int step = atoi(store_get("seekStep", "10")); if (!step) step = 10;
  P.rep = (n - P.lastKeyAt < 0.4 && dir == P.seekDir) ? P.rep + 1 : 0; P.lastKeyAt = n; P.seekDir = dir; step *= P.rep < 4 ? 1 : P.rep < 10 ? 3 : 6;
  if (P.seekT < 0) seekStartAt = n; /* (a new scrub: the previews' progress shows a moment) */
  double base = P.seekT >= 0 ? P.seekT : video_time(); P.seekT = base + dir * step; if (P.seekT < 0) P.seekT = 0; if (P.seekT > d - 1) P.seekT = d - 1; P.seekScrub = 1;
  if (chapters_on()) { double mg = step * 0.45 < 8 ? step * 0.45 : 8; for (int i = 1; i < P.nchap; i++) { double c = P.chap[i].s; if (fabs(c - P.seekT) <= mg && (dir > 0 ? c > base : c < base)) { P.seekT = c; break; } } } /* close to a chapter's start: on it */
  if (P.mode == 0) P.skm = 1; else arm_hide();
  seek_text();
  P.skOn = 1; /* the pill stays put while it is up: only its number changes */ P.seekTxtUntil = 0; P.seekCommitAt = n + 1.2;
}
static int stripQuick; /* OK on a picture: the strip goes at once (the film is on its way there), not with the slow fade of letting go */
static void commit_seek(void);
static void commit_ok(void) { if (P.seekT >= 0 && P.seekScrub) stripQuick = 1; commit_seek(); }
static void commit_seek(void) { if (P.seekT < 0) return; if (live_bcast()) { P.seekT = -1; return; } double t = P.seekT; P.seekT = -1;
  int scrub = P.seekScrub; P.seekScrub = 0;
  if (scrub && prev_enabled() && sp_active()) { double sh = sp_shown_t(); fprintf(stderr, "[seek] accuracy: the picture shown is %.1f s from where it lands (%.1f s; a picture every %.0f s, %d made)\n", sh >= 0 ? sh - t : 999, t, sp_slot(), sp_made()); double sn = sp_snap(t); if (fabs(sn - t) > 0.05) fprintf(stderr, "[seek] %.2f s lands on the shown picture's keyframe at %.2f s\n", t, sn); t = sn; scrub = 0; } /* (the picture shown: the film lands on it) */
  if (scrub && getenv("SB_SEEKSNAP") && mkv && atoi(getenv("SB_SEEKSNAP")) && prev_enabled()) { int atChap = 0; for (int i = 0; i < P.nchap; i++) if (fabs(P.chap[i].s - t) < 0.05) atChap = 1; double k; /* (SB_SEEKSNAP, development, off: a scrub of the viewer's lands on the keyframe at or before it within 1.5 s, as the previews will show it; never a chapter jump, resume, skip or a source's same moment; a chapter's start kept) */
    if (!atChap && mkv_key_before(mkv, t, 1.5, &k)) { fprintf(stderr, "[seek] %.2f s snapped to the keyframe at %.2f s (%d known)\n", t, k, mkv_key_count(mkv)); t = k; }
  }
  if (getenv("SB_SEEKSNAP")) { P.seekSnapAt = now_(); P.seekSnapT = t; } /* (SB_SEEKSNAP=0: the times only, for the comparison) */
  video_seek(t); P.seekTxtUntil = now_() + 0.9; }
static void cancel_seek(void) { P.seekT = -1; P.skm = 0; P.skOn = 0; P.seekTxtUntil = 0; }

/* ---- picture size, three states always in this order: Original (the whole picture), Fill (fills the screen: a file
   of another shape is cropped to it; a 16:9 file, which already fills it, gets a light crop) and Zoom (a 16:9 file
   with cinema bars baked in loses them; any other shape is cropped further). Chosen per title (a show keeps it from
   episode to episode), else the default from Settings ---- */
static void aspect_key(char *k, size_t cap) { snprintf(k, cap, "aspect_%.*s", (int)strcspn(P.id, ":"), P.id); }
static void aspect_load(void) { char k[96]; aspect_key(k, sizeof k); const char *a = store_get(k, ""); if (!*a) a = store_get("aspectMode", "fit"); snprintf(P.aspect, sizeof P.aspect, "%s", strcmp(a, "stretch") ? a : "fit"); }
/* the web sizes the picture's box (Fill / Zoom go past the screen); a TV's plane blanks for a rectangle that leaves
   the screen, so the part of that box on the screen is shown as a crop of the frame instead */
void player_reapply_window(void);
static float vwin[4]; /* etapa-90: where the picture sits when it has bars round it (w 0: none) */
static void apply_aspect(void) {
  vwin[2] = 0;
  int vw = video_width(), vh = video_height(); float W = SB_W, H = SB_H; P.lastVw = vw; P.lastVh = vh;
  if (growPending || growAt > 0 || shrinkAt > 0) return; /* a preview growing or shrinking places the picture itself every frame (its framing comes as it ends): never a full-screen jump in the middle */
  if (!vw || !vh) { video_set_window(0, 0, W, H); return; }
  float fit = fminf(W / vw, H / vh), fill = fmaxf(W / vw, H / vh), k = fit; int wide = fabsf((float)vw / vh - W / H) < 0.03f;
  if (!strcmp(P.aspect, "fill")) k = wide ? fit * 1.12f : fill; else if (!strcmp(P.aspect, "zoom")) k = wide ? fit * 2.39f / (W / H) : fill * 1.15f;
  float fw = roundf(vw * fit), fh = roundf(vh * fit), fx = roundf((W - fw) / 2), fy = roundf((H - fh) / 2);
  if (k == fit) { video_set_window(fx, fy, fw, fh); vwin[0] = fx; vwin[1] = fy; vwin[2] = fw; vwin[3] = fh; return; }
  float w = roundf(vw * k), h = roundf(vh * k), x = roundf((W - w) / 2), y = roundf((H - h) / 2);
  float dx = fmaxf(0, x), dy = fmaxf(0, y), dw = fminf(W, x + w) - dx, dh = fminf(H, y + h) - dy;
  int sx = (int)roundf((dx - x) / k), sy = (int)roundf((dy - y) / k), sw = (int)roundf(dw / k), sh = (int)roundf(dh / k);
  if (sx + sw > vw) sw = vw - sx; if (sy + sh > vh) sh = vh - sy;
  if (!video_set_crop(sx, sy, sw, sh, dx, dy, dw, dh)) video_set_window(fx, fy, fw, fh); /* no crop on this TV: the picture stays whole */
}
static void next_aspect(void) {
  static const char *A[3] = { "fit", "fill", "zoom" }; static const char *N[3] = { "Original", "Fill the screen", "Zoom" }; int i = 0; for (int k = 0; k < 3; k++) if (!strcmp(A[k], P.aspect)) i = k;
  int n = (i + 1) % 3; char key[96]; aspect_key(key, sizeof key);
  snprintf(P.aspect, sizeof P.aspect, "%s", A[n]); store_set(key, P.aspect); apply_aspect();
  /* paused: the TV shows the new size with its next frame, when play resumes (asking for the same frame again was a
     seek: the stream buffered, a trailer jumped back) */
  int b = btn_index("asp"); if (b >= 0) { snprintf(P.btns[b].lb, 48, "%s", T(N[n])); P.btns[b].lw = text_width(P.btns[b].lb, 24, 700, 0) + 22; } /* the button names the mode it is in */
}

/* ---- prompts: skip intro / recap, the credits card ---- */
static int in_seg(int has, double s, double e, double t, double tail) { return has && t >= s && t < e - tail; }
static void paint_prompt(void) { if (P.prompt.k) P.shown = P.prompt; }
static void go_next(void);
static void next_auto(void);
static void dismiss_prompt(void) { if (P.prompt.k) P.dismissed[P.prompt.k] = 1; P.prompt.k = 0; if (P.mode == 3) P.mode = 0; paint_prompt(); }
static void prompt_go(int i) {
  if (!P.prompt.k || i >= P.prompt.nbtn) return; const char *id = P.prompt.b[i].id;
  if (!strcmp(id, "skip")) { P.dismissed[P.prompt.k] = 1; double to = P.prompt.b[i].to; P.prompt.k = 0; if (P.mode == 3) P.mode = 0; paint_prompt(); video_seek(to); }
  else if (!strcmp(id, "credits")) dismiss_prompt();
  else if (!strcmp(id, "coll")) { mark_watched(); save_progress(1); closeColl = 1; close_player(0, 0); } /* the next film: its page, its streams open */
  else go_next();
}
static void check_prompt(void) {
  double d = dur(); if (d <= 0 || P.seekT >= 0) return; double t = video_time(); int want = 0, big = 0;
  int skipOn = strcmp(skip_mode(), "off") != 0; /* Off: no intro / recap button at all (the next episode's card stays) */
  if (skipOn && in_seg(P.intro.has, P.intro.s, P.intro.e, t, 2) && !P.dismissed[1]) want = 1;
  else if (skipOn && in_seg(P.recap.has, P.recap.s, P.recap.e, t, 2) && !P.dismissed[2]) want = 2;
  else if (!P.dismissed[3] && (P.next.has || P.coll.has || P.sim)) { int credits = P.outro.has ? t >= P.outro.s : (d > 600 && d - t <= 90 && d - t > 2); if (credits) { want = 3; big = 1; } }
  if (want == P.prompt.k) return;
  P.prompt.k = want; P.prompt.big = big; P.prompt.autoN = 0; P.prompt.nbtn = 0;
  if (want == 1 || want == 2) { P.prompt.b[0].id = "skip"; snprintf(P.prompt.b[0].lb, 48, "%s", T(want == 1 ? "Skip intro" : "Skip recap")); P.prompt.b[0].to = want == 1 ? P.intro.e : P.recap.e; P.prompt.b[0].main = 0; P.prompt.nbtn = 1;
    /* the pill fills up and skips by itself when full (about 6 s, less when the part ends sooner); OK skips now, a pause stops it */
    if (!paused() && !strcmp(skip_mode(), "auto")) { double e = want == 1 ? P.intro.e : P.recap.e, dd = e - t - 1; P.prompt.autoN = 1; P.prompt.t0 = now_(); P.prompt.adur = dd < 2.5 ? 2.5 : dd > 6 ? 6 : dd; } }
  else if (want == 3) { P.prompt.b[0].id = "credits"; snprintf(P.prompt.b[0].lb, 48, "%s", T("Watch credits")); P.prompt.b[0].main = 0; P.prompt.b[1].id = P.next.has ? "next" : P.coll.has ? "coll" : "sim"; snprintf(P.prompt.b[1].lb, 48, "%s", P.next.has ? T("Next episode") : P.coll.has ? T("Next film") : T("Something similar")); P.prompt.b[1].main = 1; P.prompt.nbtn = 2;
    /* credits + autoplay on: "Next episode" fills up and plays by itself unless you pick something */
    if (P.next.has && strcmp(store_get("bingeWatch", "true"), "false") && strcmp(store_get("enableNextVideo", "true"), "false")) { P.prompt.autoN = 1; P.prompt.t0 = now_(); P.prompt.adur = 10; } }
  P.prompt.idx = P.prompt.nbtn ? P.prompt.nbtn - 1 : 0;
  if (want && big && P.mode == 0) P.mode = 3; /* the credits card takes the focus */
  if (!want && P.mode == 3) P.mode = 0;
  paint_prompt();
}

/* ---- next episode: played straight away from the source most like the current one (same addon, resolution, HDR,
   audio, release group, debrid vs torrent); the art covers the switch. With no source at all: the title page's list ---- */
static char closeVid[SB_VID];
static void play_stream(const Stream *s, const char *vid, double startAt);
static void prepare(const char *vid, double startAt, int keepSubs);
static int can_play(const Stream *s) { return !(s->p2p || !strncmp(s->url, "p2p://", 6)) || p2p_available(); } /* torrents need the engine library beside the app */
static void rel_group(const char *s, char *out, size_t cap) { /* "-GROUP" at the end of a word (before .mkv / .mp4 / .avi) */
  out[0] = 0; for (const char *p = s; (p = strchr(p, '-')); p++) { const char *q = p + 1; while (isalnum((unsigned char)*q)) q++; size_t n = q - p - 1; if (n < 2 || n > 12) continue;
    const char *r = q; if (*r == '.' && (!strncasecmp(r + 1, "mkv", 3) || !strncasecmp(r + 1, "mp4", 3) || !strncasecmp(r + 1, "avi", 3))) r += 4;
    if (*r && !isspace((unsigned char)*r)) { if (r == q) continue; r = q; if (*r && !isspace((unsigned char)*r)) continue; }
    snprintf(out, cap, "%.*s", (int)n, p + 1); for (char *c = out; *c; c++) *c = (char)tolower((unsigned char)*c); return; }
}
static const char *src_kind(const char *label) {
  char s[700]; lower(label, s, sizeof s); if (strstr(s, "remux")) return "remux"; if (strstr(s, "bluray") || strstr(s, "blu-ray") || strstr(s, "bdrip") || strstr(s, "brrip")) return "bluray";
  int nf = 0; for (const char *p = s; (p = strstr(p, "nf")); p++) if (!isalnum((unsigned char)p[2]) && p[2] != '_') nf = 1;
  if (strstr(s, "webdl") || strstr(s, "web-dl") || strstr(s, "webrip") || has_word(s, "web") || strstr(s, "amzn") || nf || strstr(s, "dsnp") || strstr(s, "hmax") || strstr(s, "atvp")) return "web";
  return strstr(s, "hdtv") ? "tv" : "";
}
static int cached_flag(const char *label) { char s[700]; lower(label, s, sizeof s); if (strstr(s, "cached") || strstr(s, "\xe2\x9a\xa1")) return 1;
  for (const char *p = s; (p = strchr(p, '[')); p++) { int n = 0; while (p[1 + n] >= 'a' && p[1 + n] <= 'z') n++; if (n >= 2 && n <= 3 && p[1 + n] == '+' && p[2 + n] == ']') return 1; } return 0; }
static void info_hash(const char *u, char *out) { out[0] = 0; const char *p = NULL; if (!strncmp(u, "p2p://", 6)) p = u + 6; else if ((p = strstr(u, "ih="))) p += 3; else if ((p = strstr(u, "/torrents/"))) p += 10;
  if (!p) return; int n = 0; while (n < 40 && isxdigit((unsigned char)p[n])) n++; if (n == 40) { for (int i = 0; i < 40; i++) out[i] = (char)tolower((unsigned char)p[i]); out[40] = 0; } }
static int likeness(const Stream *s) {
  char b[700]; sb_stream_info(s, b, sizeof b); const char *a = P.nx.info; int sc = 0;
  if (P.nx.addon[0] && !strcmp(s->addon, P.nx.addon)) sc += 20;
  static const char *RS[5] = { "SD", "720p", "1080p", "1440p", "4K" }; const char *r1 = res_of(a), *r2 = res_of(b);
  if (*r1 && !strcmp(r1, r2)) sc += 30; else if (*r1 && *r2) { int i1 = 0, i2 = 0; for (int i = 0; i < 5; i++) { if (!strcmp(RS[i], r1)) i1 = i; if (!strcmp(RS[i], r2)) i2 = i; } sc -= 12 * abs(i1 - i2); }
  char h1[32], h2[32]; hdr_of(a, h1, sizeof h1); hdr_of(b, h2, sizeof h2); if (!strcmp(h1, h2)) sc += 12;
  char a1[48], a2[48]; audio_of(a, a1, sizeof a1); audio_of(b, a2, sizeof a2); a1[strcspn(a1, " ")] = 0; a2[strcspn(a2, " ")] = 0; if (*a1 && !strcmp(a1, a2)) sc += 10;
  char g1[16], g2[16]; rel_group(a, g1, sizeof g1); rel_group(b, g2, sizeof g2); if (*g1 && !strcmp(g1, g2)) sc += 25;
  const char *k1 = src_kind(a), *k2 = src_kind(b); if (*k1 && !strcmp(k1, k2)) sc += 10;
  int c1 = cached_flag(a), c2 = cached_flag(b); if (c1 == c2) sc += c1 ? 15 : 5;
  int p1 = !strncmp(P.nx.url, "p2p://", 6), p2 = !strncmp(s->url, "p2p://", 6); sc += p1 == p2 ? 15 : -15;
  char x1[48], x2[48]; info_hash(P.nx.url, x1); info_hash(s->url, x2); if (*x1 && !strcmp(x1, x2)) sc += 60; /* the same season pack: the next file of the very same torrent */
  return sc;
}
static unsigned nxTok;
/* the next episode's seek previews made ahead: at 80 % of this one, its stream chosen as the next episode will choose
   it (the closest to this one: likeness), its index read alone (no subtitles, nothing played) and 48 pictures made
   from it, only while this one has a good buffer (seekprev's prep); kept on this device, so the next episode opens
   with them. A torrent never. */
static struct { char vid[SB_VID], key[300]; unsigned tok; int has, done, prepped; double bs, at; Stream best; MkvSession *m; } NP;
static void np_stop(void) { if (NP.m || NP.prepped) sp_prepare(NULL, NULL); if (NP.m) { mkv_close(NP.m); NP.m = NULL; } NP.tok++; NP.has = NP.done = NP.prepped = 0; }
static int likeness_to_this(const Stream *s) { __typeof__(P.nx) keep = P.nx; snprintf(P.nx.info, sizeof P.nx.info, "%s", P.sname); snprintf(P.nx.addon, sizeof P.nx.addon, "%s", P.saddon); snprintf(P.nx.url, sizeof P.nx.url, "%s", P.st.url[0] ? P.st.url : P.url); int v = likeness(s); P.nx = keep; return v; }
static void np_part(StreamGroup *g, int gi, void *ud) { (void)gi;
  if (P.open && (unsigned)(uintptr_t)ud == NP.tok && !NP.done) for (int i = 0; i < g->n; i++) { const Stream *s = &g->items[i]; if (!s->url[0] || s->p2p || strncmp(s->url, "http", 4)) continue;
      int v = P.st.binge[0] && !strcmp(s->binge, P.st.binge) ? 1000 : likeness_to_this(s); if (!NP.has || v > NP.bs) { NP.bs = v; NP.best = *s; NP.has = 1; } }
  free(g->items); }
static void np_done(int total, void *ud) { (void)total; if (!P.open || (unsigned)(uintptr_t)ud != NP.tok || NP.done) return; NP.done = 1;
  if (!NP.has) { fprintf(stderr, "[player] next episode's previews: no direct or debrid stream for it\n"); return; }
  snprintf(NP.key, sizeof NP.key, "%s|%s|%.0f|0", P.id, NP.vid, NP.best.size); NP.m = mkv_open_keys(NP.best.url); NP.at = now_();
  fprintf(stderr, "[player] next episode's previews: made ahead from %s (%.1f GB)\n", NP.best.addon, NP.best.size / 1e9); }
static void np_tick(void) {
  if (!P.open || !P.started || !P.live || P.isTrailer || P.isP2p || !P.next.has || !P.next.vid[0] || !prev_enabled() || !prev_available() || live_title()) return;
  double d = dur(); if (d < 600) return;
  if (strcmp(NP.vid, P.next.vid)) { if (video_time() < d * 0.8) return; np_stop(); snprintf(NP.vid, sizeof NP.vid, "%s", P.next.vid); sb_streams(P.type, NP.vid, np_part, np_done, (void *)(uintptr_t)NP.tok); return; } /* (at 80 %: once for that episode) */
  if (NP.m && !NP.prepped) { if (mkv_key_count(NP.m) >= 4) { sp_prepare(NP.m, NP.key); NP.prepped = 1; } else if (mkv_dead(NP.m) || now_() - NP.at > 30) { mkv_close(NP.m); NP.m = NULL; } } }
static void nx_finish(void) {
  if (!P.nx.tok || P.nx.done) return; P.nx.done = 1; P.nx.tok = 0;
  if (P.nx.has) { Stream b = P.nx.best; play_stream(&b, P.nx.vid, 0); }
  else { snprintf(closeVid, sizeof closeVid, "%s", P.nx.vid); close_player(1, 0); }
}
static void nx_part(StreamGroup *g, int gi, void *ud) {
  (void)gi; if (P.open && P.nx.tok && (unsigned)(uintptr_t)ud == P.nx.tok && !P.nx.done) {
    /* the same bingeGroup is the addon's own word that it is the same kind of file: taken at once (the official rule) */
    if (P.nx.binge[0]) for (int i = 0; i < g->n; i++) { const Stream *s = &g->items[i]; if (s->url[0] && can_play(s) && !strcmp(s->binge, P.nx.binge)) { P.nx.best = *s; P.nx.has = 1; P.nx.bs = 1000; free(g->items); fprintf(stderr, "[player] next episode: same binge group (%s)\n", P.nx.binge); nx_finish(); return; } }
    for (int i = 0; i < g->n; i++) { const Stream *s = &g->items[i]; if (!s->url[0] || !can_play(s)) continue; int v = likeness(s); if (!P.nx.has || v > P.nx.bs) { P.nx.bs = v; P.nx.best = *s; P.nx.has = 1; } }
    free(g->items); if (P.nx.has && P.nx.bs >= 95) nx_finish(); return; } /* same addon, quality, group: no need to wait for the slow ones */
  free(g->items);
}
static void nx_done(int total, void *ud) { (void)total; if (P.open && P.nx.tok && (unsigned)(uintptr_t)ud == P.nx.tok) nx_finish(); }
static void next_auto(void) {
  if (!P.next.has) { go_next(); return; }
  mark_watched(); save_progress(1);
  char nv[SB_VID], info[700], url[SB_URL_MAX], addon[96], binge[96]; snprintf(nv, sizeof nv, "%s", P.next.vid); snprintf(info, sizeof info, "%s", P.sname); snprintf(url, sizeof url, "%s", P.st.url[0] ? P.st.url : P.url); snprintf(addon, sizeof addon, "%s", P.saddon); snprintf(binge, sizeof binge, "%s", P.st.binge);
  prepare(nv, 0, 0);
  snprintf(P.nx.vid, sizeof P.nx.vid, "%s", nv); snprintf(P.nx.info, sizeof P.nx.info, "%s", info); snprintf(P.nx.binge, sizeof P.nx.binge, "%s", binge); snprintf(P.nx.url, sizeof P.nx.url, "%s", url); snprintf(P.nx.addon, sizeof P.nx.addon, "%s", addon);
  set_status(T("Finding the closest source\xe2\x80\xa6")); P.nx.tok = ++nxTok; if (!P.nx.tok) P.nx.tok = ++nxTok; P.nx.until = now_() + 15;
  sb_streams(P.type, nv, nx_part, nx_done, (void *)(uintptr_t)P.nx.tok);
}
static void go_next(void) {
  if (P.next.has && !P.nextTried) { P.nextTried = 1; next_auto(); }
  else if (P.next.has) { mark_watched(); snprintf(closeVid, sizeof closeVid, "%s", P.next.vid); close_player(1, 0); }
  else if (P.sim) close_player(0, 1);
}

#include "screens/player_trays.inc" /* etapa-89: the trays: sources, episodes, audio, subtitles */
/* ---- keys ---- */
/* from the very start: the skips and the credits card are offered again */
static void from_start(void) { cancel_seek(); memset(P.dismissed, 0, sizeof P.dismissed); P.prompt.k = 0; paint_prompt(); P.ended = 0; P.endGoAt = 0; video_seek(0); if (paused()) video_play(); show_osd(1); }
static struct { float x[4], y, w[4], h; } DG; /* the card's buttons, for the pointer */
static void dly_draw(void);
static void pos_draw(void);
static void live_card_draw(void);
/* ---- the delay card: the subtitles' timing set while watching. A small card at the top with - and + (a quarter of a
   second each), the delay between them, play / pause and Done; the picture and the lines stay in view, so a change is
   seen at once. Back or Done closes it (mode 4) ---- */
#define DLY_STEP 0.25
static void dly_open_k(int kind) { if (kind && live_title()) { toast(T("Not on live channels"), 2500, NULL); return; } close_tray(); P.osd = 0; P.mode = 4; P.dlyKind = kind; P.dlyF = 0; mo_set(&P.dlyR, 0); mo_to(&P.dlyA, 1, .28f, C_OUT, 0); }
static void dly_open(void) { close_tray(); P.osd = 0; P.mode = 4; P.dlyKind = 0; P.dlyF = 0; mo_set(&P.dlyR, 0); mo_to(&P.dlyA, 1, .28f, C_OUT, 0); }
static void dly_close(void) { P.mode = 0; mo_to(&P.dlyA, 0, .2f, C_EASE, 0); }
static void dly_change(double d) {
  if (P.dlyKind) { double v = P.audDelay + d * 0.2; v = floor(v / 0.05 + 0.5) * 0.05; if (v > 5) v = 5; if (v < -5) v = -5; P.audDelay = fabs(v) < 1e-6 ? 0 : v; P.dlyAt = now_(); video_audio_delay((int)lround(P.audDelay * 1000)); return; } /* the sound: 50 ms steps, 5 s at most */
  double v = P.subDelay + d; v = floor(v / DLY_STEP + 0.5) * DLY_STEP; if (v > 120) v = 120; if (v < -120) v = -120; P.subDelay = fabs(v) < 1e-6 ? 0 : v; P.dlyAt = now_(); }
static void dly_act(int i) { P.dlyF = i; mo_to(&P.dlyR, (float)i, .18f, C_OUT, 0);
  if (i == 0) dly_change(-DLY_STEP); else if (i == 1) dly_change(DLY_STEP); else if (i == 2) toggle(); else dly_close(); }
static void dly_key(int k) {
  if (k == K_LEFT) { if (P.dlyF > 0) { P.dlyF--; mo_to(&P.dlyR, (float)P.dlyF, .18f, C_OUT, 0); } else nudge_start(&P.nudge, k); }
  else if (k == K_RIGHT) { if (P.dlyF < 3) { P.dlyF++; mo_to(&P.dlyR, (float)P.dlyF, .18f, C_OUT, 0); } else nudge_start(&P.nudge, k); }
  else if (k == K_OK) dly_act(P.dlyF);
  else if (k == K_UP || k == K_DOWN) nudge_start(&P.nudge, k);
}
/* the position card (mode 5): the tray steps away so the subtitles sit where they will; Up and Down move them 1 % at a
   time, gliding there, a sample line standing in when none shows; OK or Back is done */
/* every subtitle style is set where it can be seen: the tray steps away, the subtitles (or a sample line) sit where they
   will, the card at the top holds that style's choices; size, colour and background move with Left / Right and show at
   once, the height with Up / Down */
static void style_open(int kind) { close_tray(); P.osd = 0; P.mode = 5; P.styK = kind; P.posAt = 0; mo_to(&P.posA, 1, .28f, C_OUT, 0); }
static void pos_open(void) { style_open(0); }
static void style_back(int kind) { /* the subtitle tray again, its Style tab, the row of the style just set */
  subTab = (P.nSubAddons > 0 || P.nAdx > 0) ? 2 : 1; show_osd(1); open_tray(4); P.tray.zone = 2; P.tray.c = 0; /* (the controls with it, as it was opened: a tray never floats alone) */
  static const char *const U[7] = { "pos", "size", "color", "bg", "bright", "font", "fontar" }; TCol *c = &P.tray.cols[0]; for (int i = 0; i < c->n; i++) if (!strcmp(c->items[i].u, U[kind >= 0 && kind < 7 ? kind : 0])) P.tray.r[0] = i; }
static int style_idx(void) { int c = color_idx(); return P.styK == 1 ? size_idx() : P.styK == 2 ? (c < 0 ? 0 : c) : P.styK == 4 ? bright_idx() : P.styK == 5 ? subfont_lat() : P.styK == 6 ? subfont_ar() : bg_idx(); }
static int style_n(void) { return P.styK == 1 ? 7 : P.styK == 4 ? 4 : P.styK >= 5 ? SUBF_N : 3; }
static void style_change(int d) {
  int i = style_idx() + d; if (i < 0 || i >= style_n()) { nudge_start(&P.nudge, d > 0 ? K_RIGHT : K_LEFT); return; }
  if (P.styK == 1) { char v[4]; snprintf(v, sizeof v, "%d", i); store_set("subtitleSize", v); } else if (P.styK == 2) store_set("subsColor", SUB_COLORS[i]); else if (P.styK == 4) store_set(bright_key(), SUB_BRIGHT[i]); else if (P.styK >= 5) { char v[4]; snprintf(v, sizeof v, "%d", i); store_set(P.styK == 5 ? "subsFont" : "subsFontAr", v); } else store_set("subsBgStyle", SUB_BGS[i]);
  P.posAt = now_(); }
static void pos_close(void) { P.mode = 0; mo_to(&P.posA, 0, .2f, C_EASE, 0); }
static void pos_change(int d) { int v = pos_pct() + d; if (v < POS_MIN || v > POS_MAX) { nudge_start(&P.nudge, d > 0 ? K_UP : K_DOWN); return; } char b[8]; snprintf(b, sizeof b, "%d", v); store_set("subsPosPct", b); P.posAt = now_(); }
static void pos_key(int k) {
  if (P.styK) { if (k == K_LEFT || k == K_RIGHT) style_change(app_mirror_key(k) == K_RIGHT ? 1 : -1); else if (k == K_OK) pos_close(); else if (k == K_UP || k == K_DOWN) nudge_start(&P.nudge, k); return; }
  if (k == K_UP) pos_change(1); else if (k == K_DOWN) pos_change(-1); else if (k == K_OK) pos_close(); else if (k == K_LEFT || k == K_RIGHT) nudge_start(&P.nudge, k); }
static struct { float y, h, ux, uw, dx, dw, ox, ow; } PZ; /* the card's parts, for the pointer */
static void hub_from_player(void) { close_tray(); P.osd = 0; P.mode = 6; hub_open(1, P.id); } /* the live hub over the channel playing (mode 6) */
static void close_player(int goNext, int goSim);
#ifdef SB_ANDROIDTV
void plat_open_external_video(const char *url, const char *title, double startSec);
#endif
/* etapa-90: Other player: the stream as it plays, from where it is, in a player of the TV's; this one closes (its place kept) */
static void open_external_now(void) {
#ifdef SB_ANDROIDTV
  if (strncmp(P.url, "http", 4)) { toast(T("Torrents play only here"), 2500, NULL); return; }
  char t[300]; if (episodic() && P.episode > 0) snprintf(t, sizeof t, "%s S%d:E%d", P.name, P.season, P.episode); else snprintf(t, sizeof t, "%s", P.name);
  plat_open_external_video(P.url, t, video_time()); close_player(0, 0);
#endif
}
static void act(const char *id) {
  if (!strcmp(id, "ext")) { open_external_now(); return; } if (!strcmp(id, "pp")) toggle(); else if (!strcmp(id, "rw")) seek_by(-1); else if (!strcmp(id, "ff")) seek_by(1); else if (!strcmp(id, "beg")) from_start(); else if (!strcmp(id, "asp")) next_aspect(); else if (!strcmp(id, "nxt")) next_auto(); else if (!strcmp(id, "cc")) cc_toggle();
  else if (!strcmp(id, "chn")) { hub_from_player(); return; }
  else if (!strcmp(id, "mv")) { multi_button(); if (multi_armed()) hub_from_player(); else build_btns(); return; }
  else open_tray(!strcmp(id, "eps") ? 1 : !strcmp(id, "src") ? 2 : !strcmp(id, "aud") ? 3 : 4);
  arm_hide();
}
static void back(void) {
  if (P.closing) return;
  if (P.mode == 2) { tray_back(); return; }
  if (P.mode == 4 && P.dlyKind) { dly_close(); open_tray(3); return; } /* Back: the Audio tray again */
  if (P.mode == 4) { dly_close(); if (!P.isTrailer) { subTab = (P.nSubAddons > 0 || P.nAdx > 0) ? 2 : 1; open_tray(4); P.tray.zone = 2; P.tray.c = 1; TCol *c = &P.tray.cols[1]; for (int i = 0; i < c->n; i++) if (!strcmp(c->items[i].u, "dlyopen")) P.tray.r[1] = i; } return; } /* Back: the tray again, on Adjust */
  if (P.mode == 5) { int k = P.styK; pos_close(); if (!P.isTrailer) style_back(k); return; } /* Back: the tray again, on that style */
  if (P.mode == 6) { if (!hub_back()) { hub_close(); P.mode = 0; } return; }
  if (P.adopt && adoptFull && !P.failed && lpk_can_return(P.id) && (P.mode == 0 || P.mode == 1)) { hide_osd(); if (lpk_take_back(P.id)) { fprintf(stderr, "[player] back to All channels, the channel playing on behind it\n"); keepVideo = 1; adoptFull = 0; finish_close(); } else close_player(0, 0); return; } /* taken full screen from behind All channels: back there, playing on */
  if (P.adopt && !P.failed && shrinkAt <= 0 && (P.live || growPending || growAt > 0) && lpk_can_return(P.id) && (P.mode == 0 || P.mode == 1)) { hide_osd(); grow_reverse(); return; } /* grown from its preview: back into the box, playing on */ /* the hub: from the channels up to the catalogs, then away */
  if (P.prompt.k && P.prompt.autoN) { P.prompt.autoN = 0; paint_prompt(); return; } /* a countdown running (skip, next episode): Back stops it, the controls up or not; the prompt stays */
  if (P.mode == 3) { dismiss_prompt(); return; }
  if (P.seekT >= 0) { cancel_seek(); return; }
  if (P.mode == 1 && !paused()) { hide_osd(); return; }
  close_player(0, 0);
}
static void ss_exit(void) { P.ssOn = 0; P.lastAct = now_(); }
static void try_next(void);

/* ---- X-Ray (as Prime Video's): Up while the film plays opens a panel at the left, the film going on behind it:
   Cast (OK on a face: the person, as Apple TV's card) and Trivia. Back closes the card, then the panel ---- */
static struct { int on, tab, zone, idx, card, cardPid; Mo a, card_a; float scroll; char cardRole[96], cardName[80], cardPhoto[200]; } XR; /* zone 0 the tabs, 1 the list */
static void xr_open(void) { if (P.isTrailer || live_title()) return; hide_osd(); XR.on = 1; XR.zone = 1; XR.idx = 0; XR.card = 0; XR.scroll = 0; mo_to(&XR.a, 1, .28f, C_OUT, 0); sound_focus(); }
static void xr_close(void) { XR.on = 0; XR.card = 0; mo_to(&XR.a, 0, .22f, C_OUT, 0); mo_to(&XR.card_a, 0, .18f, C_OUT, 0); }
static int xr_count(void) { if (XR.tab == 0) { const InsightFace *fc; int ep = episodic(); return insight_get(P.type, P.id, ep ? P.season : -1, ep ? P.episode : -1, &fc); } const char **tl; int st; return insight_trivia(P.id, &tl, &st); }
static void xr_key(int k) {
  if (XR.card) { if (k == K_BACK || k == K_OK || k == K_LEFT) { XR.card = 0; mo_to(&XR.card_a, 0, .18f, C_OUT, 0); sound_focus(); } return; }
  int n = xr_count();
  if (k == K_BACK) { xr_close(); return; }
  if (XR.zone == 0) {
    if (k == K_LEFT || k == K_RIGHT) { int t = k == K_RIGHT ? 1 : 0; if (t != XR.tab) { XR.tab = t; XR.idx = 0; XR.scroll = 0; sound_focus(); } else nudge_start(&P.nudge, k); }
    else if (k == K_DOWN || k == K_OK) { if (n) { XR.zone = 1; sound_focus(); } else nudge_start(&P.nudge, k); }
    else if (k == K_UP) nudge_start(&P.nudge, k);
    return; }
  if (k == K_UP) { if (XR.idx > 0) XR.idx--; else XR.zone = 0; sound_focus(); }
  else if (k == K_DOWN) { if (XR.idx < n - 1) { XR.idx++; sound_focus(); } else nudge_start(&P.nudge, k); }
  else if (k == K_RIGHT) { if (XR.tab == 0) { XR.tab = 1; XR.idx = 0; XR.scroll = 0; sound_focus(); } else nudge_start(&P.nudge, k); }
  else if (k == K_LEFT) { if (XR.tab == 1) { XR.tab = 0; XR.idx = 0; XR.scroll = 0; sound_focus(); } else xr_close(); }
  else if (k == K_OK && XR.tab == 0 && XR.idx < n) { const InsightFace *fc; int ep = episodic(); if (insight_get(P.type, P.id, ep ? P.season : -1, ep ? P.episode : -1, &fc) > XR.idx) {
      const InsightFace *f = &fc[XR.idx]; XR.card = 1; XR.cardPid = f->pid; snprintf(XR.cardName, sizeof XR.cardName, "%s", f->name); snprintf(XR.cardRole, sizeof XR.cardRole, "%s", f->role); snprintf(XR.cardPhoto, sizeof XR.cardPhoto, "%s", f->photo);
      insight_person(f->pid); mo_to(&XR.card_a, 1, .26f, C_OUT, 0); sound_focus(); } }
}
static void xr_date(const char *ymd, char *out, size_t cap) { int y = 0, m = 0, d = 0; out[0] = 0; static const char *MO[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
  if (sscanf(ymd, "%d-%d-%d", &y, &m, &d) == 3 && m >= 1 && m <= 12) snprintf(out, cap, "%d %s %d", d, T(MO[m - 1]), y); }
static int xr_age(const char *born, const char *died) { int y, m, d, y2, m2, d2; if (sscanf(born, "%d-%d-%d", &y, &m, &d) != 3) return -1;
  if (!died || sscanf(died, "%d-%d-%d", &y2, &m2, &d2) != 3) { time_t t = time(NULL); struct tm tm; localtime_r(&t, &tm); y2 = tm.tm_year + 1900; m2 = tm.tm_mon + 1; d2 = tm.tm_mday; }
  return y2 - y - ((m2 < m || (m2 == m && d2 < d)) ? 1 : 0); }
static void xr_draw(void) {
  { static double lt; double nw = now_(); float fdt = lt ? (float)(nw - lt) : 0; lt = nw; if (fdt > 0.05f) fdt = 0.05f; mo_tick(&XR.a, fdt); mo_tick(&XR.card_a, fdt); }
  float a = XR.a.v * (1 - XR.card_a.v); uint32_t ac = ACC2 | 0xff000000u; int ep = episodic(); /* (the card covers the panel: the panel fades as it comes) */
  if (a < 0.003f) goto card;
  float PW = 700, ox = -PW * (1 - a) * 0.25f; gfx_opacity_push(a);
  gfx_rrect_h(0, 0, PW + 260, SB_H, 0, RGBA(8, 8, 12, 236), RGBA(8, 8, 12, 0)); gfx_rect(0, 0, PW, SB_H, RGBA(8, 8, 12, 120));
  gfx_offset_push(ox, 0);
  float x = 72, y = 70;
  text_draw("X-Ray", x, y, 46, 800, WHITE_A(255), -0.6f, 0, ALIGN_LEFT);
  { char sub[300]; if (ep && P.season >= 0) snprintf(sub, sizeof sub, "%s  \xc2\xb7  T%d:E%d", P.name, P.season, P.episode); else snprintf(sub, sizeof sub, "%s", P.name); text_draw(sub, x, y + 62, 24, 600, WHITE_A(160), 0, PW - 120, ALIGN_LEFT); }
  /* the tabs */
  float ty = y + 112, th = 58, tx = x; const char *TB[2] = { T("Cast"), T("Trivia") };
  for (int t = 0; t < 2; t++) { float w = text_width(TB[t], 26, 700, 0) + 56; int sel = XR.tab == t, f = sel && XR.zone == 0;
    gfx_rrect(tx, ty, w, th, th / 2, f ? WHITE_A(250) : sel ? WHITE_A(46) : WHITE_A(14)); text_draw(TB[t], tx + 28, ty + (th - text_line_height(26)) / 2, 26, 700, f ? RGB(16, 16, 20) : WHITE_A(sel ? 255 : 170), 0, 0, ALIGN_LEFT); tx += w + 12; }
  float ly = ty + th + 34, lh = SB_H - 90 - ly;
  if (XR.tab == 0) { const InsightFace *fc; int nf = insight_get(P.type, P.id, ep ? P.season : -1, ep ? P.episode : -1, &fc);
    if (!nf) text_draw(insight_state() == 1 || !insight_state() ? T("Getting the cast\xe2\x80\xa6") : T("No cast for this title"), x, ly + 10, 28, 600, WHITE_A(160), 0, PW - 140, ALIGN_LEFT);
    float RH = 124, want = XR.idx > 3 ? -(XR.idx - 3) * RH : 0; XR.scroll += (want - XR.scroll) * fminf(1, app_dt() * 12);
    gfx_clip_push(x - 20, ly - 6, PW - x + 10, lh + 12);
    for (int i = 0; i < nf; i++) { float ry = ly + i * RH + XR.scroll; if (ry + RH < ly || ry > ly + lh) continue; int fo = XR.zone == 1 && i == XR.idx;
      if (fo) { gfx_rrect(x - 16, ry, PW - x - 40, RH - 12, 22, WHITE_A(30)); gfx_ring(x - 16, ry, PW - x - 40, RH - 12, 22, 3, ac); }
      float asp; GLuint tx2 = img_get(fc[i].photo, 185, 278, &asp); float d = 92, py = ry + (RH - 12 - d) / 2;
      if (tx2) { float a2 = asp > 0 && asp < 1 ? asp : 0.667f, v0 = (1 - a2) * 0.2f; gfx_image(tx2, x, py, d, d, d / 2, WHITE_A(255), 0, v0, 1, v0 + a2); } else gfx_circle(x + d / 2, py + d / 2, d / 2, WHITE_A(36));
      text_draw(fc[i].name, x + d + 24, py + 12, 30, 800, WHITE_A(fo ? 255 : 230), -0.2f, PW - x - d - 90, ALIGN_LEFT);
      if (fc[i].role[0]) text_draw(T1("as {c}", "c", fc[i].role), x + d + 24, py + 54, 24, 500, WHITE_A(170), 0, PW - x - d - 90, ALIGN_LEFT); }
    gfx_clip_pop(); }
  else { const char **tl = NULL; int st = 0, nt = insight_trivia(P.id, &tl, &st);
    if (!nt) text_draw(st == 2 ? T("No trivia for this title") : T("Getting the trivia\xe2\x80\xa6"), x, ly + 10, 28, 600, WHITE_A(160), 0, PW - 140, ALIGN_LEFT);
    float cw = PW - x - 40, yy = ly; static float hts[24]; float off = 0; for (int i = 0; i < XR.idx && i < 24; i++) off += hts[i]; float want = off > lh * 0.35f ? -(off - lh * 0.35f) : 0; XR.scroll += (want - XR.scroll) * fminf(1, app_dt() * 12);
    gfx_clip_push(x - 20, ly - 6, PW - x + 10, lh + 12); yy = ly + XR.scroll;
    for (int i = 0; i < nt && i < 24; i++) { int fo = XR.zone == 1 && i == XR.idx; int nl = text_wrap_count(tl[i], cw - 48, 7, 25, 500, 0); float h = 28 + nl * 36 + 24; hts[i] = h + 14;
      if (yy + h > ly - 10 && yy < ly + lh + 10) { gfx_rrect(x - 16, yy, cw, h, 22, fo ? WHITE_A(30) : WHITE_A(12)); if (fo) gfx_ring(x - 16, yy, cw, h, 22, 3, ac);
        text_draw_wrapped(tl[i], x + 8, yy + 26, cw - 48, 7, 25, 500, WHITE_A(fo ? 245 : 200), 0, ALIGN_LEFT, 36); }
      yy += h + 14; }
    gfx_clip_pop(); }
  { float hx = x, hy = SB_H - 52; if (XR.tab == 0) hx += key_hint(hx, hy, K_OK, T("Details"), WHITE_A(170), 0) + 28; key_hint(hx, hy, K_BACK, T("Close"), WHITE_A(170), 0); }
  gfx_offset_pop(); gfx_opacity_pop();
  card:;
  /* the person, as Apple TV's card: the film paused or not behind, the panel under it */
  float ca = XR.card_a.v * XR.a.v; if (ca < 0.003f) return; const InsightPerson *pp = insight_person(XR.cardPid);
  gfx_opacity_push(ca); gfx_rrect_h(0, 0, SB_W, SB_H, 0, RGBA(6, 6, 10, 250), RGBA(6, 6, 10, 120)); gfx_rect(0, 0, SB_W * 0.6f, SB_H, RGBA(6, 6, 10, 160));
  float cx = 120, cy = 110; { float hw = key_hint(cx, cy + 10, K_BACK, T("Back"), WHITE_A(220), 0); (void)hw; }
  float phW = 300, phH = 450, py = cy + 70; float asp; GLuint t = img_get(XR.cardPhoto, 300, 450, &asp);
  gfx_shadow(cx, py + 14, phW, phH, 28, 40, BLACK_A(160)); if (t) gfx_image(t, cx, py, phW, phH, 28, WHITE_A(255), 0, 0, 1, 1); else gfx_rrect(cx, py, phW, phH, 28, WHITE_A(30));
  float tx3 = cx + phW + 56, tw = SB_W * 0.62f - tx3; if (tw < 520) tw = 520;
  text_draw(XR.cardName, tx3, py + 6, 58, 800, WHITE_A(255), -0.8f, tw, ALIGN_LEFT);
  if (XR.cardRole[0]) text_draw(T1("as {c}", "c", XR.cardRole), tx3, py + 84, 32, 600, WHITE_A(200), 0, tw, ALIGN_LEFT);
  float fy = py + 146;
  if (pp && pp->born[0]) { char d[40], ln[200]; xr_date(pp->born, d, sizeof d); int ag = xr_age(pp->born, pp->died[0] ? pp->died : NULL); char an[16]; snprintf(an, sizeof an, "%d", ag);
    if (ag >= 0 && !pp->died[0]) snprintf(ln, sizeof ln, "%s  \xc2\xb7  %s", d, T1("{n} years", "n", an)); else snprintf(ln, sizeof ln, "%s", d);
    icon_draw("p_cake", tx3, fy + 2, 28, WHITE_A(160)); text_draw(ln, tx3 + 42, fy, 26, 600, WHITE_A(220), 0, tw - 42, ALIGN_LEFT); fy += 44; }
  if (pp && pp->place[0]) { icon_draw("d_country", tx3, fy + 2, 28, WHITE_A(160)); text_draw(pp->place, tx3 + 42, fy, 26, 600, WHITE_A(220), 0, tw - 42, ALIGN_LEFT); fy += 44; }
  if (pp && pp->bio[0]) { fy += 12; text_draw_wrapped(pp->bio, tx3, fy, tw, 6, 25, 400, WHITE_A(205), 0, ALIGN_LEFT, 36); }
  else if (!pp || pp->state == 1) text_draw("\xe2\x80\xa6", tx3, fy + 12, 28, 600, WHITE_A(150), 0, 0, ALIGN_LEFT);
  if (pp && pp->nknown) { float ky = py + phH + 50; text_draw(T("Known for"), cx, ky, 30, 800, WHITE_A(240), -0.2f, 0, ALIGN_LEFT); ky += 52;
    for (int i = 0; i < pp->nknown && i < 8; i++) { float kx = cx + i * 204; float a3; GLuint pt = img_get(pp->known[i].poster, 185, 278, &a3);
      if (pt) gfx_image(pt, kx, ky, 176, 264, 16, WHITE_A(255), 0, 0, 1, 1); else gfx_rrect(kx, ky, 176, 264, 16, WHITE_A(24));
      char lb[140]; if (pp->known[i].year[0]) snprintf(lb, sizeof lb, "%s", pp->known[i].year); else lb[0] = 0;
      text_draw(pp->known[i].title, kx, ky + 276, 21, 700, WHITE_A(225), 0, 176, ALIGN_LEFT); if (lb[0]) text_draw(lb, kx, ky + 304, 19, 500, WHITE_A(150), 0, 176, ALIGN_LEFT); } }
  gfx_opacity_pop();
}
/* ---- etapa-72: Lumen's intro before a film, "Still watching?" after a run of episodes nobody touched the remote
   through, and "Where you left off" (the episode before, in a few lines) as a series starts ---- */
static struct { int run, ask; double askAt; double introAt; int recapWant, recapOn; double recapAt; char recapT[200], recap[900]; int recapLines; Tween introA, recapA, askA; } LX;
/* etapa-79: smart credits: TMDB's keywords say whether a film has a scene during or after its credits; near the end a card says to stay */
static struct { unsigned tok; int sting, shown; double at; Tween a; char id[SB_ID]; } ST;
static void on_kw(int st, const unsigned char *b, size_t len, void *ud) { (void)len; if ((unsigned)(uintptr_t)ud != ST.tok) return; cJSON *j = st == 200 ? net_json(b) : NULL; const cJSON *k;
  cJSON_ArrayForEach(k, js_arr(j, "keywords")) { int id = (int)js_num(k, "id", 0); const char *nm = js_str(k, "name", ""); if (id == 179430 || strstr(nm, "duringcreditsstinger")) ST.sting |= 1; if (id == 179431 || strstr(nm, "aftercreditsstinger")) ST.sting |= 2; }
  if (ST.sting) fprintf(stderr, "[credits] %s has a scene %s the credits\n", ST.id, ST.sting & 1 ? "during" : "after"); net_json_free(j); }
static void on_find(int st, const unsigned char *b, size_t len, void *ud) { (void)len; if ((unsigned)(uintptr_t)ud != ST.tok) return; cJSON *j = st == 200 ? net_json(b) : NULL;
  int tm = (int)js_num(cJSON_GetArrayItem(js_arr(j, "movie_results"), 0), "id", 0); net_json_free(j); if (!tm) return; char p[64]; snprintf(p, sizeof p, "/movie/%d/keywords", tm); tmdb_request(p, on_kw, (void *)(uintptr_t)ST.tok); }
static void sting_ask(void) { ST.tok++; ST.sting = 0; ST.shown = 0; tween_set(&ST.a, 0); snprintf(ST.id, sizeof ST.id, "%s", P.id);
  if (P.live || strcmp(P.type, "movie") || strncmp(P.id, "tt", 2) || !tmdb_has_key() || !strcmp(store_get("creditsHint", "true"), "false")) return;
  char p[80]; snprintf(p, sizeof p, "/find/%s?external_source=imdb_id", P.id); tmdb_request(p, on_find, (void *)(uintptr_t)ST.tok); }
static int lx_limit(void) { return atoi(store_get("stillWatching", "2")); } /* episodes played by themselves before it asks (0: never) */
/* the moment a next episode would start by itself: 1 = it asks instead (the film paused under the card) */
static int lx_gate(void) {
  int lim = lx_limit(); if (lim <= 0 || P.live) return 0;
  if (LX.run < lim) { LX.run++; return 0; }
  LX.ask = 1; LX.askAt = now_(); tween_set(&LX.askA, 0); tween_to(&LX.askA, 1, 0.4f, EASE_OUT); P.prompt.autoN = 0; paint_prompt();
  if (!paused() && !P.ended) video_pause(); fprintf(stderr, "[player] still watching? (%d episodes by themselves)\n", LX.run); return 1; }
static void lx_begin(int again) {
  if (!again) { LX.run = 0; LX.ask = 0; LX.introAt = strcmp(store_get("playerIntro", "true"), "false") && !P.live ? now_() : 0; }
  sting_ask();
  LX.recapOn = 0; LX.recapWant = !again && !P.live && !strcmp(P.type, "series") && (P.episode > 1 || P.season > 1) && strcmp(store_get("recapCard", "true"), "false");
}
/* the episode before this one: its name and what happened (TMDB in the app's language, else the meta's) */
static int lx_recap_fill(void) {
  int s = P.season, e = P.episode - 1; const cJSON *vs = js_arr(P.meta, "videos"), *v, *best = NULL;
  if (e < 1) { int le = 0; cJSON_ArrayForEach(v, vs) { if ((int)js_num(v, "season", -1) == s - 1) { int ee = (int)js_num(v, "episode", js_num(v, "number", 0)); if (ee > le) { le = ee; best = v; } } } s--; e = le; }
  else cJSON_ArrayForEach(v, vs) if ((int)js_num(v, "season", -1) == s && (int)js_num(v, "episode", js_num(v, "number", 0)) == e) best = v;
  if (s < 1 || e < 1) return 0;
  const char *nm = best ? js_str(best, "name", js_str(best, "title", "")) : "", *ov = best ? js_str(best, "overview", js_str(best, "description", "")) : "";
  const TmdbLoc *L = tmdb_wanted(P.id) ? tmdb_loc("series", P.id, 0) : NULL; const char *tn = NULL, *to = NULL;
  if (L && tmdb_episode(L, s, e, &tn, &to)) { if (tn && *tn) nm = tn; if (to && *to) ov = to; }
  if (!*ov) return 0;
  char se[40]; snprintf(se, sizeof se, "T%d \xc2\xb7 E%d", s, e);
  snprintf(LX.recapT, sizeof LX.recapT, "%s%s%s", se, *nm ? "  \xe2\x80\x94  " : "", nm); snprintf(LX.recap, sizeof LX.recap, "%s", ov);
  LX.recapLines = text_wrap_count(LX.recap, 760, 4, 26, 400, 0); return 1; }
static void lx_tick(float dt) {
  tween_tick(&ST.a, dt);
  if (ST.sting && !ST.shown) { double d = video_duration(), t = video_time(), left = d - t, at = d * 0.06 > 360 ? d * 0.06 : 360; if (d > 1200 && t > 60 && left > 0 && left <= at && video_state() == VID_PLAYING) { ST.shown = 1; ST.at = now_(); tween_to(&ST.a, 1, 0.5f, EASE_OUT); fprintf(stderr, "[credits] the card: stay for the scene\n"); } }
  if (ST.shown == 1 && now_() - ST.at > 12) { ST.shown = 2; tween_to(&ST.a, 0, 0.6f, EASE_IN_OUT); }
  { static float acc; if (video_state() == VID_PLAYING && !P.live) acc += dt; /* etapa-74: time watched per month, for the Retrospective */
    if (acc >= 60) { time_t t = time(NULL); struct tm tm; localtime_r(&t, &tm); char mk[12]; snprintf(mk, sizeof mk, "%d-%02d", tm.tm_year + 1900, tm.tm_mon + 1);
      cJSON *o = cJSON_Duplicate(store_node("watchLog"), 1); if (!cJSON_IsObject(o)) { cJSON_Delete(o); o = cJSON_CreateObject(); }
      double v = js_num(o, mk, 0) + acc; cJSON_DeleteItemFromObject(o, mk); cJSON_AddNumberToObject(o, mk, v); store_set_node("watchLog", o); acc = 0; } }
  tween_tick(&LX.introA, dt); tween_tick(&LX.recapA, dt); tween_tick(&LX.askA, dt);
  if (LX.recapWant && video_state() == VID_PLAYING && P.artA.v < 0.05f && (!LX.introAt || now_() - LX.introAt > 2.6)) { LX.recapWant = 0;
    if (lx_recap_fill()) { LX.recapOn = 1; LX.recapAt = now_(); tween_set(&LX.recapA, 0); tween_to(&LX.recapA, 1, 0.5f, EASE_OUT); fprintf(stderr, "[player] where you left off: %s\n", LX.recapT); } }
  if (LX.recapOn && now_() - LX.recapAt > 14) { LX.recapOn = 0; tween_to(&LX.recapA, 0, 0.5f, EASE_IN_OUT); }
  if (LX.ask && now_() - LX.askAt > 20 * 60) { LX.ask = 0; fprintf(stderr, "[player] still watching? no answer in 20 min: closed\n"); close_player(0, 0); } /* (asleep: the TV rests) */
}
static void lx_draw(void) {
  if (ST.a.v > 0.01f) { float a = ST.a.v, w = 640, h = 130, x = SB_W - w - 64, y = 70 - 20 * (1 - a); gfx_opacity_push(a);
    gfx_shadow(x, y + 16, w, h, 30, 50, BLACK_A(130)); gfx_rrect(x, y, w, h, 30, RGBA(14, 12, 22, 225)); gfx_ring(x, y, w, h, 30, 2, col_alpha(ACC2 | 0xff000000u, 0.7f));
    icon_draw("wn_new", x + 30, y + 35, 60, ACC2 | 0xff000000u);
    text_draw(T(ST.sting & 1 ? "There's a scene during the credits" : "There's a scene after the credits"), x + 110, y + 28, 28, 800, WHITE_A(250), 0, w - 140, ALIGN_LEFT);
    text_draw(T("Stay until the end!"), x + 110, y + 72, 24, 500, WHITE_A(200), 0, w - 140, ALIGN_LEFT); gfx_opacity_pop(); }
  if (LX.introAt) { double t = now_() - LX.introAt; if (t > 2.6) LX.introAt = 0;
    else { float a = t < 2.0 ? 1 : (float)(1 - (t - 2.0) / 0.6); gfx_opacity_push(a); gfx_rect(0, 0, SB_W, SB_H, RGB(0x0b, 0x09, 0x12)); lumen_mark_draw(SB_W / 2, SB_H / 2 - 40, 110, (float)t * 0.9f, 1); gfx_opacity_pop(); } }
  float ra = LX.recapA.v; if (ra > 0.01f && LX.recap[0]) { float w = 840, h = 46 + 44 + LX.recapLines * 36 + 40, x = 72, y = SB_H - 150 - h + 14 * (1 - ra);
    gfx_opacity_push(ra); gfx_shadow(x, y + 20, w, h, 28, 60, BLACK_A(120)); gfx_rrect(x, y, w, h, 28, RGBA(14, 12, 22, 215)); gfx_ring(x, y, w, h, 28, 1, WHITE_A(26));
    text_draw(T("Where you left off"), x + 40, y + 30, 22, 700, col_alpha(ACC2 | 0xff000000u, 1), 1.5f, 0, ALIGN_LEFT);
    text_draw(LX.recapT, x + 40, y + 64, 30, 700, WHITE_A(250), -0.2f, w - 80, ALIGN_LEFT);
    text_draw_wrapped(LX.recap, x + 40, y + 108, 760, 4, 26, 400, WHITE_A(210), 0, ALIGN_LEFT, 36); gfx_opacity_pop(); }
  float aa = LX.askA.v; if (LX.ask && aa > 0.01f) { gfx_opacity_push(aa); gfx_rect(0, 0, SB_W, SB_H, BLACK_A(170));
    float cx = SB_W / 2, cy = SB_H / 2; text_draw(T("Still watching?"), cx, cy - 90, 64, 800, WHITE_A(255), -1, 0, ALIGN_CENTER);
    text_draw(T("The next episode waits for you"), cx, cy + 0, 30, 400, WHITE_A(200), 0, 0, ALIGN_CENTER);
    { const char *a1 = T("Next episode"), *a2 = T("Stop"); float w1 = 38 + 10 + text_width(a1, 22, 600, 0), w2 = 38 + 10 + text_width(a2, 22, 600, 0), gap = 60, x0 = cx - (w1 + gap + w2) / 2;
      key_hint(x0, cy + 90, K_OK, a1, WHITE_A(240), 0); key_hint(x0 + w1 + gap, cy + 90, K_BACK, a2, WHITE_A(200), 0); }
    gfx_opacity_pop(); }
}
void player_key(int k, int down) {
  if (!down) { if (P.open && P.mode == 6 && k == K_OK) hub_ok_up(); return; } /* (the hub over the picture: OK let go, a tap; held, its streams) */
  if (!P.open) return; if (shrinkAt > 0) return; /* (going back into its box) */ app_touch();
  if (LX.ask) { if (k == K_OK || k == K_PLAY || k == K_PLAYPAUSE) { LX.ask = 0; LX.run = 0; tween_to(&LX.askA, 0, 0.3f, EASE_OUT); go_next(); } else if (k == K_BACK || k == K_STOP) { LX.ask = 0; close_player(0, 0); } return; } /* etapa-72: "Still watching?" */
  LX.run = 0; if (LX.recapOn && (k == K_BACK || k == K_OK)) { LX.recapOn = 0; tween_to(&LX.recapA, 0, 0.3f, EASE_IN_OUT); if (k == K_BACK) return; }
  if (P.pw.on && k != K_BACK) { if (k == K_OK) pw_end("Play now"); return; } /* (waiting for the previews: OK plays at once, Back closes, nothing else) */
  P.lastAct = now_(); if (P.ssOn) { ss_exit(); return; } /* the key that wakes the screensaver does nothing else */
  if (P.failed && P.canNext && k == K_OK) { try_next(); return; }
  if (!P.started || !P.live || P.failed) { if (k == K_BACK || k == K_EXIT) close_player(0, 0); return; } /* the art is up (resolving): only Back does anything */
  P.hvOn = 0;
  if ((P.mode == 4 || P.mode == 5) && (k == K_PLAY || k == K_PAUSE || k == K_PLAYPAUSE)) { if (k == K_PLAYPAUSE || (k == K_PLAY) == paused()) toggle(); return; } /* the card stays: the controls don't come */
  if (k == K_PLAY) { if (paused()) play_on(); show_osd(1); return; }
  if (k == K_PAUSE) { if (!paused()) { video_pause(); save_progress(1); } show_osd(1); return; }
  if (k == K_PLAYPAUSE) { toggle(); show_osd(1); return; }
  if (k == K_STOP || k == K_EXIT) { close_player(0, 0); return; }
  if (k == K_REW) { seek_by(-1); return; } if (k == K_FF) { seek_by(1); return; }
  if (XR.on) { xr_key(k); return; } /* the X-Ray panel has the keys */
  if (k == K_BACK) { back(); return; }
  if (P.mode == 2) { tray_key(k); return; }
  if (P.mode == 4) { dly_key(k); return; }
  if (P.mode == 5) { pos_key(k); return; }
  if (P.mode == 3) {
    if (P.prompt.k == 3 && P.next.has && (k == K_LEFT || k == K_RIGHT)) { nudge_start(&P.nudge, k); return; } /* the card: OK or BACK */
    if ((k == K_LEFT && P.prompt.idx == 0) || (k == K_RIGHT && P.prompt.idx >= P.prompt.nbtn - 1)) nudge_start(&P.nudge, k);
    else if (k == K_LEFT) P.prompt.idx--; else if (k == K_RIGHT) P.prompt.idx++;
    else if (k == K_OK) prompt_go(P.prompt.idx);
    else if (k == K_UP || k == K_DOWN) { P.mode = 0; show_osd(0); }
    return; }
  if (multi_on() && P.mode == 0 && (k == K_LEFT || k == K_RIGHT)) { multi_audio(k == K_RIGHT ? 1 : 0); return; } /* two screens: the sound goes to the left or the right one */
  if (k == K_YELLOW && live_title() && live_catalogs() > 0 && P.mode != 6 && P.mode != 2) { hub_from_player(); return; } /* the yellow key: the channels over the picture */
  if (P.mode == 6) { if (k == K_CH_UP || k == K_CH_DOWN) { live_zap(k == K_CH_UP ? 1 : -1); return; } hub_key(k); if (!hub_on()) P.mode = 0; return; } /* the hub has the keys */
  if (P.mode == 0 && live_title() && P.seekT < 0) { /* a channel: the channel keys switch (the yellow key or the Channels button: the hub), Left / Right bring the controls (nothing to seek); Down as ever */
    if ((k == K_CH_UP || k == K_CH_DOWN) && live_lineup_n() > 1) { live_zap(k == K_CH_UP ? 1 : -1); return; }
    if (k == K_OK && live_catalogs() > 0) { hub_from_player(); return; } /* OK: the channels */
    if (k == K_LEFT || k == K_RIGHT) { show_osd(0); return; } }
  if (P.mode == 0) {
    if (k == K_OK) { if (P.seekT >= 0) commit_ok(); else if (P.prompt.k) prompt_go(P.prompt.idx); else { toggle(); show_osd(0); } return; }
    if (k == K_LEFT) { seek_by(-1); return; } if (k == K_RIGHT) { seek_by(1); return; }
    if ((k == K_UP || k == K_DOWN) && P.seekT >= 0 && chapters_on()) { chap_jump(k == K_UP ? 1 : -1); return; } /* seeking: from chapter to chapter */
    if (k == K_UP && P.seekT < 0 && !P.prompt.k && !P.isTrailer && P.type[0] && strcmp(P.type, "other") && !strncmp(P.id, "tt", 2)) { xr_open(); return; } /* Up: the X-Ray panel (Down: the controls) */
    if (k == K_UP || k == K_DOWN) { if (P.seekT >= 0) commit_seek(); show_osd(0); return; }
    return; }
  arm_hide();
  if (P.row == 1) {
    if ((k == K_UP || k == K_DOWN) && P.seekT >= 0 && chapters_on()) { chap_jump(k == K_UP ? 1 : -1); return; } /* seeking: from chapter to chapter */
    if (k == K_LEFT) seek_by(-1); else if (k == K_RIGHT) seek_by(1); else if (k == K_OK) { if (P.seekT >= 0) commit_ok(); else toggle(); }
    else if (k == K_DOWN) { if (P.seekT >= 0) commit_seek(); P.row = 0; }
    else if (k == K_UP) { if (P.prompt.k) { P.mode = 3; } else if (P.seekT < 0 && !P.isTrailer && !strncmp(P.id, "tt", 2)) xr_open(); else nudge_start(&P.nudge, k); }
    return; }
  if ((k == K_LEFT && P.bIdx == 0) || (k == K_RIGHT && P.bIdx == P.nbtn - 1)) nudge_start(&P.nudge, k); /* the row's end */
  else if (k == K_LEFT) P.bIdx--; else if (k == K_RIGHT) P.bIdx++;
  else if (k == K_UP) { if (live_title()) nudge_start(&P.nudge, k); else P.row = 1; } else if (k == K_DOWN) hide_osd(); /* (a channel: no bar to seek on) */ else if (k == K_OK) act(P.btns[P.bIdx].id);
}

/* ---- open / start / close ---- */
static void fill_from_meta(void) {
  const cJSON *m = P.meta; if (!m) return;
  snprintf(P.name, sizeof P.name, "%s", js_str(m, "name", P.name)); const char *lg = js_str(m, "logo", NULL); if (lg && *lg) snprintf(P.logo, sizeof P.logo, "%s", lg);
  if (!P.poster[0]) snprintf(P.poster, sizeof P.poster, "%s", js_str(m, "poster", "")); snprintf(P.year, sizeof P.year, "%.4s", js_str(m, "releaseInfo", js_str(m, "year", "")));
  const cJSON *gs = js_arr(m, "genres"); if (gs && cJSON_GetArraySize(gs) && cJSON_IsString(cJSON_GetArrayItem(gs, 0))) snprintf(P.genre, sizeof P.genre, "%s", cJSON_GetArrayItem(gs, 0)->valuestring);
  { const cJSON *vv = js_arr(m, "videos"); if (!strcmp(P.type, "series") || (vv && cJSON_GetArraySize(vv) > 1 && strcmp(P.type, "movie") && strcmp(P.type, "other"))) P.videos = (cJSON *)vv; }
  if (P.videos && strcmp(P.vid, P.id)) { P.season = P.episode = -1; /* from the video's own numbers; a series' id ("tt..:s:e") as the fallback (an anime id like "kitsu:123:5" is no season) */
    { const char *c = strrchr(P.vid, ':'); if (c && !strcmp(P.type, "series")) { const char *p = c - 1; while (p > P.vid && *p != ':') p--; if (*p == ':') { P.season = atoi(p + 1); P.episode = atoi(c + 1); } } }
    { const cJSON *v0; cJSON_ArrayForEach(v0, P.videos) if (!strcmp(js_str(v0, "id", ""), P.vid)) { if (js_num(v0, "season", -1) >= 0) P.season = (int)js_num(v0, "season", 0); if (js_num(v0, "episode", -1) >= 0) P.episode = (int)js_num(v0, "episode", 0); break; } }
    const cJSON *cur = NULL, *nxt = NULL, *first = NULL, *v; cJSON_ArrayForEach(v, P.videos) { if (!strcmp(js_str(v, "id", ""), P.vid)) cur = v; int s = (int)js_num(v, "season", -1), e = (int)js_num(v, "episode", -1); if (s == P.season && e == P.episode + 1) nxt = v; if (P.season >= 0 && s == P.season + 1 && (!first || e < (int)js_num(first, "episode", 0))) first = v; }
    if (!nxt && first) nxt = first; /* last episode of a season -> first of the next one */
    if (cur && !nxt && js_num(cur, "season", -1) < 0) { /* no seasons (anime, channels): the next in the list, by episode number when given, else in order */
      int ce = (int)js_num(cur, "episode", -1), seen = 0; const cJSON *after = NULL;
      cJSON_ArrayForEach(v, P.videos) { if (ce >= 0 && (int)js_num(v, "episode", -1) == ce + 1) { nxt = v; break; } if (seen && !after) after = v; if (v == cur) seen = 1; }
      if (!nxt) nxt = after; }
    if (nxt) { const char *rel = js_str(nxt, "released", ""); struct tm tm; memset(&tm, 0, sizeof tm); if (strlen(rel) >= 10 && sscanf(rel, "%d-%d-%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday) == 3) { tm.tm_year -= 1900; tm.tm_mon--; if (timegm(&tm) > time(NULL)) nxt = NULL; } } /* not out yet */
    if (cur) snprintf(P.epTitle, sizeof P.epTitle, "%s", js_str(cur, "name", js_str(cur, "title", "")));
    if (nxt) { P.next.has = 1; snprintf(P.next.vid, 80, "%s", js_str(nxt, "id", "")); if (js_num(nxt, "season", -1) >= 0) snprintf(P.next.se, 32, "S%d · E%d", (int)js_num(nxt, "season", 0), (int)js_num(nxt, "episode", 0)); else if (js_num(nxt, "episode", -1) >= 0) snprintf(P.next.se, 32, "E%d", (int)js_num(nxt, "episode", 0)); else P.next.se[0] = 0; snprintf(P.next.title, 256, "%s", js_str(nxt, "name", js_str(nxt, "title", ""))); snprintf(P.next.thumb, 512, "%s", js_str(nxt, "thumbnail", "")); snprintf(P.next.ov, 600, "%s", js_str(nxt, "overview", js_str(nxt, "description", ""))); }
  } else if (P.type[0] && strcmp(P.type, "other")) P.sim = 1;
}
/* the title's metadata: from memory at once, else from the disk cache (that answer can come back before the player
   is open) or the network; a series still missing its episodes or next episode asks again 20 s later */
static unsigned metaTok;
static void on_meta(cJSON *m, void *ud) {
  if ((unsigned)(uintptr_t)ud != metaTok || !m) return;
  if (P.meta && P.videos) return; /* nothing missing (and the open tray points into this copy) */
  if (P.meta) cJSON_Delete(P.meta); P.meta = cJSON_Duplicate(m, 1); fill_from_meta(); if (P.started) build_btns();
}
static void ask_meta(void) { metaTok++; if (!metaTok) metaTok++; sb_meta(P.type, P.id, on_meta, (void *)(uintptr_t)metaTok); }
static void on_size(int st, const unsigned char *body, size_t len, void *ud) {
  (void)len; if (!P.open || (unsigned)(uintptr_t)ud != P.sizeTok) return; P.sizeAns = 1; P.httpSt = st; if (st >= 400) fprintf(stderr, "[player] the stream's server answers %d\n", st); if (st <= 0) return; double b = atof((const char *)body); if (b <= 0) return; P.size = b;
  char n[16]; if (b >= 1e9) { snprintf(n, sizeof n, "%.*f", b >= 1e10 ? 0 : 1, b / 1e9); snprintf(P.sizeTxt, sizeof P.sizeTxt, "%s", T1("{n} GB", "n", n)); } else { snprintf(n, sizeof n, "%d", (int)(b / 1e6 + 0.5)); snprintf(P.sizeTxt, sizeof P.sizeTxt, "%s", T1("{n} MB", "n", n)); }
}
/* A stream whose text names only TrueHD, or only DTS on a TV seen failing it: the TV is not given it until the file's
   tracks are read (the subtitle reader's head read, a moment): a track it plays is chosen once it lists them, else the
   conversion starts without a silent first try. Every other stream loads at once, as always */
static int dts_defer_wanted(void) {
  if (P.isTrailer || P.isHls || strncmp(P.url, "http", 4) || !hls_conv_available()) return 0;
  BestRate r; if (!best_rate(&P.st, NULL, -1, 100, &r) || !r.mute) return 0;
  char au[48]; best_audio_of(P.sname, au, sizeof au); return strstr(au, "TrueHD") || best_learned("dts") == 0;
}
static void dts_defer_tick(void) {
  if (P.dts.conv) { P.dts.deferUntil = 0; return; } /* (started meanwhile, from the file's head) */
  int listed = mkv_listed(mkv); if (!listed && mkv_state(mkv) != MKV_FALLBACK && now_() < P.dts.deferUntil) return;
  P.dts.deferUntil = 0; MkvAudio ma[16]; int m = listed ? mkv_audio_tracks(mkv, ma, 16) : 0, playable = 0, first = -1;
  for (int i = 0; i < m; i++) { VidTrack v; memset(&v, 0, sizeof v); snprintf(v.codec, sizeof v.codec, "%s", ma[i].codec); if (!aud_bad(&v)) playable = 1; else if (first < 0) first = i; }
  fprintf(stderr, "[dts] the file's sound: %d track(s), %s\n", m, !m ? "not read: the TV tries it" : playable ? "one plays here: the TV loads it" : "none plays here");
  if (m && !playable && first >= 0) { const char *want = aud_want(); int ord = first; /* the track in the language wanted (this show's own, else the settings'), else the first */
    if (*want) for (int i = 0; i < m; i++) if (lang_match(ma[i].lang, want)) { ord = i; break; }
    if (dts_convert(ord, "known before loading")) return; }
  if (listed && dv7_convert("known before loading")) return;
  video_load(P.url);
}
static void start_video(void) {
  P.vidAt = now_(); P.started = 1; P.isTrailer = !strcmp(P.type, "other"); P.isHls = strstr(P.url, ".m3u8") != NULL; P.liveStream = P.hlsVod = 0; liveTok++; for (int i = 0; i < LIVE_RELOADS; i++) liveRe[i] = -1e9;
  best_caps_probe(NULL, NULL); /* (once: kept) */
  video_audio_delay(0); video_speed(1); aud_fx_apply(); /* a new stream: in sync, normal speed; night mode and the boost as set */
  video_start_silent(pw_wanted()); /* (to be held for its previews: it loads silent) */
  if (P.adopt) { fprintf(stderr, "[player] taking the preview's playback as it plays%s\n", adoptFull ? " (full screen already)" : ""); P.liveT0 = now_(); P.steadyAt = now_() - 1; growPending = !adoptFull; growAt = 0; planeAt = 0; memcpy(planeSent, growR, sizeof planeSent); } /* (it grows once the page under it is pictured) */ /* (already loaded and running) */
  else if (dts_defer_wanted() || dv7_defer_wanted()) P.dts.deferUntil = now_() + 4; else video_load(P.url);
  P.ps.on = ps_wanted(); P.ps.v = 0; P.ps.t0 = now_(); sp_verify_reset();
  if (P.ps.on) { double fs = P.st.size > 0 ? P.st.size : P.size; ps_want(P.type, P.id, P.vid, best_dur(P.meta, P.type), P.url, fs); ps_hold(1); fprintf(stderr, "[player] its seek previews from a copy 2.5x lighter if one proves the same cut (the film %.1f GB%s)\n", fs / 1e9, film_heavy() ? ", heavy" : ""); } else ps_cancel(); /* (a light film: its own keyframes; a copy made ahead stops, never beside it) */
  P.pw.on = pw_wanted(); P.pw.held = 0; P.pw.t0 = now_(); P.pw.target = pw_target(); P.pw.eta = 0; P.pw.shown = 0; P.pw.etaShown = 0; P.pw.lastF = -1; P.pw.made = 0; P.pw.slips = 0; P.pw.holdT = 0; P.pw.gainAt = now_(); if (P.pw.on) fprintf(stderr, "[player] previews first: waiting for %d %% of them, the film loading meanwhile (OK plays at once)\n", P.pw.target);
  live_probe(); /* (after the load: video_set_live holds for this source) */
  build_btns(); if (!P.adopt) apply_aspect(); P.res = res_of(P.sname);
  if ((episodic() || (strcmp(P.type, "movie") && strcmp(P.type, "other") && strcmp(P.vid, P.id))) && !P.isTrailer && (!P.next.has || !P.videos)) { ask_meta(); P.metaRetryAt = now_() + 20; }
  if (!P.isHls && !P.isTrailer && !strncmp(P.url, "http", 4)) { static unsigned szTok; P.sizeTok = ++szTok; net_size(P.url, on_size, (void *)(uintptr_t)P.sizeTok); } /* asked once per stream */
  asssubs_close(); if (mkv) { prevspike_stop(); sp_close(); mkv_close(mkv); mkv = NULL; }
  if (!P.isHls && !P.isTrailer && !strncmp(P.url, "http", 4)) { mkvWantCues = prev_enabled() && !P.isP2p && sp_may_make(); /* (seek previews want the index even with no text track; never for a torrent on a TV) */
    mkv = mkv_open(P.url, P.startAt > 0 ? P.startAt : 0, P.isP2p); } /* not Matroska: it gives up after one small read */
  load_ext_subs(0); load_addon_subs(0); load_emb_tracks();
}
static char nextAlt[SB_URL_MAX];
void player_fallback(const char *url) { snprintf(nextAlt, sizeof nextAlt, "%s", url ? url : ""); }
/* a new source / episode: the state starts over behind the title art (the art crossfades in when already playing) */
/* ---- the seek pill's time as a rolling clock: each character keeps a slot (digits as wide as the widest), only the
   ones that change roll: the old one slides down and fades, the new one comes in from above (the other way seeking
   back). A change coming faster than the roll shortens it rather than queueing ---- */
#define ROLL_MAX 24
typedef struct { char c[ROLL_MAX][5]; float w[ROLL_MAX]; int n; float tot; } RollStr;
static struct { char key[48]; RollStr cur, prev; Mo t; int dir; double at; float dw; } RL;
static void roll_split(const char *s, RollStr *o) {
  o->n = 0; o->tot = 0;
  while (*s && o->n < ROLL_MAX) { int k = 1; unsigned char b = (unsigned char)*s; if (b >= 0xf0) k = 4; else if (b >= 0xe0) k = 3; else if (b >= 0xc0) k = 2;
    char *c = o->c[o->n]; int i = 0; for (; i < k && s[i]; i++) c[i] = s[i]; c[i] = 0; s += i;
    o->w[o->n] = c[0] >= '0' && c[0] <= '9' && !c[1] ? RL.dw : text_width(c, 25, 600, 0) + .2f; o->tot += o->w[o->n]; o->n++; }
}
static float roll_layout(const char *txt, int dir) {
  if (RL.dw <= 0) for (char d = '0'; d <= '9'; d++) { char b[2] = { d, 0 }; float w = text_width(b, 25, 600, 0) + .2f; if (w > RL.dw) RL.dw = w; }
  if (strcmp(RL.key, txt)) {
    int first = !RL.key[0]; double now = now_(); float d = first ? 0 : (float)fmin(.18, fmax(.07, now - RL.at)); /* held keys: the roll keeps up */
    RL.prev = RL.cur; snprintf(RL.key, sizeof RL.key, "%s", txt); roll_split(txt, &RL.cur); RL.dir = dir < 0 ? -1 : 1; RL.at = now;
    if (first) { RL.prev = RL.cur; mo_set(&RL.t, 1); } else { mo_set(&RL.t, 0); mo_to(&RL.t, 1, d, C_OUT, 0); }
  }
  return RL.cur.tot;
}
static void roll_draw(float cx, float y, float lh, uint32_t col) {
  float x = cx + RL.cur.tot / 2, t = RL.t.v, dy = lh * .55f * RL.dir; int n = RL.cur.n > RL.prev.n ? RL.cur.n : RL.prev.n;
  for (int j = 0; j < n; j++) { int ic = RL.cur.n - 1 - j, ip = RL.prev.n - 1 - j; /* slots counted from the right: the seconds stay put */
    const char *a = ic >= 0 ? RL.cur.c[ic] : NULL, *b = ip >= 0 ? RL.prev.c[ip] : NULL; float w = a ? RL.cur.w[ic] : RL.prev.w[ip], mx = x - w / 2; x -= w;
    if (t >= 1 || (a && b && !strcmp(a, b))) { if (a) text_draw(a, mx, y, 25, 600, col, 0, 0, ALIGN_CENTER); continue; }
    if (b) text_draw(b, mx, y + dy * t, 25, 600, col_alpha(col, 1 - t), 0, 0, ALIGN_CENTER);
    if (a) text_draw(a, mx, y - dy * (1 - t), 25, 600, col_alpha(col, t), 0, 0, ALIGN_CENTER); }
}
static void draw_osd(void); static void draw_tray(void); static void live_card_draw(void);
/* another source or episode from inside the player: the controls as they are on screen at that moment, taken ONCE as
   a picture, fade off as the controls do (.2 s) while the art comes back over them (.45 s); freed once gone */
static GfxTarget handSnap; static Mo handA;
static void hand_snap(void) {
  if (!handSnap.fbo && !gfx_target_create(&handSnap, 1280, 720)) return;
  gfx_target_begin(&handSnap, RGBA(0, 0, 0, 0)); draw_osd(); draw_tray(); gfx_target_end();
  mo_set(&handA, 1); mo_to(&handA, 0, .2f, C_EASE, 0);
}
static void hand_tick(float dt) { mo_tick(&handA, dt); if (handSnap.fbo && !handA.on && handA.v <= 0.001f) gfx_target_free(&handSnap); }
static void hand_draw(void) { if (handSnap.fbo && handA.v > 0.001f) gfx_image(handSnap.tex, 0, 0, SB_W, SB_H, 0, WHITE_A((int)(255 * handA.v + .5f)), 0, 1, 1, 0); }
static void np_stop(void);
static void prepare(const char *vid, double startAt, int keepSubs) {
  resumeMsg[0] = 0; loadGen++; np_stop(); /* (the next episode's pictures made ahead: kept, this one opens with them) */
  int again = P.open; char type[16]; snprintf(type, sizeof type, "%s", P.type);
  float artWas = again ? P.artA.v : 1; /* the art comes back from where it is (the page's .45 s), never at once */
  if (again && P.live && artWas < 0.999f) hand_snap();
  if (again) { video_unload(); hls_conv_stop(); P.live = 0; asssubs_close(); if (mkv) { prevspike_stop(); sp_close(); mkv_close(mkv); mkv = NULL; } if (P.isP2p || P.resolving) p2p_player_switching(); } /* (another source inside the player: the torrent kept until the next one is known, p2p.c) */
  SubEntry *keep = NULL, *keepA = NULL; int nkeep = 0, nkeepA = 0; if (again && keepSubs && !strcmp(vid, P.vid)) { if (P.nExt) { keep = P.ext; nkeep = P.nExt; P.ext = NULL; } if (P.nAdx) { keepA = P.adx; nkeepA = P.nAdx; P.adx = NULL; } }
  cJSON *meta = P.meta; P.meta = NULL; subs_free(P.cues, P.ncues); free(P.ext); free(P.adx); srcs_free(); cols_clear();
  char id[SB_ID]; snprintf(id, sizeof id, "%s", P.id); char name[256]; snprintf(name, sizeof name, "%s", P.name); char poster[512]; snprintf(poster, sizeof poster, "%s", P.poster); char bg[512]; snprintf(bg, sizeof bg, "%s", P.bg); char logo[512]; snprintf(logo, sizeof logo, "%s", P.logo); unsigned subReq = P.subReq, adxReq = P.adxReq;
  char v[SB_VID]; snprintf(v, sizeof v, "%s", vid && *vid ? vid : id);
  sync_stop(0); memset(&P, 0, sizeof P); sub_reset(); P.subReq = subReq; P.adxReq = adxReq; P.open = 1; P.openedAt = now_(); P.seekT = -1; P.curAud = -1; snprintf(P.curSub, sizeof P.curSub, "off");
  snprintf(P.type, sizeof P.type, "%s", type); snprintf(P.id, sizeof P.id, "%s", id); snprintf(P.vid, sizeof P.vid, "%s", v); if (aud_original_mode()) (void)tmdb_orig_lang(P.id); /* (Original audio: its language on its way while the film loads) */ snprintf(P.name, sizeof P.name, "%s", name); snprintf(P.poster, sizeof P.poster, "%s", poster); snprintf(P.bg, sizeof P.bg, "%s", bg); snprintf(P.logo, sizeof P.logo, "%s", logo);
  P.meta = meta; P.ext = keep; P.nExt = nkeep; P.adx = keepA; P.nAdx = nkeepA; P.startAt = startAt; aspect_load(); set_status("");
  if (!strcmp(P.type, "series")) { const char *c = strrchr(P.vid, ':'); if (c && c > P.vid) { const char *q = c - 1; while (q > P.vid && *q != ':') q--; if (*q == ':') { P.season = atoi(q + 1); P.episode = atoi(c + 1); } } } /* known from the id, the metadata or not */
  memset(&P.pg.g, 0, sizeof P.pg.g); P.pg.age = 0; P.pg.certAge = -1; P.pg.guideIn = P.pg.certIn = 0; P.pg.asked = 0; P.pg.sess++; P.pg.at = P.pg.askAt = 0; mo_set(&P.pgA, 0); mo_set(&P.pgY, -10);
  mo_set(&P.artA, artWas); mo_set(&P.in, again ? 1 : 0); mo_set(&P.lnA, 1); mo_set(&P.pwA, 0); mo_set(&P.pwF, 0); P.pw.on = 0; mo_set(&P.prB, 104); mo_set(&P.subB, SB_H * pos_pct() / 100.0f); mo_set(&P.ttlX, -30); mo_set(&P.barY, 18); mo_set(&P.btnY, 24); mo_set(&P.topY, -12); mo_set(&P.prY, 20); mo_set(&P.skS, 0); mo_set(&P.ccS, 1);
  fill_from_meta(); if (!P.logo[0] && !strncmp(P.id, "tt", 2)) snprintf(P.logo, sizeof P.logo, "https://images.metahub.space/logo/medium/%s/img", P.id);
  if (!P.bg[0]) { Item it = lib_item(); sb_bg_mid(&it, P.bg, sizeof P.bg); }
  lx_begin(again);
}
static void on_p2p(void *ud, const char *url, const char *err, uint64_t size) {
  if (!P.open || P.closing || (unsigned)(uintptr_t)ud != p2pTok) return;
  P.resolving = 0;
  if (!url) { fail(err ? err : T("No peers found for this torrent")); return; }
  snprintf(P.url, sizeof P.url, "%s", url); P.isP2p = 1; set_status("");
  if (size) { P.size = (double)size; char n[16]; if (P.size >= 1e9) { snprintf(n, sizeof n, "%.*f", P.size >= 1e10 ? 0 : 1, P.size / 1e9); snprintf(P.sizeTxt, sizeof P.sizeTxt, "%s", T1("{n} GB", "n", n)); } else { snprintf(n, sizeof n, "%d", (int)(P.size / 1e6 + 0.5)); snprintf(P.sizeTxt, sizeof P.sizeTxt, "%s", T1("{n} MB", "n", n)); } }
  start_video();
}
static void on_yt(const Trailer *t, const char *err, void *ud) {
  if (!P.open || P.closing || (unsigned)(uintptr_t)ud != p2pTok) return; P.resolving = 0; P.ytTok = 0;
  if (!t) { fail(T("Couldn’t play this YouTube video")); fprintf(stderr, "[player] YouTube: %s\n", err ? err : "?"); return; }
  snprintf(P.url, sizeof P.url, "%s", t->url); snprintf(P.alt, sizeof P.alt, "%s", t->alt); start_video();
}
static int quietNext;
void player_quiet_next(void) { quietNext = 1; }
/* a preview playing in its box, picked: the player takes that very playback (nothing loaded again, no art), and the
   picture grows from the box to the screen while the page fades off around it */
static struct { int on; char src[SB_URL_MAX]; float x, y, w, h, r; } AD;
void player_adopt_next(const char *src, float x, float y, float w, float h, float r) { AD.on = 1; snprintf(AD.src, sizeof AD.src, "%s", src); AD.x = x; AD.y = y; AD.w = w; AD.h = h; AD.r = r; }
static float ease_io(double t) { t = t < 0 ? 0 : t > 1 ? 1 : t; return (float)(t < 0.5 ? 4 * t * t * t : 1 - (-2 * t + 2) * (-2 * t + 2) * (-2 * t + 2) / 2); }
static void grow_rect(float k, float *o) { o[0] = growR[0] * (1 - k); o[1] = growR[1] * (1 - k); o[2] = growR[2] + (SB_W - growR[2]) * k; o[3] = growR[3] + (SB_H - growR[3]) * k; } /* the box (k 0) to the screen (k 1) */
/* the plane told where the picture goes: at the pace this TV takes, never an edge moving back (out: growing) */
static void plane_step(const float *want, int out, int force) { VidPlaneCaps c = video_plane_caps(); double now = now_();
  if (!force && !c.perFrame && planeAt > 0 && (now - planeAt) * 1000 < c.stepMs) return;
  float x0 = want[0], y0 = want[1], x1 = want[0] + want[2], y1 = want[1] + want[3], s0 = planeSent[0], t0 = planeSent[1], s1 = planeSent[0] + planeSent[2], t1 = planeSent[1] + planeSent[3];
  if (!force) { if (out) { x0 = fminf(x0, s0); y0 = fminf(y0, t0); x1 = fmaxf(x1, s1); y1 = fmaxf(y1, t1); } else { x0 = fmaxf(x0, s0); y0 = fmaxf(y0, t0); x1 = fminf(x1, s1); y1 = fminf(y1, t1); } }
  if (x1 - x0 < 2 || y1 - y0 < 2) return;
  video_set_window(x0, y0, x1 - x0, y1 - y0); planeSent[0] = x0; planeSent[1] = y0; planeSent[2] = x1 - x0; planeSent[3] = y1 - y0; planeAt = now; growCalls++; }
/* Back while it grows (or before it started): the same window goes back the way it came, from where it is */
static void grow_reverse(void) {
  float kh = growPending ? 0 : growAt > 0 ? grow_k() : 1; growPending = 0; growAt = 0; /* (grown already: from the whole screen) */
  if (kh >= 1) { planeSent[0] = 0; planeSent[1] = 0; planeSent[2] = SB_W; planeSent[3] = SB_H; } /* (its framing since: the shrink starts from the screen) */
  double lo = 0, hi = SHRINK_D; for (int i = 0; i < 24; i++) { double m = (lo + hi) / 2; if (ease_io(m / SHRINK_D) < 1 - kh) lo = m; else hi = m; } /* (the shrink's moment whose window is this one) */
  shrinkAt = now_() - lo; planeAt = 0; growFrames = growCalls = 0; growWorst = 0; }
static float grow_k(void) { if (growPending) return 0; if (growAt <= 0) return 1; VidPlaneCaps c = video_plane_caps(); double t = now_() - growAt; if (c.cut) t -= 0.12; return ease_io(t / GROW_D); } /* (eased in and out: it leaves the box gently and lands gently) */
static float shrink_k(void) { if (shrinkAt <= 0) return 0; return ease_io((now_() - shrinkAt) / SHRINK_D); }
int player_page_morphing(void) { return P.open && (growPending || growAt > 0 || shrinkAt > 0); } /* a preview growing to the screen or shrinking back into its box */
float player_page_alpha(void) { return growAt > 0 ? 1 - grow_k() : shrinkAt > 0 ? shrink_k() : 1; }
/* switching channels full screen: the picture fades to black, stays black while the next one is found and loads (the
   corner pill says it), and the new picture fades up out of black once it runs; a switch that finds nothing fades
   the old one back */
static int zapOn, zapDown; static float zapF;
void player_zap_begin(void) { if (P.open && P.live && !P.closing) { zapOn = 1; zapDown = 0; } }
void player_zap_end(void) { zapOn = 0; }
static void zap_shade(void) {
  float dt = app_dt(); if (zapOn) { if (!P.live) zapDown = 1; else if (zapDown) zapOn = 0; } /* (the new picture runs: up it comes) */
  if (!P.open || P.closing || P.failed) zapOn = 0;
  zapF += zapOn ? dt / 0.15f : -dt / 0.3f; /* (down quickly at the switch, up as soon as the new picture runs) */ zapF = zapF < 0 ? 0 : zapF > 1 ? 1 : zapF;
  if (zapF > 0.003f) { float k = zapF * zapF * (3 - 2 * zapF); gfx_rect(0, 0, SB_W, SB_H, BLACK_A((int)(255 * k))); } }
static void play_stream(const Stream *s, const char *vid, double startAt) {
  src_use_add(s->addon); /* (one more play for this addon: the favourite goes first in Sources) */
  { extern int app_behind(void); if (app_behind()) awayHold = 0; } /* (a play started while behind, a phone's: it plays on) */
  if (!s->url[0]) { if (s->ext[0]) app_open_external(s->ext, s->name); return; } /* a page to open elsewhere: never played here */
  if (!can_play(s)) { toast(T("Couldn’t start the torrent engine"), 2500, NULL); return; }
  int adopt = AD.on && !strcmp(AD.src, s->url) && !P.open; AD.on = 0;
  prepare(vid, startAt, 1); P.quiet = quietNext || adopt; quietNext = 0; P.adopt = adopt; adoptFull = adopt && AD.w >= SB_W - 1 && AD.h >= SB_H - 1; if (adopt) { mo_set(&P.artA, 0); growR[0] = AD.x; growR[1] = AD.y; growR[2] = AD.w; growR[3] = AD.h; growRad = AD.r > 0 ? AD.r : 26; } /* (switching channels: no art, the corner pill alone says it tunes) */
  if (!strcmp(P.type, "other")) { memcpy(trSubs, trNext, sizeof trSubs); trSubsN = trNextN; snprintf(P.trName, sizeof P.trName, "%.*s", (int)strcspn(s->name, "\n"), s->name); }
  else trSubsN = 0;
  trNextN = 0;
  P.st = *s; snprintf(P.url, sizeof P.url, "%s", s->url); snprintf(P.alt, sizeof P.alt, "%s", nextAlt); nextAlt[0] = 0;
  if ((s->hdrs[0] || s->rhdrs[0]) && !strncmp(s->url, "http", 4)) { hproxy_url(s->url, s->hdrs, s->rhdrs, P.url, sizeof P.url); fprintf(stderr, "[player] the stream wants its own headers: through %s\n", P.url); } /* every request for it carries them */
  char info[700]; sb_stream_info(s, info, sizeof info); snprintf(P.sname, sizeof P.sname, "%s", info); snprintf(P.saddon, sizeof P.saddon, "%s", s->addon);
  if (!P.isTrailer && strcmp(P.type, "other")) sb_remember_stream(P.id, P.vid, s); /* the torrent itself, resolved again next time */
  if (!strncmp(P.url, "ytid:", 5)) { /* a YouTube video: found as the trailers are, behind the title art */
    P.resolving = 1; set_status(""); p2pTok++; P.ytTok = trailer_find(NULL, P.url + 5, 0, on_yt, (void *)(uintptr_t)p2pTok); return; }
  if (!strncmp(P.url, "p2p://", 6)) { /* the engine finds the peers and the file while the title art shows how it goes */
    P.resolving = 1; set_status(""); p2pTok++; p2p_resolve(P.url, on_p2p, (void *)(uintptr_t)p2pTok); return; } /* the loading line alone while it is quick; a reason once it is slow */
  start_video();
}
/* "Play the best stream automatically": the title art comes up at once with one quiet line about the wait, Back
   cancels, and the pick plays through the normal path */
static unsigned bestTok, bestSess; static double bestPos = -1; /* a phone's playbest: where to start (0 from the start); -1: as a choice on the TV */
static void on_best_status(const char *t, void *ud) { if (P.open && !P.closing && !P.started && bestTok && (unsigned)(uintptr_t)ud == bestSess) set_status(t); }
static void on_best_done(const Stream *s, const BestRate *r, void *ud) {
  if (!P.open || P.closing || P.started || (unsigned)(uintptr_t)ud != bestSess) return; bestTok = 0;
  if (!s) { fail(T("No stream suits this TV. Pick one from the list")); return; }
  fprintf(stderr, "[best] %s\n", r ? r->sum : ""); if (r) set_status(r->sum);
  Stream c = *s; double at = bestPos; bestPos = -1; play_stream(&c, P.vid, at > 0 ? at : at == 0 ? -1 : 0); /* (a phone's: its moment, or from the start) */
}
/* the logo the page showed (its own address, which can differ from the details' one: a translated logo, a board's):
   the player shows that very picture, already loaded, so it always flies in and is never drawn again */
static char pageLogo[512], pageLogoId[SB_ID];
static void open_title(const char *type, const char *id) {
  if (bestTok) { best_choose_cancel(bestTok); bestTok = 0; }
  if (!P.open) { memset(&P, 0, sizeof P); sx_begin_quick(1); /* the page's logo moves into the player */
    char sk[80]; snprintf(sk, sizeof sk, "logo:%s", id); const char *u = img_src_of(sx_from_tex(sk));
    snprintf(pageLogo, sizeof pageLogo, "%s", u ? u : ""); snprintf(pageLogoId, sizeof pageLogoId, "%s", id); }
  if (P.open && strcmp(P.id, id)) { P.name[0] = P.poster[0] = P.bg[0] = P.logo[0] = P.epTitle[0] = P.year[0] = P.genre[0] = 0; P.season = P.episode = 0; if (P.meta) { cJSON_Delete(P.meta); P.meta = NULL; } P.videos = NULL; } /* another title while it is open (a phone casting): never the last one's art while this one loads */
  snprintf(P.type, sizeof P.type, "%s", type); snprintf(P.id, sizeof P.id, "%s", id);
  cJSON *m = sb_meta_cached(type, id); if (!m && !strcmp(type, "other")) { m = sb_meta_cached("movie", id); if (!m) m = sb_meta_cached("series", id); } /* a trailer: the title's own art */
  if (m) { if (P.meta) cJSON_Delete(P.meta); P.meta = cJSON_Duplicate(m, 1); } else if (strcmp(type, "other")) { if (P.meta) cJSON_Delete(P.meta); P.meta = NULL; P.videos = NULL; ask_meta(); }
  if (P.meta) { snprintf(P.name, sizeof P.name, "%s", js_str(P.meta, "name", "")); snprintf(P.poster, sizeof P.poster, "%s", js_str(P.meta, "poster", "")); Item it = lib_item(); snprintf(it.background, sizeof it.background, "%s", js_str(P.meta, "background", "")); sb_bg_mid(&it, P.bg, sizeof P.bg); }
  else { if (artBg[0]) snprintf(P.bg, sizeof P.bg, "%s", artBg); if (artLogo[0]) snprintf(P.logo, sizeof P.logo, "%s", artLogo); if (artPoster[0]) snprintf(P.poster, sizeof P.poster, "%s", artPoster); if (artName[0] && !P.name[0]) snprintf(P.name, sizeof P.name, "%s", artName); /* the pictures it was handed (a phone's) */
    Item it = lib_item(); if (!P.bg[0]) { it.background[0] = 0; sb_bg_mid(&it, P.bg, sizeof P.bg); } if (!P.logo[0]) sb_logo_of(&it, P.logo, sizeof P.logo); } /* else its pictures by its id while its details are on their way */
  artName[0] = artPoster[0] = artBg[0] = artLogo[0] = 0;
  { char pu[512], lu[512]; Item it = lib_item(); sb_poster_of(&it, pu, sizeof pu); sb_logo_of(&it, lu, sizeof lu); accent_scope(id, pu, lu); }
}
/* the title's art at once, before there is anything to play (a trailer being found): status lines on it, a failure on it,
   Back closes it; the play that follows starts behind the same art */
void player_prepare(const char *type, const char *id, const char *vid, const char *trailerName) {
  open_title(type, id); prepare(vid, 0, 0); snprintf(P.trName, sizeof P.trName, "%s", trailerName ? trailerName : "");
}
void player_status(const char *text) { if (P.open && !P.closing) set_status(text); }
void player_fail(const char *text) { if (P.open && !P.closing) fail(text); }
void player_close(void) { close_player(0, 0); }
int  player_waiting(void) { return P.open && !P.closing && !P.started && !P.failed; }
/* after a failure: the best of the other streams, behind the same art (the failed one is left out) */
static void try_next(void) {
  fprintf(stderr, "[player] trying the next stream\n"); prepare(P.vid, 0, 0);
  set_status(T("Choosing the best stream")); bestSess++;
  bestTok = best_choose(P.type, P.vid, best_dur(P.meta, P.type), on_best_status, on_best_done, (void *)(uintptr_t)bestSess);
}
void player_cast_best(const char *type, const char *id, const char *vid, double pos) { player_choose_best(type, id, vid); bestPos = pos > 0 ? pos : 0; }
void player_choose_best(const char *type, const char *id, const char *vid) {
  bestPos = -1; open_title(type, id); prepare(vid, 0, 0);
  set_status(T("Choosing the best stream")); bestSess++;
  bestTok = best_choose(type, P.vid, best_dur(P.meta, type), on_best_status, on_best_done, (void *)(uintptr_t)bestSess);
}
/* a channel's name and pictures from its card, for one no addon describes (its details, when they come, take over) */
void player_reapply_window(void) { if (P.open) apply_aspect(); } /* (the hub gives the picture back) */
void player_title_hint(const Item *it) {
  if (!P.open || strcmp(P.id, it->id)) return;
  if (!P.name[0]) snprintf(P.name, sizeof P.name, "%s", it->name); if (!P.poster[0]) snprintf(P.poster, sizeof P.poster, "%s", it->poster);
  if (!P.bg[0]) snprintf(P.bg, sizeof P.bg, "%s", it->background[0] ? it->background : it->poster); if (!P.logo[0]) snprintf(P.logo, sizeof P.logo, "%s", it->logo);
}
void player_play(const Stream *s, const char *type, const char *id, const char *vid, int fromStart) { open_title(type, id); play_stream(s, vid, fromStart ? -1 : 0); }
/* ---- cast (core/castlink.c): a phone plays a stream here and drives it ---- */
void player_next_subs(const char *const *urls, const char *const *langs, int n) {
  memset(castSubs, 0, sizeof castSubs); if (n > 16) n = 16;
  for (int i = 0; i < n; i++) { SubEntry *e = &castSubs[i]; snprintf(e->url, sizeof e->url, "%s", urls[i]); snprintf(e->lang, sizeof e->lang, "%s", langs[i] && *langs[i] ? langs[i] : "und"); snprintf(e->name, sizeof e->name, "%s", e->lang); e->n = i + 1; e->from = SUB_STREAM; }
  castSubsN = n > 0 ? n : -1; /* (none from the phone, a best-stream play among them: the built-in OpenSubtitles list as on the TV; 0 kept it empty, "None found") */
}
void player_next_art(const char *name, const char *poster, const char *bg, const char *logo) {
  snprintf(artName, sizeof artName, "%s", name ? name : ""); snprintf(artPoster, sizeof artPoster, "%s", poster ? poster : ""); snprintf(artBg, sizeof artBg, "%s", bg ? bg : ""); snprintf(artLogo, sizeof artLogo, "%s", logo ? logo : ""); }
void player_play_at(const Stream *s, const char *type, const char *id, const char *vid, double pos) { open_title(type, id); play_stream(s, vid, pos > 0 ? pos : 0); }
void player_cast(int cmd, double pos) {
  if (!P.open || P.closing) return;
  if (P.pw.on) { if (cmd == 2 || cmd == 5) pw_end("Play now, from the phone"); else if (cmd == 4) close_player(0, 0); return; } /* (waiting for the seek previews: play or Play now starts it, as OK; a pause changes nothing, it is held already) */
  if (cmd == 1 && !paused()) { video_pause(); save_progress(1); } else if (cmd == 2 && paused()) play_on();
  else if (cmd == 3 && P.live) video_seek(pos < 0 ? 0 : pos); else if (cmd == 4) close_player(0, 0);
}
int player_cast_previews(int *pct, int *target, int *eta) { *pct = *target = *eta = 0; if (!P.open || P.closing || P.isTrailer || !P.pw.on) return 0;
  int p = sp_progress(); *pct = p < 0 ? 0 : p > 100 ? 100 : p; *target = P.pw.target; *eta = P.pw.etaShown && P.pw.eta > 0 && P.pw.eta <= 900 ? (int)ceil(P.pw.eta) : 0; return 1; } /* (the same numbers as the TV's line: made of 100, the share it waits for, the time left once known) */
/* Sync, for a phone's remote: running or done, in the tray's words (nothing while it checks quietly by itself) */
int player_cast_sync(char *st, int cap) { st[0] = 0; if (!P.open || P.closing || (!SY.phase && !SY.st[0])) return -1; if (!SY.quiet) snprintf(st, (size_t)cap, "%s", SY.st); return SY.phase ? 1 : 0; }
/* the subtitles and audio as the trays list them, for a phone: ids it hands back as they are ("off", "emb:<track>",
   "ext1:<n>" the stream's and OpenSubtitles', "ext2:<n>" the addons', audio "<track>" or "conv:<n>" a converted one) */
char *player_cast_tracks(void) {
  if (!P.open || P.closing) return NULL;
  cJSON *j = cJSON_CreateObject(), *sl = cJSON_AddArrayToObject(j, "subs"), *al = cJSON_AddArrayToObject(j, "audio"); char id[48], lb[200];
  cJSON_AddStringToObject(j, "t", "tracks");
  for (int i = 0; i < P.nEmb; i++) { cJSON *o = cJSON_CreateObject(); snprintf(id, sizeof id, "emb:%d", P.emb[i].idx); emb_label(i, lb, sizeof lb);
    cJSON_AddStringToObject(o, "id", id); cJSON_AddStringToObject(o, "label", lb); cJSON_AddStringToObject(o, "lang", P.emb[i].lang); cJSON_AddStringToObject(o, "from", T("Embedded")); cJSON_AddItemToArray(sl, o); }
  char cur[48] = "off";
  if (!strncmp(P.curSub, "emb:", 4)) snprintf(cur, sizeof cur, "%s", P.curSub);
  for (int li = 1; li <= 2; li++) { const SubEntry *l = li == 1 ? P.ext : P.adx; int n = li == 1 ? P.nExt : P.nAdx;
    for (int i = 0; i < n; i++) { const SubEntry *e = &l[i]; cJSON *o = cJSON_CreateObject(); snprintf(id, sizeof id, "ext%d:%d", li, i); const char *ln = lang_name(e->lang);
      if (*ln) snprintf(lb, sizeof lb, "%s%s%s", ln, e->label[0] ? " · " : "", e->label); else snprintf(lb, sizeof lb, "%s", T(e->name));
      cJSON_AddStringToObject(o, "id", id); cJSON_AddStringToObject(o, "label", lb); cJSON_AddStringToObject(o, "lang", e->lang); cJSON_AddStringToObject(o, "from", e->addon[0] ? e->addon : "OpenSubtitles"); cJSON_AddItemToArray(sl, o);
      if (P.pick.list == li && !strncmp(P.curSub, "ext:", 4) && !strcmp(P.curSub + 4, e->url)) snprintf(cur, sizeof cur, "%s", id); } }
  cJSON_AddStringToObject(j, "sub", cur);
  if (P.dts.conv) { MkvAudio ma[16]; int m = mkv ? mkv_audio_tracks(mkv, ma, 16) : 0;
    for (int i = 0; i < m; i++) { cJSON *o = cJSON_CreateObject(); snprintf(id, sizeof id, "conv:%d", i); const char *ln = lang_name(ma[i].lang); snprintf(lb, sizeof lb, "%s", *ln ? ln : ma[i].lang[0] ? ma[i].lang : T("Audio"));
      cJSON_AddStringToObject(o, "id", id); cJSON_AddStringToObject(o, "label", lb); cJSON_AddStringToObject(o, "lang", ma[i].lang); cJSON_AddItemToArray(al, o); }
    snprintf(id, sizeof id, "conv:%d", P.dts.convOrd); cJSON_AddStringToObject(j, "aud", id); }
  else { VidTrack ts[16]; int n = aud_list(ts, 16), sel = P.curAud >= 0 ? P.curAud : video_audio_current();
    for (int i = 0; i < n; i++) { cJSON *o = cJSON_CreateObject(); snprintf(id, sizeof id, "%d", ts[i].idx); const char *ln = lang_name(ts[i].lang);
      if (*ln) snprintf(lb, sizeof lb, "%s", ln); else if (ts[i].lang[0]) snprintf(lb, sizeof lb, "%s", ts[i].lang); else snprintf(lb, sizeof lb, "%s %d", T("Audio"), i + 1);
      if (ts[i].name[0] && strcmp(ts[i].name, lb)) { size_t L = strlen(lb); snprintf(lb + L, sizeof lb - L, " · %s", ts[i].name); }
      cJSON_AddStringToObject(o, "id", id); cJSON_AddStringToObject(o, "label", lb); cJSON_AddStringToObject(o, "lang", ts[i].lang); cJSON_AddItemToArray(al, o); }
    snprintf(id, sizeof id, "%d", sel); cJSON_AddStringToObject(j, "aud", id); }
  cJSON_AddNumberToObject(j, "delay", (int)lround(P.subDelay * 1000));
  cJSON_AddNumberToObject(j, "canSync", !strncmp(P.curSub, "ext:", 4) && !P.isTrailer);
  { int sc = P.isTrailer ? 0 : player_sub_scope(); cJSON_AddNumberToObject(j, "scope", sc); if (sc == 2) cJSON_AddNumberToObject(j, "pics", sub_pictures()); cJSON_AddNumberToObject(j, "subAr", sub_is_arabic()); } /* (a phone's remote: what the subtitles shown can take (0 all, 1 styled, 2 nothing; pictures or drawn by the TV), whether they are Arabic (its Arabic font row)) */ /* (Sync: possible on an addon's subtitles; how it goes comes with the state) */
  char *s = cJSON_PrintUnformatted(j); cJSON_Delete(j); return s;
}
/* a phone's pick, as the trays make it: kind "sub" (id or "off"), "audio" (id), "subdelay" (ms, absolute) */
void player_cast_track(const char *kind, const char *id, int ms) {
  if (!P.open || P.closing) return;
  if (!strcmp(kind, "subsync")) { if (!SY.phase || SY.quiet) sync_run(); return; } /* (a phone's Sync: as the tray's; pressed while it runs, nothing) */
  if (!strcmp(kind, "subdelay")) { double v = ms / 1000.0; if (v > 120) v = 120; if (v < -120) v = -120; P.subDelay = fabs(v) < 1e-6 ? 0 : v; P.dlyAt = now_(); refresh_tray(4); return; }
  if (!strcmp(kind, "sub")) {
    if (!strcmp(id, "off")) subs_off();
    else if (!strncmp(id, "emb:", 4)) set_emb_sub(atoi(id + 4));
    else if (!strncmp(id, "ext1:", 5) || !strncmp(id, "ext2:", 5)) { int li = id[3] - '0', k = atoi(id + 5); const SubEntry *l = li == 1 ? P.ext : P.adx; int n = li == 1 ? P.nExt : P.nAdx; if (k >= 0 && k < n) set_ext_sub(li, &l[k]); }
    show_pref_sub(); refresh_tray(4); return; }
  if (!strcmp(kind, "audio")) {
    if (!strncmp(id, "conv:", 5)) { int ord = atoi(id + 5);
      { MkvAudio ma[16]; int m = mkv ? mkv_audio_tracks(mkv, ma, 16) : 0; if (ord >= 0 && ord < m) show_pref_aud(ma[ord].lang, NULL); } /* (as the tray's pick) */
      if (P.dts.conv == 2 && ord != P.dts.convOrd && ord >= 0) { P.dts.convOrd = ord; P.dts.conv = 1; P.dts.convAt = video_time(); P.dts.convT0 = now_();
        int fl = conv_flags(ord); P.dts.copyA = !!(fl & HC_COPYAUDIO); video_unload(); P.live = 0; P.liveT0 = 0; hls_conv_start(P.url, ord, fl); fprintf(stderr, "[dts] audio track %d instead (phone)\n", ord); } }
    else { int ai = atoi(id); P.autoAudDone = 1; set_audio(ai); VidTrack ts[16]; int n = video_audio_tracks(ts, 16); for (int i = 0; i < n; i++) if (ts[i].idx == ai) show_pref_aud(ts[i].lang, ts[i].name); }
    refresh_tray(3); }
}
/* a torrent's peers and speed as the player's corner shows them ("" when none), for a phone's remote */
const char *player_p2p_line(void) { return P.open && !P.closing && !P.isTrailer && P.isP2p ? P.p2pLine : ""; }
/* the info line as the player shows it (resolution, its picture and sound, the size | the sound playing | the subtitles on), for a phone's remote */
void player_cast_chips(void *arr) { cJSON *a = arr; if (!a || !P.open || P.closing || P.isTrailer) return;
  for (int i = 0; i < P.nchip && i < 8; i++) { cJSON *o = cJSON_CreateObject(); if (P.chip[i].sep) cJSON_AddNumberToObject(o, "sep", 1); else { cJSON_AddStringToObject(o, "t", P.chip[i].t); if (P.chip[i].icon) cJSON_AddStringToObject(o, "i", P.chip[i].icon); if (P.chip[i].bold) cJSON_AddNumberToObject(o, "b", 1); } cJSON_AddItemToArray(a, o); } }
/* the stream in a few words, for a phone's remote ("1080p · HDR10 · DDP 5.1"): the decoder's HDR and the playing track's sound once known, the stream's name before */
void player_cast_info(char *out, int cap) { out[0] = 0; if (!P.open || P.closing || P.isTrailer || cap < 2) return; char hdr[32], codec[48], chs[8], au[64]; int n = 0;
  play_av(hdr, sizeof hdr, codec, sizeof codec, chs, sizeof chs); snprintf(au, sizeof au, "%s%s%s%s", !strncmp(codec, "EAC3", 4) ? "DDP" : codec, !strncmp(codec, "EAC3", 4) ? codec + 4 : "", codec[0] && chs[0] ? " " : "", codec[0] ? chs : ""); /* (the phone says DDP) */
  const char *parts[3] = { P.res && *P.res ? P.res : "", hdr, au }; for (int i = 0; i < 3; i++) if (parts[i][0] && n < cap - 1) n += snprintf(out + n, (size_t)(cap - n), "%s%s", n ? " \xc2\xb7 " : "", parts[i]); }
/* what plays, for a phone's remote: its type, id and episode ("" when nothing) */
void player_cast_ident(char *type, int tc, char *id, int ic, char *vid, int vc) { type[0] = id[0] = vid[0] = 0; if (!P.open || P.closing || P.isTrailer) return; snprintf(type, (size_t)tc, "%s", P.type); snprintf(id, (size_t)ic, "%s", P.id); snprintf(vid, (size_t)vc, "%s", P.vid); }
int player_cast_state(double *pos, double *du, char *title, int cap) {
  *pos = *du = 0; if (title && cap) title[0] = 0;
  if (!P.open || P.closing) return 0;
  if (title && cap) snprintf(title, (size_t)cap, "%s", P.name);
  if (P.failed) return 5;
  if (bestTok && !P.started) return 6; /* (CAST_CHOOSING: finding the best stream) */
  if (P.pw.on) return 1; /* (held on its first frame for the seek previews: loading, never "paused", to a phone) */
  VidState vs = video_state(); if (!P.live) return vs == VID_ERROR ? 5 : 1;
  *pos = video_time(); *du = dur();
  return vs == VID_ENDED ? 4 : vs == VID_ERROR ? 5 : paused() ? 3 : vs == VID_LOADING || vs == VID_BUFFERING ? 1 : 2;
}
static int diaryWant; static Item diaryIt;
static void finish_close(void) { if (diaryWant) { diaryWant = 0; diary_ask(&diaryIt); } if (hub_on() && hub_player_mode()) hub_close(); /* (one from Home stays: it is the page) */ { extern void detail_refresh_watched(void); detail_refresh_watched(); } /* the title page under it: its ticks as they are now */ sync_stop(0); gfx_target_free(&handSnap); mo_set(&handA, 0); if (!keepVideo) video_unload(); keepVideo = 0; /* (back into its preview: it plays on there) */ asssubs_close(); if (mkv) { prevspike_stop(); sp_close(); mkv_close(mkv); mkv = NULL; } p2p_player_closed(); p2pTok++; metaTok++; P.open = 0; P.closing = 0; if (P.meta) cJSON_Delete(P.meta); P.meta = NULL; subs_free(P.cues, P.ncues); free(P.ext); free(P.adx); srcs_free(); cols_clear(); memset(&P, 0, sizeof P); sub_reset(); free(extAss); extAss = NULL; app_refresh_boards(); }
static int closeNext, closeSim, closeArt; static double closeAt, closeT0; /* closeArt: closed on the loading screen (its line and words fade as the logo flies back) */
static void close_player(int goNext, int goSim) {
  if (!P.open || P.closing) return; multi_stop(); if (bestTok) { best_choose_cancel(bestTok); bestTok = 0; } P.closing = 1; ps_hold(0); sp_verify_reset(); np_stop(); NP.vid[0] = 0; /* (the next episode's pictures made so far: kept) */ /* (a lighter copy not taken yet: it reads on freely, nothing plays) */ closeArt = !P.live && P.artA.v > 0.999f; closeT0 = now_(); sx_begin_quick(0); /* the player's logo moves back to the page */ closeNext = goNext; closeSim = goSim; P.ssOn = 0; P.nx.tok = 0; P.sk.tok = 0;
  { double d = dur(), t = video_time(); int fin = d > 600 && (P.ended || t / d >= 0.85);
    if (!P.live && !P.isTrailer && fin && !live_title() && !strncmp(P.id, "tt", 2)) { cJSON *o = cJSON_CreateObject(); cJSON_AddStringToObject(o, "type", P.type); cJSON_AddStringToObject(o, "id", P.id); cJSON_AddStringToObject(o, "name", P.name); store_set_node("becauseOf", o); extern void because_changed(void); because_changed(); } /* etapa-87: Home's "Because you watched" */
    if (!P.live && !P.isTrailer && fin && !strcmp(P.type, "movie") && d > 1200) { Item it = lib_item(); diaryWant = 1; diaryIt = it; }
    else if (!P.live && !P.isTrailer && fin && !strcmp(P.type, "series") && P.season > 0) { /* etapa-83: a season's last episode: the season's stars */
      int last = 0; const cJSON *v; cJSON_ArrayForEach(v, js_arr(P.meta, "videos")) if ((int)js_num(v, "season", -1) == P.season) { int e = (int)js_num(v, "episode", js_num(v, "number", 0)); if (e > last) last = e; }
      if (last > 1 && P.episode >= last) { Item it = lib_item(); it.ep = P.season; diaryWant = 1; diaryIt = it; } } } /* etapa-79: the diary asks, once the player has gone */
  scrobble_player_end(video_time(), dur(), 0); tracking_player_end(video_time(), dur()); save_progress(1); video_pause(); /* (Test playback: nothing told, nothing kept) */
  /* the title's own picture fades back over the paused frame, the video goes, then the cover fades onto the app */
  closeAt = now_() + (closeArt ? 0.01 : 0.26); /* the cover fades back over the paused frame (see css()); from the loading screen the art fades at once, under the logo on its way back */
}
static void close_step(void) {
  if (!P.closing || now_() < closeAt) return;
  if (closeAt > 0) { closeAt = 0; video_unload(); hls_conv_stop(); P.live = 0; P.exitFade = 1;
    if (closeNext && closeVid[0]) { DetailParams p; memset(&p, 0, sizeof p); p.it = lib_item(); snprintf(p.it.videoId, sizeof p.it.videoId, "%s", closeVid); p.openStreams = 1; closeVid[0] = 0; app_go(SCR_DETAIL, &p, 0); }
    else if (closeSim) { DetailParams p; memset(&p, 0, sizeof p); p.it = lib_item(); app_go(SCR_SIM, &p, 0); }
    else if (closeColl) { closeColl = 0; DetailParams p; memset(&p, 0, sizeof p); p.it = closeCollIt; p.openStreams = 1; app_go(SCR_DETAIL, &p, 0); }
    else accent_scope(SCR == SCR_DETAIL ? P.id : NULL, P.poster, P.logo); }
  if (P.exitFade && P.in.v <= 0.001f && !P.in.on) finish_close();
}

/* ---- per frame ---- */
/* the info chips, grouped as the page groups them: [resolution] [HDR · audio format] [size] | [audio language + channels]
   [subtitles]. Kept per place: only a new chip comes in, each 0.05 s after the one before it (the subtitles' last); a
   chip whose words change (another subtitle, another audio track) crossfades them in place, its width gliding */
/* etapa-75: the chips that say quality wear their colours (4K gold, Dolby Vision violet to pink, HDR amber, Atmos blue), with a glass sheen; the rest as before */
static void chip_bg(const char *t, float x, float y, float w) {
  uint32_t c0 = 0, c1 = 0;
  if (strstr(t, "Dolby Vision") || strstr(t, "DV")) { c0 = RGB(0x8b, 0x5c, 0xf6); c1 = RGB(0xec, 0x48, 0x99); }
  else if (strstr(t, "4K") || strstr(t, "2160")) { c0 = RGB(0xf5, 0x9e, 0x0b); c1 = RGB(0xd9, 0x77, 0x06); }
  else if (strstr(t, "HDR")) { c0 = RGB(0xf9, 0x73, 0x16); c1 = RGB(0xf5, 0x9e, 0x0b); }
  else if (strstr(t, "Atmos")) { c0 = RGB(0x25, 0x63, 0xeb); c1 = RGB(0x06, 0xb6, 0xd4); }
  if (!c0) { gfx_rrect(x, y, w, 46, 23, RGBA(10, 12, 22, 140)); gfx_ring(x, y, w, 46, 23, 2, WHITE_A(56)); return; }
  if (1) { gfx_rrect(x, y, w, 46, 23, RGBA(12, 10, 14, 190)); gfx_ring(x, y, w, 46, 23, 2, c0); return; } /* etapa-87: the colour on its edge (the full colour washed out on the TV) */
  gfx_shadow(x, y + 6, w, 46, 23, 18, col_alpha(c0, 0.45f)); gfx_rrect_h(x, y, w, 46, 23, c0, c1);
  gfx_rrect_v(x, y, w, 23, 23, WHITE_A(70), WHITE_A(0)); gfx_ring(x, y, w, 46, 23, 2, WHITE_A(110)); }
static float chip_delay(int i) { return i >= 1 ? 0.05f * (i < 6 ? i : 6) : 0; }
static void chips_in(void) { double n = now_(); for (int i = 0; i < P.nchip; i++) { P.chip[i].at = n; P.chip[i].delay = chip_delay(i); P.chip[i].xAt = 0; } }
/* the picture and sound playing, for the info line and a phone's remote: the HDR the TV decodes once it says (the
   stream's name before: "Dolby Vision · HDR10" as that names it), the sound of the track playing by its codec and
   channels (a track's name may be its language alone), else the name's; codec in audio_of's words ("EAC3", "DTS-HD MA"), chs "5.1" */
static void play_av(char *hdr, size_t hc, char *codec, size_t cc, char *chs, size_t sc) {
  hdr[0] = codec[0] = chs[0] = 0;
  { char vh[32]; if (video_hdr(vh, sizeof vh) && vh[0] && strcasecmp(vh, "none")) snprintf(hdr, hc, "%s", !strncasecmp(vh, "dolby", 5) || !strncmp(vh, "DV", 2) ? "Dolby Vision" : vh); }
  if (!hdr[0]) { char h2[32]; hdr_of(P.sname, h2, sizeof h2); snprintf(hdr, hc, "%s%s", !strncmp(h2, "DV", 2) ? "Dolby Vision" : h2, !strncmp(h2, "DV", 2) ? h2 + 2 : ""); }
  { VidTrack ts[16]; int nt = aud_list(ts, 16), sel = P.curAud >= 0 ? P.curAud : video_audio_current(); /* (aud_list: the codec and channels from the file where the TV leaves them out) */
    for (int i = 0; i < nt; i++) if (ts[i].idx == sel) { const char *c = ts[i].codec; if (!strncmp(c, "A_", 2)) c += 2; /* (the file's names: A_EAC3, A_DTS...) */
      const char *nm = !strcasecmp(c, "eac3") || !strcasecmp(c, "ec-3") || !strcasecmp(c, "e-ac-3") ? "EAC3" : !strcasecmp(c, "ac3") || !strcasecmp(c, "ac-3") ? "AC3" : !strcasecmp(c, "dts-hd") || !strncasecmp(c, "dts/ma", 6) ? "DTS-HD MA" : !strcasecmp(c, "dts-x") ? "DTS:X" : !strncasecmp(c, "dts", 3) ? "DTS" : !strcasecmp(c, "truehd") ? "TrueHD" : !strncasecmp(c, "aac", 3) || !strncasecmp(c, "mp4a", 4) ? "AAC" : !strcasecmp(c, "opus") ? "Opus" : !strcasecmp(c, "flac") ? "FLAC" : "";
      if (*nm) { int ch = ts[i].channels; snprintf(codec, cc, "%s%s", nm, strcasestr(ts[i].name, "atmos") ? " Atmos" : ""); snprintf(chs, sc, "%s", ch >= 8 ? "7.1" : ch >= 6 ? "5.1" : ch == 2 ? "2.0" : ""); } break; } }
  if (!codec[0]) { char au[48]; audio_of(P.sname, au, sizeof au); snprintf(codec, cc, "%s", au); char *sp = strrchr(codec, ' '); if (sp && sp[1] >= '0' && sp[1] <= '9' && sp[2] == '.') { snprintf(chs, sc, "%s", sp + 1); *sp = 0; } }
}
static void info_update(void) {
  if (P.isTrailer) { P.nchip = 0; return; }
  char key[256]; static char hdr[32], codec[48], chs[8]; static double avAt; double tn = now_(); /* (the decoder tells its HDR and track after the first picture: asked twice a second, not every frame) */
  if (!P.infoKey[0] || tn - avAt > 0.5 || tn < avAt) { avAt = tn; play_av(hdr, sizeof hdr, codec, sizeof codec, chs, sizeof chs); }
  snprintf(key, sizeof key, "%s|%d|%d%s|%s|%s|%d|%s|%s%s", P.res ? P.res : "", P.curAud, P.pick.list, P.pick.addon, P.curSub, P.sizeTxt, P.nEmb, hdr, codec, chs); if (!strcmp(key, P.infoKey)) return;
  int first = !P.infoKey[0]; snprintf(P.infoKey, sizeof P.infoKey, "%s", key);
  char fmt[96]; snprintf(fmt, sizeof fmt, "%s%s%s", hdr, hdr[0] && codec[0] ? " · " : "", codec); /* the page's "  ·  " collapses to one space each side */
  char aud[96] = ""; { VidTrack ts[16]; int nt = aud_list(ts, 16); int sel = P.curAud >= 0 ? P.curAud : video_audio_current(); for (int i = 0; i < nt; i++) if (ts[i].idx == sel) { const char *ln = lang_name(ts[i].lang); snprintf(aud, sizeof aud, "%s", *ln ? ln : ts[i].name); } }
  if (chs[0]) { size_t l = strlen(aud); snprintf(aud + l, sizeof aud - l, "%s%s", l ? " " : "", chs); }
  char sub[140] = "";
  if (!strncmp(P.curSub, "emb:", 4)) { int nn = atoi(P.curSub + 4); for (int i = 0; i < P.nEmb; i++) if (P.emb[i].idx == nn) { char lb[120]; emb_label(i, lb, sizeof lb); snprintf(sub, sizeof sub, "%s · %s", lb, T("In the file")); } }
  else if (!strncmp(P.curSub, "ext:", 4)) { int i = sub_row(1, P.ext, P.nExt); if (i >= 0) { const char *ln = lang_name(P.ext[i].lang); snprintf(sub, sizeof sub, "%s · %s", *ln ? ln : T(P.ext[i].name), T("External")); }
    if ((i = sub_row(2, P.adx, P.nAdx)) >= 0) { const char *ln = lang_name(P.adx[i].lang); snprintf(sub, sizeof sub, "%s · %s", *ln ? ln : T(P.adx[i].name), P.adx[i].addon); } }
  struct { const char *t, *icon; int bold, sep; } nw[8]; int n = 0;
  if (P.res && *P.res) { nw[n].t = P.res; nw[n].icon = NULL; nw[n].bold = 1; nw[n++].sep = 0; }
  if (fmt[0]) { nw[n].t = fmt; nw[n].icon = NULL; nw[n].bold = 0; nw[n++].sep = 0; }
  if (P.sizeTxt[0]) { nw[n].t = P.sizeTxt; nw[n].icon = NULL; nw[n].bold = 0; nw[n++].sep = 0; }
  if (n && (aud[0] || sub[0])) { nw[n].t = "|"; nw[n].icon = NULL; nw[n].bold = 0; nw[n++].sep = 1; }
  if (aud[0]) { nw[n].t = aud; nw[n].icon = "p_spk"; nw[n].bold = 0; nw[n++].sep = 0; }
  if (sub[0]) { nw[n].t = sub; nw[n].icon = "p_cc"; nw[n].bold = 0; nw[n++].sep = 0; }
  double now = now_();
  for (int i = 0; i < n; i++) {
    int kept = i < P.nchip && P.chip[i].sep == nw[i].sep && P.chip[i].icon == nw[i].icon && !strcmp(P.chip[i].t, nw[i].t);
    int same = i < P.nchip && P.chip[i].sep == nw[i].sep && P.chip[i].icon == nw[i].icon && !nw[i].sep && P.chip[i].at && now - P.chip[i].at > P.chip[i].delay + .42; /* there and settled: only its words change */
    if (!kept && same) { int b = P.chip[i].bold; snprintf(P.chip[i].old, sizeof P.chip[i].old, "%s", P.chip[i].t); P.chip[i].oldW = text_width(P.chip[i].old, 24, b ? 800 : 600, b ? 0.5f : 0); P.chip[i].xAt = now; snprintf(P.chip[i].t, sizeof P.chip[i].t, "%s", nw[i].t); }
    else if (!kept) { snprintf(P.chip[i].t, sizeof P.chip[i].t, "%s", nw[i].t); P.chip[i].icon = nw[i].icon; P.chip[i].bold = nw[i].bold; P.chip[i].sep = nw[i].sep; P.chip[i].at = now; P.chip[i].delay = chip_delay(i); P.chip[i].xAt = 0; }
  }
  P.nchip = n; if (first) chips_in();
}
/* names and stories of the episodes in the app's language (TMDB), once they are known */
static void loc_apply(void) {
  if (P.locDone || P.isTrailer || strcmp(P.type, "series")) return;
  int st = tmdb_state(P.type, P.id, 0); if (st == TMDB_PENDING) return; P.locDone = 1; if (st != TMDB_READY) return;
  const TmdbLoc *L = tmdb_loc(P.type, P.id, 0); if (!L) return; const char *nm, *ov;
  if (P.season > 0 && P.episode > 0 && tmdb_episode(L, P.season, P.episode, &nm, &ov) && *nm) snprintf(P.epTitle, sizeof P.epTitle, "%s", nm);
  if (P.next.has) { int s = 0, e = 0; const char *c = strrchr(P.next.vid, ':'); if (c && c > P.next.vid) { const char *q = c - 1; while (q > P.next.vid && *q != ':') q--; s = atoi(q + 1); e = atoi(c + 1); }
    if (tmdb_episode(L, s, e, &nm, &ov)) { if (*nm) snprintf(P.next.title, sizeof P.next.title, "%s", nm); if (*ov) snprintf(P.next.ov, sizeof P.next.ov, "%s", ov); } }
}
/* a picture that stopped while the TV still says "playing": the time runs on between the TV's reports, so a frozen
   playhead shows as the time pulled back by each report (or, where it is not run on, as a time that does not move) */
static int stalled(VidState vs) {
  static double lastT = -1, backAt, sameSince; double t = video_time(), n = now_();
  if (vs != VID_PLAYING || P.seekT >= 0) { lastT = -1; backAt = 0; sameSince = 0; return 0; }
  if (lastT >= 0) { if (t < lastT - 0.25 && t > lastT - 5) backAt = n; /* (a bigger jump is a seek) */
    if (fabs(t - lastT) < 0.001) { if (!sameSince) sameSince = n; } else sameSince = 0; }
  lastT = t;
  return (backAt && n - backAt < 0.9) || (sameSince && n - sameSince > 0.5);
}
/* the viewer notice, asked as the picture comes (a trailer has none): IMDb's Parents Guide votes read as an age
   (any severe 16+, three severe 18+, moderate 13+, mild only 7+) and the reasons with how much; when the guide has
   nothing (or cannot be reached), the official rating's age alone (TMDB). Nothing usable within 5 s: no notice. */
static void pg_decide(void) {
  if (P.pg.at || !P.pg.asked || now_() - P.pg.askAt > 5) return; /* (shown once; never late) */
  if (P.pg.guideIn && P.pg.g.n) { int sev = 0, mod = 0; for (int i = 0; i < P.pg.g.n; i++) { sev += P.pg.g.lvl[i] == PG_SEVERE; mod += P.pg.g.lvl[i] == PG_MODERATE; }
    P.pg.age = sev >= 3 ? 18 : sev ? 16 : mod ? 13 : 7; P.pg.at = now_(); return; }
  if (P.pg.guideIn && P.pg.certIn && P.pg.certAge >= 0) { P.pg.age = P.pg.certAge; P.pg.at = now_(); } /* (the age alone, no reasons) */
}
static void on_pg(const ParGuide *g, void *ud) { if (!P.open || P.closing || (unsigned)(uintptr_t)ud != P.pg.sess) return; if (g) P.pg.g = *g; P.pg.guideIn = 1; fprintf(stderr, "[notice] guide: %d reasons\n", g ? g->n : -1); pg_decide(); }
static void on_age(const Rating *r, void *ud) { if (!P.open || P.closing || (unsigned)(uintptr_t)ud != P.pg.sess) return; P.pg.certAge = rating_age(r); if (prof_kid(-1) && P.pg.certAge > 12) { fprintf(stderr, "[player] Kids profile: %d+ not played\n", P.pg.certAge); toast(T("This title is not for the Kids profile"), 3500, NULL); close_player(0, 0); return; } /* etapa-74 */ P.pg.certIn = 1; snprintf(P.pg.cert, sizeof P.pg.cert, "%s", r ? r->rating : ""); P.pg.desc[0] = 0; for (int i = 0; r && i < r->ndesc && i < 3; i++) { size_t l = strlen(P.pg.desc); snprintf(P.pg.desc + l, sizeof P.pg.desc - l, "%s%s", l ? ", " : "", r->desc[i]); } /* (the rating's own reasons, when it gives them) */ fprintf(stderr, "[notice] rating \"%s\" -> age %d\n", r ? r->rating : "", P.pg.certAge); pg_decide(); }
static void pg_ask(void) { if (P.pg.asked || P.isTrailer || strncmp(P.id, "tt", 2)) return; P.pg.asked = 1; P.pg.askAt = now_();
  parental_get(P.id, on_pg, (void *)(uintptr_t)P.pg.sess); rating_get(P.type, P.id, on_age, (void *)(uintptr_t)P.pg.sess); }
/* the page's classes, as targets: each motion goes there with the duration, curve and delay the page gives that state */
static void css(void) {
  int tr = P.mode == 2 && P.tray.kind > 0, ss = P.ssOn, osd = P.osd && !ss, pz = P.pausedShown, skm = P.skm && !osd;
  if (osd && !tr) { mo_to(&P.ttlA, 1, .42f, C_OUT, 0); mo_to(&P.ttlX, 0, .42f, C_OUT, 0); mo_to(&P.ttlY, 0, .42f, C_OUT, 0); }
  else if (osd) { mo_to(&P.ttlA, 0, .42f, C_OUT, 0); mo_to(&P.ttlX, 0, .42f, C_OUT, 0); mo_to(&P.ttlY, 10, .42f, C_OUT, 0); } /* a tray takes its place: it sinks a little */
  else { mo_to(&P.ttlA, 0, .2f, C_EASE, 0); mo_to(&P.ttlX, -30, .2f, C_EASE, 0); mo_to(&P.ttlY, 0, .2f, C_EASE, 0); }
  if (osd) { mo_to(&P.barA, tr ? 0 : 1, .36f, C_EASE, .06f); mo_to(&P.barY, tr ? 10 : 0, .36f, C_OUT, .06f); mo_to(&P.btnA, 1, .36f, C_EASE, .12f); mo_to(&P.btnY, 0, .36f, C_OUT, .12f); }
  else { float bd = stripQuick ? .12f : seekK.v > 0.02f ? .45f : .2f; mo_to(&P.barA, skm ? 1 : 0, bd, C_EASE, 0); mo_to(&P.barY, skm ? 0 : 18, bd, C_EASE, 0); mo_to(&P.btnA, 0, .2f, C_EASE, 0); mo_to(&P.btnY, 24, .2f, C_EASE, 0); }
  if (ss) mo_to(&P.topR, 0, 1, C_EASE, 0); else { mo_to(&P.topR, osd ? 1 : 0, .3f, C_EASE, 0); mo_to(&P.topY, osd ? 0 : -12, .3f, C_EASE, 0); }
  if (ss) { mo_to(&P.dim, 0, 1, C_EASE, 0); mo_to(&P.dimT, 0, 1, C_EASE, 0); }
  else { mo_to(&P.dim, osd || pz ? 1 : skm ? .6f : 0, .3f, C_EASE, 0); mo_to(&P.dimT, osd || pz ? 1 : 0, .3f, C_EASE, 0); }
  mo_to(&P.vig, pz ? 1 : 0, .9f, C_EASE, 0); mo_to(&P.pzA, pz ? 1 : 0, .3f, C_EASE, 0);
  if (!P.closing && !ss && (!P.live || P.artA.v > 0.02f)) { mo_set(&P.dim, 0); mo_set(&P.dimT, 0); mo_set(&P.vig, 0); } /* the title's art fades straight into the picture: the shades come after it */
  if (P.closing) mo_to(&P.artA, 1, .2f, C_EASE, 0); else if (ss) mo_to(&P.artA, 1, 1.4f, C_EASE, 0); else mo_to(&P.artA, (P.live && !P.pw.on) || (P.quiet && !P.failed && !P.pw.on) ? 0 : 1, .45f, C_EASE, 0);
  if (P.exitFade) mo_to(&P.in, 0, .35f, C_EASE, 0); else mo_to(&P.in, 1, .22f, C_EASE, 0);
  mo_to(&P.lnA, P.live && !P.closing && !P.pw.on ? 0 : 1, .16f, C_EASE, 0);
  mo_to(&P.pwA, P.pw.on && P.pw.shown && !P.closing && !P.failed ? 1 : 0, P.pw.on ? .35f : .2f, C_EASE, P.pw.on ? .25f : 0); /* previews first: the bar's fill and Play now */
  if (P.pw.on && P.pw.shown) { double pc = sp_progress_f(); float f = pc <= 0 ? 0 : pc >= P.pw.target ? 1 : (float)(pc / P.pw.target); if (fabsf(f - P.pwF.to) > .004f) mo_to(&P.pwF, f, .5f, C_EASE, 0); } /* the line runs until the picture runs, then goes with the art */
  /* the spinner: loading, a seek on its way, or a picture that stopped moving; it comes after 0.3 s (a quick seek shows
     none) and goes as soon as the time moves again */
  VidState vs = video_state(); int wait = P.live && !paused() && (vs == VID_BUFFERING || vs == VID_LOADING || video_settling() || stalled(vs));
  if (wait) mo_to(&P.bufA, 1, .2f, C_EASE, .3f); else mo_to(&P.bufA, 0, .2f, C_EASE, 0);
  mo_to(&P.skA, P.skOn ? 1 : 0, stripQuick ? .12f : seekK.v > 0.02f ? .45f : .16f, C_EASE, 0); mo_to(&P.skS, P.skOn ? 1 : 0, .2f, C_SEEK, 0);
  mo_to(&P.knA, (P.mode == 1 && P.row == 1) || P.skOn ? 1 : 0, .15f, C_EASE, 0);

  mo_to(&P.prA, P.prompt.k ? 1 : 0, .3f, C_EASE, 0); mo_to(&P.prY, P.prompt.k ? 0 : 20, .3f, C_OUT, 0); mo_to(&P.prB, P.osd ? 440 : 104, .3f, C_EASE, 0);
  { float rest = SB_H * pos_pct() / 100.0f; mo_to(&P.subB, tr ? SB_H * 0.62f : P.osd ? fmaxf(420, rest) : P.skm ? fmaxf(200, rest) : rest, P.mode == 5 ? .16f : .3f, C_EASE, 0); } /* (the chosen height, never under the controls) */
  mo_to(&P.hvA, P.hvOn ? 1 : 0, .15f, C_EASE, 0); mo_to(&P.ccS, strcmp(P.curSub, "off") ? 0 : 1, .26f, C_OUT, 0);
  mo_to(&P.p2pA, P.isP2p && P.p2pLine[0] && osd ? 1 : 0, .3f, C_EASE, 0);
  { int on = P.pg.at > 0 && now_() - P.pg.at < 6.0; /* 6 s from when it comes, controls or not, then it fades; once only */
    mo_to(&P.pgA, on ? 1 : 0, on ? .5f : .55f, C_EASE, 0); mo_to(&P.pgY, on ? 0 : -14, on ? .6f : .55f, C_OUT, 0); }
  if (P.tray.kind > 0) { mo_to(&P.tray.open, 1, .38f, C_TRIN, 0); mo_to(&P.tray.openA, 1, .18f, C_EASE, 0); }
  else { mo_to(&P.tray.open, .12f, .26f, C_TROUT, 0); mo_to(&P.tray.openA, 0, .2f, C_EASE, .04f); }
}
static void foc_update(void) {
  int fi = P.bIdx; if (P.mode == 2 && P.tray.kind) { int ak = P.tray.kind < 0 ? -P.tray.kind : P.tray.kind; int b = btn_index(ak == 1 || ak == 5 ? "eps" : ak == 6 ? "chn" : ak == 2 ? "src" : ak == 3 ? "aud" : "subs"); if (b >= 0) fi = b; }
  if (fi >= P.nbtn) fi = P.nbtn - 1; if (fi < 0) return;
  /* the focused button opens into a pill with its name: the others slide aside, the pill and the name follow (0.32 s) */
  int show = P.live && ((P.mode == 1 && P.row == 0) || P.mode == 2); float lw = show && P.btns[fi].lb[0] ? P.btns[fi].lw : 0;
  for (int k = 0; k < P.nbtn; k++) { float dx = 0; if (fi < P.nL && k < P.nL && k > fi) dx = lw; if (fi >= P.nL && k >= P.nL && k <= fi) dx = -lw; mo_to(&P.btns[k].dx, dx, .32f, C_OUT, 0); }
  float x = P.btns[fi].x0 + (fi >= P.nL ? -lw : 0), y = P.btns[fi].y, sz = P.btns[fi].w / 76;
  float fd = P.focSeen && fabsf(x - P.focX.to) > 260 ? .42f : .32f; /* across the gap between the two groups: the same glide, a little longer (never a jump) */
  if (!P.focSeen) { mo_set(&P.focX, x); mo_set(&P.focY, y); mo_set(&P.focW, (76 + lw) * sz); mo_set(&P.focH, 76 * sz); }
  else { mo_to(&P.focX, x, fd, C_OUT, 0); mo_to(&P.focY, y, fd, C_OUT, 0); mo_to(&P.focW, (76 + lw) * sz, fd, C_OUT, 0); mo_to(&P.focH, 76 * sz, fd, C_OUT, 0); }
  P.focSeen = 1; mo_to(&P.focA, show ? 1 : 0, .18f, C_EASE, 0);
  if (strcmp(P.tip, P.btns[fi].lb)) { snprintf(P.tip, sizeof P.tip, "%s", P.btns[fi].lb); P.tipAt = now_(); } /* a new name: its entrance again */
  mo_to(&P.tipA, lw > 0 ? 1 : 0, .14f, C_EASE, 0);
}
/* an MP4's own chapters, read (mp4chap.c): as the MKV reader's (n <= 0: none, the skips go on without them) */
static void on_mp4_chapters(const char *url, const Mp4Chapter *l, int n, void *ud) { (void)ud;
  if (!P.open || strcmp(url, P.url)) return; PlayerChapter pc[64]; int k = 0;
  for (int i = 0; i < n && k < 64; i++) { snprintf(pc[k].name, sizeof pc[k].name, "%s", l[i].name); pc[k].s = l[i].s; pc[k].e = i + 1 < n ? l[i + 1].s : dur(); k++; }
  player_chapters(url, pc, k); }
/* a film or episode whose file brings fewer than two chapters gets them made (Settings: Chapters on), so every one has
   them: the opening, recap and end credits where the skip sources put them (named), and marks about every 10 minutes
   between (numbered), none closer than 90 s to another. Made once the file and the skip sources have answered */
static void chap_generate(void) {
  if (P.chapGen || P.nchap >= 2 || P.isTrailer || live_title() || P.liveStream || !P.started) return;
  double d = dur(); if (d < 600) return; if (!P.chapReadyAt) P.chapReadyAt = now_(); double w = now_() - P.chapReadyAt;
  if ((P.sk.chapWait || P.chapWant) && w < 20) return; /* (the file's own, still being read) */
  if (!P.sk.sent && w < 16) return; /* (the skip sources: 14 s at most) */
  P.chapGen = 1; double m[24]; int n = 0; m[n++] = 0;
  if (P.intro.has) m[n++] = P.intro.s; if (P.intro.has && P.intro.e < d - 60) m[n++] = P.intro.e;
  if (P.recap.has) m[n++] = P.recap.s; if (P.recap.has && P.recap.e < d - 60) m[n++] = P.recap.e;
  if (P.outro.has && P.outro.s > 60) m[n++] = P.outro.s;
  int parts = (int)(d / 600 + 0.5); if (parts < 2) parts = 2; if (parts > 16) parts = 16;
  for (int k = 1; k < parts && n < 24; k++) { double at = d * k / parts; int near = 0; for (int i = 0; i < n; i++) if (fabs(m[i] - at) < 90) near = 1; if (!near && (!P.outro.has || at < P.outro.s - 90)) m[n++] = at; }
  for (int i = 1; i < n; i++) for (int j = i; j > 0 && m[j] < m[j - 1]; j--) { double t = m[j]; m[j] = m[j - 1]; m[j - 1] = t; }
  P.nchap = 0; for (int i = 0; i < n && P.nchap < 64; i++) { if (P.nchap && m[i] - P.chap[P.nchap - 1].s < 20) continue; if (m[i] < 0 || m[i] >= d - 5) continue; P.chap[P.nchap].s = m[i]; P.chap[P.nchap].nm[0] = 0; P.nchap++; }
  P.chLast = -1; fprintf(stderr, "[player] %d chapters made (the file had none)\n", P.nchap); }
/* a film in a collection: the next one found, for the credits card (checked once a second until known) */
static void coll_next_check(void) {
  if (P.coll.tried || !P.started || P.isTrailer || strcmp(P.type, "movie") || now_() < P.coll.at) return; P.coll.at = now_() + 1;
  char key[24]; const char *nm; int r = coll_of("movie", P.id, key, sizeof key, &nm); if (r < 0) { P.coll.tried = 1; return; } if (r == 0) return;
  const Coll *c = coll_get(key); if (!c) return; P.coll.tried = 1; int ix = coll_index(c, P.id), nx = -1; if (ix < 0) return;
  for (int i = ix + 1; i < c->n; i++) if (!strcmp(c->part[i].type, "movie")) { nx = i; break; } if (nx < 0) return; /* (the next film: a franchise's shows passed over) */
  if (!c->part[nx].id[0]) { P.coll.tried = 0; return; } /* (its IMDb id on its way: asked again in a second) */
  const CollPart *x = &c->part[nx]; P.coll.has = 1; snprintf(P.coll.id, sizeof P.coll.id, "%s", x->id); snprintf(P.coll.name, sizeof P.coll.name, "%s", x->name); snprintf(P.coll.poster, sizeof P.coll.poster, "%s", x->poster);
  snprintf(P.coll.bg, sizeof P.coll.bg, "https://images.metahub.space/background/medium/%s/img", x->id);
  char a[16], b[16]; snprintf(a, sizeof a, "%d", nx + 1); snprintf(b, sizeof b, "%d", c->n); snprintf(P.coll.part, sizeof P.coll.part, "%s", T2("Part {n} of {m}", "n", a, "m", b));
  memset(&closeCollIt, 0, sizeof closeCollIt); snprintf(closeCollIt.type, sizeof closeCollIt.type, "movie"); snprintf(closeCollIt.id, sizeof closeCollIt.id, "%s", x->id); snprintf(closeCollIt.name, sizeof closeCollIt.name, "%s", x->name); snprintf(closeCollIt.poster, sizeof closeCollIt.poster, "%s", x->poster);
  fprintf(stderr, "[player] next in its collection: %s\n", x->id); }
/* behind another app (#69, core/away.c): a film playing is paused where it is (kept, Trakt told by the pause, as the
   remote's Pause does); one still loading is held as it starts. A play started while behind (a phone's) plays on.
   Back in front: a film paused for it stays paused, its controls up */
void player_away(int behind) {
  if (!behind) { if (awayHold && P.open && !P.closing && paused()) show_osd(1); awayHold = 0; return; }
  if (!P.open || P.closing) return;
  awayHold = 1; if (!paused() && video_state() == VID_PLAYING) { video_pause(); save_progress(1); fprintf(stderr, "[player] behind another app: paused at %.0f s\n", video_time()); } }
void player_tick(float dt) {
  if (awayHold && (!P.open || P.closing)) awayHold = 0;
  if (awayHold && video_state() == VID_PLAYING) { extern int app_behind(void); if (app_behind()) { video_pause(); save_progress(1); fprintf(stderr, "[player] behind another app: held as it started\n"); } }
  if (P.open) lx_tick(dt);
  status_tick(); pw_tick(); ps_tick(); if (getenv("SB_PREV_LOG")) sp_dev_log(); { static double npAt; if (now_() - npAt > 1) { npAt = now_(); np_tick(); } }
  { int sk = P.open && P.seekT >= 0 && P.seekScrub && prev_enabled() && sp_active() && sp_made() > 0 && !live_bcast(); /* (no picture yet: the seek pill alone, never a row of loaders) */ /* a scrub of the viewer's with previews: in 0.18 s, out 0.45 s where it was let go */
    static double skLast; if (sk) { skLast = now_(); stripQuick = 0; } else if (stripQuick) skLast = -1e9; else if (stripT >= 0 && P.open && now_() - skLast < 0.6) sk = 1; /* (between two presses the strip stays: it never closes and opens again; OK on a picture: it goes at once) */
    if (sk) { if (stripT < 0 || seekK.v <= 0.01f) stripFrom = video_time(); if (P.seekT >= 0 && fabs(P.seekT - stripT) > 0.01) { stripT = P.seekT; stripMovedAt = now_(); } } stripHold = sk; tween_to(&seekK, sk, sk ? 0.18f : stripQuick ? 0.08f : 0.45f, EASE_CSS_EASE); tween_tick(&seekK, dt); if (!sk && seekK.v <= 0.001f) { stripT = -1; stripQuick = 0; } }
  coll_next_check();
  hub_tick(dt);
  hand_tick(dt);
  subs_pump(); /* subtitle files and lists, fetched on their own threads */
  if (!P.open) return; video_pump(); sync_tick();
  css();
  Mo *mo[] = { &P.pgA, &P.pgY, &P.in, &P.artA, &P.ttlA, &P.ttlX, &P.ttlY, &P.barA, &P.barY, &P.btnA, &P.btnY, &P.topR, &P.topY, &P.dim, &P.dimT, &P.vig, &P.pzA, &P.bufA, &P.lnA, &P.pwA, &P.pwF, &P.skA, &P.skS, &P.knA,
    &P.prA, &P.prY, &P.prB, &P.subB, &P.ccS, &P.dlyA, &P.dlyR, &P.posA, &P.p2pA, &P.focX, &P.focY, &P.focW, &P.focH, &P.focA, &P.tipA, &P.hvA, &P.tray.open, &P.tray.openA, &P.tray.rowX, &P.tray.tabsX, &P.tray.ringX, &P.tray.ringY, &P.tray.ringA, &RL.t };
  if (P.skA.v <= 0.001f && !P.skOn) RL.key[0] = 0; /* the pill gone: its next time shows at once */
  for (size_t i = 0; i < sizeof mo / sizeof *mo; i++) mo_tick(mo[i], dt);
  for (int i = 0; i < 4; i++) { mo_tick(&P.tray.colY[i], dt); mo_tick(&P.tray.plY[i], dt); mo_tick(&P.tray.plA[i], dt); }
  for (int i = 0; i < P.nbtn; i++) { mo_tick(&P.btns[i].dx, dt); P.btns[i].x = P.btns[i].x0 + P.btns[i].dx.v; }
  nudge_tick(&P.nudge, dt);
  if (P.tray.kind < 0 && !P.tray.openA.on && P.tray.openA.v <= 0.001f) { P.tray.kind = 0; cols_clear(); srcs_free(); } /* put away: its lists go too */
  mo_tick(&SR.y, dt); mo_tick(&SR.plY, dt); mo_tick(&SR.plH, dt); mo_tick(&SR.plA, dt); tm_tick(dt); tray_swap_step(); src_tick(dt);
  if (!P.prompt.k && P.prA.v <= 0.001f) P.shown.k = 0;
  close_step(); if (P.closing) return;
  if (P.resolving) { const char *st = p2p_status(); if (strcmp(st, status)) set_status(st); return; } /* the art: nothing for a few seconds, then why it is slow ("Finding peers…", "Getting the file list…") */
  VidState vs = video_state();
  /* Trakt: what plays, every frame (it decides when to tell; trailers and live broadcasts never) */
  if (!P.isTrailer && !live_bcast()) { int ep = episodic(); ScrobbleItem si = { P.type, P.id, P.name, P.year, ep ? P.season : -1, ep ? P.episode : -1 };
    scrobble_player(&si, vs == VID_PLAYING && video_live() ? 1 : paused() ? 2 : 0, video_time(), dur());
    tracking_player(P.type, P.vid[0] ? P.vid : P.id, vs == VID_PLAYING && video_live() ? 1 : paused() ? 2 : 0, video_time(), dur()); } /* (and the tracking addons) */
  if (P.started && mkv && !P.isTrailer && !P.isP2p && !sp_active()) { static double spAt; if (now_() - spAt > 0.1) { spAt = now_(); double t0 = video_time(), b0 = video_buffered_end(); char sk[300]; snprintf(sk, sizeof sk, "%s|%s|%.0f|%.0f", P.id, P.vid, P.size, dur()); sp_player_tick(prev_src_pick(), sk, 0, live_bcast() || !strcmp(P.type, "tv") || !strcmp(P.type, "channel"), t0, dur(), b0 > t0 ? b0 - t0 : -1, (vs == VID_BUFFERING || vs == VID_LOADING) && !P.pw.on, paused() || P.pw.on); } } /* (seek previews not started yet: asked ten times a second, so they start the moment the index is in) */
  { static double last = -9; static int was; double d = dur(); double f = P.seekT >= 0 ? P.seekT : P.hvOn && d > 0 ? P.hvP * d : -1; int on = f >= 0; /* the remote's scrub, or the pointer over the bar: its pictures first (told as it moves) */
    if (on != was || (on && fabs(f - last) > 0.4)) { sp_focus(f, 0); last = f; was = on; } }
  if (P.started && now_() - P.posTick >= 1.0) { P.posTick = now_(); double t = video_time(), b = video_buffered_end(); if (t > 0) P.lastPos = t;
    if (mkv) mkv_set_pos(mkv, t, b > t ? b - t : -1);
    if (mkv && !P.isTrailer) prevspike_tick(mkv, vs == VID_PLAYING && P.live, t, b > t ? b - t : -1); /* (SB_PREVSPIKE only: seek previews measured) */
    { char sk[300]; snprintf(sk, sizeof sk, "%s|%s|%.0f|%.0f", P.id, P.vid, P.size, dur()); sp_player_tick(P.isTrailer || P.isP2p ? NULL : prev_src_pick(), sk, 0, live_bcast() || !strcmp(P.type, "tv") || !strcmp(P.type, "channel"), t, dur(), b > t ? b - t : -1, (vs == VID_BUFFERING || vs == VID_LOADING) && !P.pw.on, paused() || P.pw.on); } /* seek previews: what plays and how it goes (a debrid or direct file only: never a torrent on a TV) */
    dts_tick();
    if (!P.isTrailer && (!P.embKnown || !P.mkvTracksOk || mkv_state(mkv) == MKV_FALLBACK)) load_emb_tracks(); /* until the file's list settles (the TV's own list may come late) */
    if (P.isP2p) { p2p_pace(vs == VID_PLAYING, vs == VID_BUFFERING || vs == VID_LOADING, dur()); char ln[160]; int has = p2p_peer_line(ln, sizeof ln); snprintf(P.p2pLine, sizeof P.p2pLine, "%s", has ? ln : "");
      int waiting = (vs == VID_BUFFERING || vs == VID_LOADING) && !paused(); const char *why = p2p_wait_reason(waiting); /* one short reason under the ring once the wait is long */
      if (waiting && P.live && !strcmp(why, T("Waiting for the next pieces…"))) why = ""; /* mid-film the spinner says it alone */
      if (waiting && !P.failed) { if (strcmp(why, status)) set_status(why); } else if (vs == VID_PLAYING && status[0] && !P.failed) set_status(""); } }
  if (P.chapWant && P.sk.chapWait) { MkvChapter mc[64]; int n = mkv ? mkv_chapters(mkv, mc, 64) : 0; /* the file's chapters, once its head is read */
    if (n == 0 && !P.mp4Asked && (!mkv || mkv_state(mkv) == MKV_FALLBACK) && !strncmp(P.url, "http", 4) && !P.isHls && !P.isTrailer) { /* no MKV: an MP4's own chapters */
      P.chapWant = 0; P.mp4Asked = 1; mp4_chapters(P.url, on_mp4_chapters, NULL); }
    else if (n >= 0) { P.chapWant = 0; PlayerChapter pc[64]; for (int i = 0; i < n; i++) { snprintf(pc[i].name, sizeof pc[i].name, "%s", mc[i].name); pc[i].s = mc[i].s; pc[i].e = mc[i].e < 0 ? dur() : mc[i].e; } player_chapters(P.url, pc, n); } }
  chap_generate();
  if (P.dts.conv == 2 && !P.failed && (vs == VID_ERROR || (!video_live() && now_() - P.dts.convT0 > 25))) { dts_conv_failed(vs == VID_ERROR ? video_error() : "no picture after 25 s"); return; }
  if (P.dts.deferUntil) dts_defer_tick();
  if (vs == VID_ERROR && !P.failed && P.started && P.isP2p && !P.p2pRetried && p2p_moved_recently()) { /* the torrent's storage moved (a USB drive pulled out): the same stream once more, same moment */
    P.p2pRetried = 1; P.startAt = P.lastPos > 0 ? P.lastPos : 0; P.didResume = 0; P.live = 0; fprintf(stderr, "[player] stream error after the torrent storage moved: again at %.0f s\n", P.startAt); video_load(P.url); vs = video_state(); }
  if (vs == VID_ERROR && !P.failed && P.started && live_reload("stream error")) vs = video_state();
  if (vs == VID_ERROR && !P.failed && P.started) { if (P.alt[0] && !P.altTried) { P.altTried = 1; fprintf(stderr, "[player] error (%s), the fallback URL\n", video_error()); video_load(P.alt); } else { if (!P.errAt) P.errAt = now_(); if (!P.sizeTok || P.sizeAns || now_() - P.errAt > 4) fail(fail_reason()); } } /* the first URL failed: the same file the other way; the reason waits for the server's answer (4 s at most) */
  /* a channel that loaded and shows no picture 12 s on (it answers, then nothing: a dead origin behind a CDN, a key
     refused, a playlist that never moves): its next stream, as an error would; the preview gives up the same way */
  if (live_title() && !P.isTrailer && P.started && !P.live && !P.failed && P.vidAt && !video_live() && now_() - P.vidAt > 12) { P.vidAt = 0; fail(T("This channel isn't broadcasting right now")); }
  if (P.nx.tok && !P.nx.done && now_() >= P.nx.until) nx_finish();
  if (P.metaRetryAt && now_() >= P.metaRetryAt) { P.metaRetryAt = 0; if (!P.next.has || !P.videos) ask_meta(); }
  if (P.extRetryAt && now_() >= P.extRetryAt) { P.extRetryAt = 0; load_ext_subs(P.extTry + 1); }
  if (P.adxRetryAt && now_() >= P.adxRetryAt) { P.adxRetryAt = 0; load_addon_subs(P.adxTry + 1); }
  if (P.embWaitAt && !P.embWaitOver && now_() >= P.embWaitAt) { P.embWaitOver = 1; auto_sub(); }
  if (P.sameWaitAt && !P.sameWaitOver && now_() >= P.sameWaitAt) { P.sameWaitOver = 1; auto_sub(); }
  sub_check(); ass_sync();
  sk_tick(); loc_apply();
  if (P.started && !P.ldd && dur() > 0) P.ldd = 1; /* the length is known: the loading line fades before the picture comes */
  /* the art stays until the picture really runs: the resume jump is made behind it, and it fades only once playing has
     been steady for a moment (never a frozen first frame, then a spinner) */
  if (P.liveT0 && !P.autoAudDone && now_() - P.liveT0 < 15) { static double aAt; if (now_() - aAt > 0.5) { aAt = now_(); auto_audio(); } } /* (the player may list the file's tracks a moment after the first picture: asked again until it has, 15 s at most) */
  if (!P.live && P.started && video_live()) { /* (P.started: this title's own stream asked for, as start_video says; before it, a picture still running is another's: Home's trailer while the best stream is chosen) */
    if (!P.liveT0) { P.liveT0 = now_(); P.steadyAt = 0; apply_aspect(); try_resume(); auto_audio(); load_emb_tracks(); }
    int steady = (video_state() == VID_PLAYING || paused()) && !video_settling(); if (!steady) P.steadyAt = 0; else if (!P.steadyAt) P.steadyAt = now_();
    if ((P.steadyAt && now_() - P.steadyAt > (P.quiet ? 0.06 : 0.35)) || now_() - P.liveT0 > 15) { /* (a channel switched to: up at its first frames) */ P.live = 1; pg_ask(); if (live_title()) { live_reach_set(P.id, 1); live_found_keep(P.id, &P.st); } /* (a channel: it really plays; its stream kept as the one that does) */ fprintf(stderr, "[player] picture running after %.1f s\n", now_() - P.liveT0); if (resumeMsg[0]) { toast(resumeMsg, 2500, NULL); resumeMsg[0] = 0; }
      { static int tr; const char *e = getenv("SB_PLAY_TRAY"); if (e && !tr) { tr = 1; open_tray(!strcmp(e, "src") ? 2 : !strcmp(e, "eps") ? 1 : !strcmp(e, "aud") ? 3 : 4); } } } } /* (development: SB_PLAY_TRAY=src|eps|aud|subs opens that tray once the picture runs, for pictures of it) */
  if (!video_live()) P.liveT0 = 0;
  if (P.live && !P.skipsAsked && dur() > 0) load_skips(); /* once the length is known: segments are checked against it */
  if (P.live) {
    if (video_width() != P.lastVw || video_height() != P.lastVh) apply_aspect(); /* the picture's size became known or changed */
    if (P.aspRefreshAt && now_() >= P.aspRefreshAt) { P.aspRefreshAt = 0; if (paused()) video_seek(video_time()); }
    if (paused() && P.prompt.autoN && (P.prompt.k == 1 || P.prompt.k == 2)) { P.prompt.autoN = 0; paint_prompt(); } /* paused: no skipping by itself */
    /* the screensaver: paused and untouched for the time chosen in Settings, the controls fade away and the title's cover takes the screen */
    if (P.ssOn && !paused()) ss_exit();
    /* (the player has no screensaver of its own any more: the app's one comes over a paused film) */
    if (paused()) { if (!P.pzSince) P.pzSince = now_(); } else P.pzSince = 0; /* (how long it has been paused, any way) */
    int pz = paused() && P.mode != 2 && !P.pw.on; if (pz && !P.pausedShown) { if (!P.pausedAt) P.pausedAt = now_(); if (now_() - P.pausedAt > 0.35) { P.pausedShown = 1; show_osd(1); } }
    else if (!pz) { P.pausedAt = 0; P.pausedShown = 0; }
    if (P.seekT >= 0 && now_() >= P.seekCommitAt) commit_seek();
    if (P.seekSnapAt) { static int stalled; double tn = now_(); /* (SB_SEEKSNAP: a committed seek's time to playing again, its picture there) */
      if (vs != VID_PLAYING) stalled = 1;
      else if ((stalled || tn - P.seekSnapAt > 1.5) && fabs(video_time() - P.seekSnapT) < 5) { fprintf(stderr, "[seek] playing again %.0f ms after the seek to %.2f s (%s, at %.2f s)\n", (tn - P.seekSnapAt) * 1000, P.seekSnapT, stalled ? "after loading" : "no stall seen", video_time()); P.seekSnapAt = 0; stalled = 0; }
      else if (tn - P.seekSnapAt > 20) { P.seekSnapAt = 0; stalled = 0; } }
    if (P.seekTxtUntil && now_() >= P.seekTxtUntil && !stripHold) { P.seekTxtUntil = 0; P.skOn = 0; P.skm = 0; } /* (the previews up: the bar stays with them, and goes with them) */
    if (P.mode == 1 && now_() >= P.hideAt && !paused() && P.seekT < 0) hide_osd();
    { double nw = now_(); if (vs == VID_PLAYING && !video_settling() && P.playTick > 0 && nw - P.playTick < 1) P.played += nw - P.playTick; P.playTick = nw; ep_back_check(); } /* (seconds really watched: an earlier episode's place) */
    if (vs == VID_PLAYING && at_credits() && !P.marked) { mark_watched(); save_progress(1); } /* the credits: watched, everywhere, at once */
    if (vs == VID_PLAYING && now_() - P.lastSave >= 10) save_progress(0);
    if (vs == VID_ENDED && !P.ended && live_reload("the stream ended")) vs = video_state();
    if (vs == VID_ENDED && !P.ended) { P.ended = 1; scrobble_player_end(video_time(), dur(), 1); tracking_player_end(video_time(), dur()); save_progress(1); mark_watched(); if (strcmp(store_get("bingeWatch", "true"), "false") && strcmp(store_get("enableNextVideo", "true"), "false") && P.next.has) P.endGoAt = now_() + 0.6; else close_player(0, 0); }
    if (P.endGoAt && now_() >= P.endGoAt && !LX.ask) { P.endGoAt = 0; if (!lx_gate()) go_next(); if (!P.open || P.closing) return; }
    check_prompt();
    if (P.prompt.autoN && (P.prompt.k == 1 || P.prompt.k == 2) && now_() - P.prompt.t0 >= P.prompt.adur && !paused()) prompt_go(0);
    else if (P.prompt.autoN && P.prompt.k == 3 && now_() - P.prompt.t0 >= 10) { P.prompt.autoN = 0; if (!lx_gate()) next_auto(); }
    if (!P.live) return; /* a new episode is being found */
    const char *r = res_of_video(video_width(), video_height()); if (*r && !P.isTrailer) P.res = r;
    info_update();
  }
  foc_update();
}

#include "screens/player_draw.inc" /* etapa-89: drawing: the art, subtitles, the controls, the trays */
/* ---- the pointer in the player: moving brings the controls; pointing at a button, the bar, a prompt or a tray
   card highlights it; a click acts there as OK does: on the bar it seeks to that spot on release (a bubble shows the
   time under the pointer), on the picture it plays / pauses, beside an open tray it puts the tray away (on another
   button: that one acts) ---- */
static int in_box(float x, float y, float bx, float by, float bw, float bh) { return x >= bx && x <= bx + bw && y >= by && y <= by + bh; }
static void ptr_hv(int on, float x) {
  double d = dur(); on = on && d > 0; if (on) { float tx = 72 + 132 + 20, tw = SB_W - 2 * 72 - 2 * (132 + 20), p = (x - tx) / tw; P.hvP = p < 0 ? 0 : p > 1 ? 1 : p; }
  P.hvOn = on;
}
static void ptr_tray(float x, float y, int down) {
  for (int i = 0; i < PG.ntab && i < P.tray.ntabs; i++) if (in_box(x, y, PG.tabX[i], PG.tabY, PG.tabW[i], PG.tabH) && x >= PG.x && x <= PG.x + PG.w) {
    if (down && i != P.tray.tab) { tray_tab(i); P.tray.zone = 1; }
    return; }
  if (PG.src && src_tray()) { if (y < PG.srcTop || y > PG.srcTop + SRC_VH || x < PG.srcX || x > PG.srcX + SRC_RW) return; float ly = y - PG.srcTop + SR.y.v;
    for (int i = 0; i < SR.n; i++) if (ly >= SR.r[i].y && ly <= SR.r[i].y + SR.r[i].h) { if (!src_focusable(i)) return; if (SR.idx != i || P.tray.onBtn || P.tray.zone != 0) { SR.idx = i; SR.moved = 1; P.tray.onBtn = 0; P.tray.zone = 0; } if (down) src_choose(i); return; }
    return; }
  if (P.tray.ncols) {
    for (int c = 0; c < P.tray.ncols; c++) if (x >= PG.colX[c] && x <= PG.colX[c] + PG.colW) {
      if (y < PG.colTop || y > PG.colBot) return;
      for (int i = 0; i < PG.nit[c] && i < P.tray.cols[c].n; i++) if (y >= PG.itY[c][i] && y <= PG.itY[c][i] + PG.itH[c][i]) {
        TItem *it = &P.tray.cols[c].items[i]; if (!focusable(it)) return;
        if (P.tray.c != c || P.tray.r[c] != i || P.tray.onBtn) { P.tray.c = c; P.tray.r[c] = i; P.tray.onBtn = 0; P.tray.zone = 2; }
        if (down) choose(it); return; }
      return; }
    return; }
  if (y < PG.rowY || y > PG.rowY + PG.rowH || x < PG.x || x > PG.x + PG.w) return;
  for (int i = 0; i < PG.nrow && i < P.tray.n; i++) if (x >= PG.rowX[i] && x <= PG.rowX[i] + PG.rowW[i]) {
    TItem *it = &P.tray.row[i]; if (!focusable(it)) return;
    if (P.tray.idx != i || P.tray.zone != 0 || P.tray.onBtn) { P.tray.idx = i; P.tray.zone = 0; P.tray.onBtn = 0; }
    if (down) choose(it); return; }
}
void player_ptr(int type, float x, float y) {
  if (!P.open || P.closing || !P.live) return; /* the art is up (resolving): nothing to point at */
  float L25 = text_line_height(25), barTop = SB_H - 54 - 94 - 30 - L25; /* the bar's row, its times included (the track alone is a few pixels high) */
  int onBar = y >= barTop && y <= barTop + L25, onBtns = y >= SB_H - 54 - 94 && y <= SB_H - 54;
  if (type == 2) { if (P.barDown && onBar) { ptr_hv(1, x); double d = dur(); if (d > 0) { P.seekT = P.hvP * d; P.seekScrub = 1; commit_seek(); } } P.barDown = 0; return; } /* the bar seeks on release, where the pointer is then */
  int down = type == 1;
  if (P.mode == 5 && P.styK) return; /* (the style card: the remote's keys) */
  if (P.mode == 5) { if (down && PZ.h > 0 && y >= PZ.y && y <= PZ.y + PZ.h) { if (x >= PZ.ux && x <= PZ.ux + PZ.uw) pos_change(y < PZ.y + PZ.h / 2 ? 1 : -1); else if (x >= PZ.ox && x <= PZ.ox + PZ.ow) pos_close(); } return; } /* the position card: its keys and Done */
  if (P.mode == 4) { /* the delay card: its buttons; elsewhere the picture stays as it is */
    if (DG.h > 0 && y >= DG.y && y <= DG.y + DG.h) for (int i = 0; i < 4; i++) if (x >= DG.x[i] && x <= DG.x[i] + DG.w[i]) { if (P.dlyF != i) { P.dlyF = i; mo_to(&P.dlyR, (float)i, .18f, C_OUT, 0); } if (down) dly_act(i); return; }
    return; }
  if (P.mode == 2) {
    if (PG.tray && in_box(x, y, PG.x, PG.y, PG.w, PG.h)) { ptr_tray(x, y, down); return; }
    if (down) { int ak = P.tray.kind < 0 ? -P.tray.kind : P.tray.kind, ob = btn_index(ak == 1 || ak == 5 ? "eps" : ak == 6 ? "chn" : ak == 2 ? "src" : ak == 3 ? "aud" : "subs"); close_tray();
      if (onBtns) for (int i = 0; i < P.nbtn; i++) if (i != ob && x >= P.btns[i].x && x <= P.btns[i].x + P.btns[i].w) { P.row = 0; P.bIdx = i; act(P.btns[i].id); break; } }
    return; }
  if (P.prompt.k && PG.prompt) { /* skip intro / recap, the next-up card (all of it) */
    int pi = -1; if (PG.card) { if (in_box(x, y, PG.prX[0], PG.prY[0], PG.prW[0], PG.prH[0])) pi = P.prompt.nbtn - 1; }
    else for (int i = 0; i < PG.npr && i < P.prompt.nbtn; i++) if (in_box(x, y, PG.prX[i], PG.prY[i], PG.prW[i], PG.prH[i])) pi = i;
    if (pi >= 0) { if (P.mode != 3 || P.prompt.idx != pi) { if (P.mode == 0) show_osd(1); P.mode = 3; P.prompt.idx = pi; } ptr_hv(0, x); if (down) prompt_go(pi); return; } }
  if (P.mode == 0 || P.mode == 3) { if (P.mode == 3) P.mode = 0; show_osd(1); } else arm_hide(); /* moving brings the controls */
  if (onBtns) for (int i = 0; i < P.nbtn; i++) if (x >= P.btns[i].x && x <= P.btns[i].x + P.btns[i].w) { if (P.row != 0 || P.bIdx != i) { P.row = 0; P.bIdx = i; } ptr_hv(0, x); if (down) act(P.btns[i].id); return; }
  if (onBar) { if (P.row != 1) P.row = 1; ptr_hv(1, x); if (down) P.barDown = 1; return; } /* the bar, its times included */
  ptr_hv(0, x);
  if (down) { if (P.seekT >= 0) commit_ok(); else toggle(); show_osd(1); } /* the picture: play / pause */
}

/* Stats for nerds: what plays and how, a few short lines (n of them filled) */
int player_nerd_lines(char (*out)[120], int max) {
  if (!P.open || max < 1) return 0; int n = 0; double t = video_time(), d = dur(), buf = video_buffered_end() - t;
  const char *st = P.closing ? "closing" : !P.started ? "starting" : paused() ? "paused" : video_state() == VID_PLAYING ? "playing" : "waiting";
  snprintf(out[n++], 120, "player  %s  %d:%02d / %d:%02d", st, (int)t / 60, (int)t % 60, (int)d / 60, (int)d % 60);
  if (n < max) { char hdr[32] = "", fp[24] = ""; video_hdr(hdr, sizeof hdr); double fps = video_fps(); if (fps > 0) snprintf(fp, sizeof fp, " @ %.3g fps", fps);
    snprintf(out[n++], 120, "video  %dx%d%s%s%s", video_width(), video_height(), fp, hdr[0] && strcmp(hdr, "none") ? " · " : "", hdr[0] && strcmp(hdr, "none") ? hdr : ""); }
  if (n < max) { VidTrack a[16]; int na = aud_list(a, 16); const VidTrack *c = P.curAud >= 0 && P.curAud < na ? &a[P.curAud] : NULL; char ch[16] = "";
    if (c && c->channels > 0) snprintf(ch, sizeof ch, " · %d ch", c->channels);
    MkvAudio ma[16]; int m = P.dts.conv && mkv ? mkv_audio_tracks(mkv, ma, 16) : 0; const MkvAudio *o = NULL; for (int i = 0; i < m; i++) if (ma[i].n == P.dts.convOrd) o = &ma[i]; if (!o && P.dts.convOrd >= 0 && P.dts.convOrd < m) o = &ma[P.dts.convOrd];
    if (P.dts.conv && o) { /* converted here: the file's own sound, then what the TV gets ("DTS 7.1 → AC3 5.1") */
      const char *k = o->codec, *nm = !strncmp(k, "A_DTS", 5) ? "DTS" : !strncmp(k, "A_TRUEHD", 8) ? "TrueHD" : !strncmp(k, "A_EAC3", 6) ? "E-AC3" : !strncmp(k, "A_AC3", 5) ? "AC3" : !strncmp(k, "A_AAC", 5) ? "AAC" : !strncmp(k, "A_OPUS", 6) ? "Opus" : !strncmp(k, "A_FLAC", 6) ? "FLAC" : k + (strncmp(k, "A_", 2) ? 0 : 2);
      char oc[8] = ""; if (o->channels >= 3) snprintf(oc, sizeof oc, " %d.1", o->channels - 1); else if (o->channels > 0) snprintf(oc, sizeof oc, " %d.0", o->channels);
      snprintf(out[n++], 120, "audio  %s%s \xe2\x86\x92 %s", nm, oc, P.dts.outDesc[0] ? P.dts.outDesc : "AC3"); }
    else snprintf(out[n++], 120, "audio  %s%s", c && c->codec[0] ? c->codec : "not reported yet", ch); }
  if (n < max) snprintf(out[n++], 120, "buffer  %.0f s ahead  ·  %s", buf > 0 ? buf : 0, P.isP2p ? "torrent" : P.isHls ? "HLS" : "direct");
  if (n < max && P.isP2p) { char pl[96]; if (p2p_peer_line(pl, sizeof pl)) snprintf(out[n++], 120, "p2p  %s", pl); }
  if (n < max) { double am = asssubs_render_ms(); if (asssubs_on()) snprintf(out[n++], 120, "subtitles  styled, %.0f ms a render", am); else if (P.curSub[0]) snprintf(out[n++], 120, "subtitles  plain"); }
  return n;
}

/* switching channels: a pill in the top corner hugging the channel's logo in its round end. While something is said
   (tuning in, nothing on, its place in the line-up, Back again to leave) it is open with the name and the words; a new
   channel's logo crossfades in its circle as the words change; tuned, it pops once, closes to the logo alone and fades */
static void lc_logo(const char *u, float cx, float cy, float s, float a) {
  if (a <= 0.003f || !u || !*u) return; float asp = 0; GLuint t = img_get(u, 128, 128, &asp); if (!t || asp <= 0) return;
  float lw = asp >= 1 ? s : s * asp, lh = asp >= 1 ? s / asp : s; gfx_image(t, cx - lw / 2, cy - lh / 2, lw, lh, 0, WHITE_A((int)(255 * a)), 0, 0, 1, 1); }
static char shN[256], shL[120]; static int shT, shF; static float ta; /* the pill's words as shown (they change only once faded out) */
static void lc_reset(void) { shN[0] = shL[0] = 0; ta = 0; }
static void live_card_draw(void) {
  const LiveCard *c = live_card(); static float a; static Tween w; static char cur[512], prev[512], curId[SB_ID]; static double swapAt, popAt; static int wasTuning;
  float want = c->on && P.open && !P.closing && P.artA.v < 0.5f && growAt <= 0 && shrinkAt <= 0 ? 1 : 0; /* (and not while a preview grows: once it fills the screen) */ a += (want - a) * fminf(1, app_dt() * 8); /* (over the art: it says it there) */
  if (a < 0.01f || !c->it) { if (a < 0.01f) { cur[0] = prev[0] = curId[0] = 0; tween_set(&w, 0); wasTuning = 0; lc_reset(); } return; }
  const Item *it = c->it; double now = now_(); int rtl = app_rtl(); char ln[120];
  const char *src = it->logo[0] ? it->logo : it->poster[0] ? it->poster : it->background;
  if (strcmp(curId, it->id)) { snprintf(prev, sizeof prev, "%s", cur); snprintf(cur, sizeof cur, "%s", src); snprintf(curId, sizeof curId, "%s", it->id); swapAt = now; }
  if (wasTuning && !c->tuning && !c->failed) popAt = now; wasTuning = c->tuning;
  if (c->tuning) snprintf(ln, sizeof ln, "%s", T("Tuning in…")); else if (c->failed) snprintf(ln, sizeof ln, "%s", T("This channel isn't broadcasting right now"));
  else if (c->backHint) snprintf(ln, sizeof ln, "%s", T("Back again to leave"));
  else { int k = live_lineup_cur(), n = live_lineup_n(); if (k >= 0 && n > 1) { char a1[12], b1[12]; snprintf(a1, sizeof a1, "%d", k + 1); snprintf(b1, sizeof b1, "%d", n); snprintf(ln, sizeof ln, "%s", T2("{n} of {total}", "n", a1, "total", b1)); } else ln[0] = 0; }
  const float H = 88, R = H / 2, D = 66; int open = c->tuning || c->failed || c->backHint || now - c->at < 1.6;
  /* the words shown: when they change, the old ones fade out, the pill takes the new ones' size, then they fade in
     (nothing is ever cut by its edge) */
  float dt = app_dt();
  int same = !strcmp(shN, it->name) && !strcmp(shL, ln) && shT == c->tuning && shF == c->failed;
  if (!same && (ta <= 0.01f || !shN[0])) { snprintf(shN, sizeof shN, "%s", it->name); snprintf(shL, sizeof shL, "%s", ln); shT = c->tuning; shF = c->failed; ta = 0; same = 1; }
  float nw = text_width(shN, 28, 800, -0.3f), lw = text_width(shL, 21, 600, 0), lead = shT ? 30 : shF ? 20 : 0, mw = fmaxf(nw, lw + lead); int capd = 0; if (mw > SB_W - 64 * 2 - 140) { mw = SB_W - 64 * 2 - 140; capd = 1; } /* (cut only at the widest pill: else it is the words' own size) */
  float full = (H - D) / 2 + D + 18 + mw + 30, tgt = open || ta > 0.05f ? full : H;
  if (w.v <= 0) tween_set(&w, tgt); else if (fabsf(w.to - tgt) > 0.5f) tween_to(&w, tgt, 0.36f, EASE_CSS_EASEOUT); tween_tick(&w, dt);
  if (!same || !open) ta -= dt * 7; else if (fabsf(w.v - full) < 2) ta += dt * 5; ta = ta < 0 ? 0 : ta > 1 ? 1 : ta;
  float W = w.v, x = rtl ? SB_W - 64 - W : 64, y = 52 - 10 * (1 - a), cx = rtl ? x + W - R : x + R, cy = y + R;
  float pk = now - popAt < 0.4 ? sinf((float)((now - popAt) / 0.4) * 3.14159f) : 0, sc = (0.94f + 0.06f * a) * (1 + 0.05f * pk);
  gfx_opacity_push(a); gfx_scale_push(sc, sc, cx, cy);
  lglass_shell(x, y, W, H, R, RGBA(14, 12, 22, 214), 1);
  gfx_circle(cx, cy, D / 2, WHITE_A(16));
  float xf = fminf(1, (float)((now - swapAt) / 0.4)); xf = xf * xf * (3 - 2 * xf); /* the logos crossfade */
  lc_logo(prev, cx, cy, D * 0.74f, 1 - xf); lc_logo(cur, cx, cy, D * 0.74f, xf);
  if (ta > 0.01f) { gfx_opacity_push(ta); int al = rtl ? ALIGN_RIGHT : ALIGN_LEFT;
    float lx = rtl ? cx - D / 2 - 18 : cx + D / 2 + 18, ty = shL[0] ? cy - (text_line_height(28) + 2 + text_line_height(21)) / 2 : cy - text_line_height(28) / 2;
    text_draw(shN, lx, ty, 28, 800, WHITE_A(255), -0.3f, capd ? mw : 0, al);
    float ly = ty + text_line_height(28) + 2, sx = lx;
    if (shT) { float qx = rtl ? lx - 10 : lx + 10, qy = ly + text_line_height(21) / 2; float ph = clock_ph(0.9) * 6.2832f; for (int q = 0; q <= 8; q++) { float an = ph - 2.356f + 1.5708f * q / 8; gfx_circle(qx + cosf(an) * 8, qy + sinf(an) * 8, 1.5f, ACC2); } sx = rtl ? lx - 30 : lx + 30; }
    if (shF) { float qx = rtl ? lx - 6 : lx + 6; gfx_circle(qx, ly + text_line_height(21) / 2, 5, RGB(0xef, 0x3b, 0x3b)); sx = rtl ? lx - 20 : lx + 20; }
    if (shL[0]) text_draw(shL, sx, ly, 21, 600, WHITE_A(shF ? 220 : 190), 0, capd ? mw - lead : 0, al);
    gfx_opacity_pop(); }
  gfx_scale_pop(); gfx_opacity_pop();
}
/* the fonts' card: the font in use by its name, written in it; five around it as samples (Aa, or an Arabic word), the
   one in use lit; Left / Right through them */
static void font_card_draw(float a) {
  int ar = P.styK == 6, n = SUBF_N, cur = style_idx(); const float h = 120, VW = 330, B = 76, CW = 104; static TMemo dW; const char *dn = T("Done"); float dw = tm_width(&dW, dn, 26, 700, 0) + 64;
  int lo = cur - 2; if (lo < 0) lo = 0; if (lo > n - 5) lo = n - 5; float tot = 5 * CW + 4 * 12;
  float w = 28 + VW + 24 + tot + 28 + 2 + 28 + dw + 28, x = SB_W / 2 - w / 2, y = 44 - 24 * (1 - a), cy = y + h / 2;
  gfx_opacity_push(a); gfx_offset_push(nudge_x(&P.nudge), nudge_y(&P.nudge));
  gfx_shadow(x, y + 14, w, h, 30, 40, BLACK_A(110)); gfx_rrect(x, y, w, h, 30, RGBA(18, 16, 28, 222)); gfx_rrect_v(x, y, w, h, 30, WHITE_A(18), WHITE_A(4)); gfx_ring(x, y, w, h, 30, 1, WHITE_A(36));
  { const char *nm = cur ? (ar ? SUBF_AR[cur] : SUBF_LAT[cur]) : T("Default"); float e = (float)((now_() - P.posAt) / 0.22); e = e < 0 ? 0 : e > 1 ? 1 : e; float lift = 6 * (1 - ease_apply(EASE_OUT, e)), vx = x + 28;
    char nb[96]; snprintf(nb, sizeof nb, ar ? "%s  \xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd8\xa7" : "%s", nm); text_draw(nb, vx + VW / 2, cy - 30 - lift, 32, text_sub_weight(600, ar ? 0 : cur, ar ? cur : 0), WHITE_A(255), 0, VW, ALIGN_CENTER); text_draw(T(ar ? "Arabic subtitle font" : "Subtitle font"), vx + VW / 2, cy + 18, 20, 600, WHITE_A(160), 0.3f, VW, ALIGN_CENTER); }
  float ox = x + 28 + VW + 24; const char *smp = ar ? "\xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd8\xa7" : "Aa"; /* (the Arabic sample: the same word everywhere, مرحبا) */
  for (int k = 0; k < 5; k++) { int i = lo + k, on = i == cur; float bh = 58, by = cy - bh / 2; int fw = text_sub_weight(600, ar ? 0 : i, ar ? i : 0);
    gfx_rrect_v(ox, by, CW, bh, 14, RGB(46, 48, 60), RGB(14, 14, 20)); text_draw(smp, ox + CW / 2, cy - text_line_height(28) / 2, 28, fw, WHITE_A(240), 0, CW - 12, ALIGN_CENTER);
    if (on) gfx_ring_out(ox, by, CW, bh, 14, 3, ACC2); else gfx_ring(ox, by, CW, bh, 14, 1, WHITE_A(40)); ox += CW + 12; }
  float sep = ox - 12 + 28; gfx_rect(sep, cy - 30, 1.5f, 60, WHITE_A(40));
  float dx = sep + 2 + 28; lglass_accent(dx, cy - B / 2, dw, B, B / 2, 1); text_draw(dn, dx + dw / 2, cy - text_line_height(26) / 2.0f, 26, 700, ACC_FG, 0, 0, ALIGN_CENTER);
  { float hy = y + h + 22; key_hint(SB_W / 2 - 60, hy, K_LEFT, NULL, WHITE_A(150), 0); key_hint(SB_W / 2 + 22, hy, K_RIGHT, NULL, WHITE_A(150), 0); }
  gfx_offset_pop(); gfx_opacity_pop(); }
void player_dev_fontcard(int k) { P.styK = k; P.posAt = 0; font_card_draw(1); } /* (development: SB_FONTCARD=5|6 draws the fonts' card over any screen) */
static void style_draw(float a) { /* [ value · what it is ]  [ the choices, the one in use lit ]  |  [ Done ] */
  if (P.styK >= 5) { font_card_draw(a); return; }
  static const char *const CN[3] = { "White", "Yellow", "Cyan" }, *const BN[3] = { "None", "Solid", "Transparent" }, *const TT[5] = { "", "Subtitle size", "Subtitle colour", "Subtitle background", "Subtitle brightness" };
  int n = style_n(), cur = style_idx(); const float h = 120, VW = 250, B = 76; static TMemo dW; const char *dn = T("Done"); float dw = tm_width(&dW, dn, 26, 700, 0) + 64;
  float ow[7], tot = 0; for (int i = 0; i < n; i++) { if (P.styK == 1) { char v[8]; snprintf(v, sizeof v, "%d%%", SUB_SIZES[i]); ow[i] = text_width(v, 22, 700, 0) + 34; } else ow[i] = P.styK == 2 ? 56 : 96; tot += ow[i] + (i ? 12 : 0); }
  float w = 28 + VW + 24 + tot + 28 + 2 + 28 + dw + 28, x = SB_W / 2 - w / 2, y = 44 - 24 * (1 - a), cy = y + h / 2;
  gfx_opacity_push(a); gfx_offset_push(nudge_x(&P.nudge), nudge_y(&P.nudge));
  gfx_shadow(x, y + 14, w, h, 30, 40, BLACK_A(110)); gfx_rrect(x, y, w, h, 30, RGBA(18, 16, 28, 222)); gfx_rrect_v(x, y, w, h, 30, WHITE_A(18), WHITE_A(4)); gfx_ring(x, y, w, h, 30, 1, WHITE_A(36));
  { char v[32]; if (P.styK == 1) snprintf(v, sizeof v, "%d %%", SUB_SIZES[cur]); else if (P.styK == 4) snprintf(v, sizeof v, "%s %%", SUB_BRIGHT[cur]); else snprintf(v, sizeof v, "%s", T(P.styK == 2 ? CN[cur] : BN[cur]));
    float e = (float)((now_() - P.posAt) / 0.22); e = e < 0 ? 0 : e > 1 ? 1 : e; float lift = 6 * (1 - ease_apply(EASE_OUT, e)), vx = x + 28;
    text_draw(v, vx + VW / 2, cy - 30 - lift, 36, 800, WHITE_A(255), -0.4f, VW, ALIGN_CENTER); text_draw(T(TT[P.styK]), vx + VW / 2, cy + 18, 20, 600, WHITE_A(160), 0.3f, VW, ALIGN_CENTER); }
  float ox = x + 28 + VW + 24; const SubStyle *st = sub_style();
  for (int i = 0; i < n; i++) { int on = i == cur; float bw = ow[i], bh = P.styK == 2 ? 56 : 58, by = cy - bh / 2;
    if (P.styK == 1) { char v[8]; snprintf(v, sizeof v, "%d%%", SUB_SIZES[i]); if (on) { lglass_accent(ox, by, bw, bh, bh / 2, 1); } else gfx_rrect(ox, by, bw, bh, bh / 2, WHITE_A(22));
      text_draw(v, ox + bw / 2, cy - text_line_height(22) / 2, 22, 700, on ? ACC_FG : WHITE_A(225), 0, 0, ALIGN_CENTER); }
    else if (P.styK == 2) { float r = 22, ccx = ox + bw / 2; if (on) gfx_ring_out(ccx - r - 5, cy - r - 5, 2 * r + 10, 2 * r + 10, r + 5, 3, ACC2); gfx_circle(ccx, cy, r, hexcol(SUB_COLORS[i])); gfx_ring(ccx - r, cy - r, 2 * r, 2 * r, r, 1.5f, BLACK_A(70));
      if (on) icon_draw("check", ccx - 11, cy - 11, 22, RGB(20, 18, 28)); }
    else if (P.styK == 4) { float k = atoi(SUB_BRIGHT[i]) / 100.0f; uint32_t base = hexcol(store_get("subsColor", "#ffffff")), bol = hexcol(store_get("subsOutlineColor", "#000000")); gfx_rrect_v(ox, by, bw, bh, 14, RGB(46, 48, 60), RGB(14, 14, 20)); /* a dark scene behind, where the glare shows */
      for (int dd = 0; dd < 8; dd++) { static const float qx[8] = { -2, 2, 0, 0, -1, 1, -1, 1 }, qy[8] = { 0, 0, -2, 2, -1, 1, 1, -1 }; text_draw("Aa", ox + bw / 2 + qx[dd], cy - text_line_height(26) / 2 + qy[dd], 26, 600, col_dim(bol, k), 0, 0, ALIGN_CENTER); }
      text_draw("Aa", ox + bw / 2, cy - text_line_height(26) / 2, 26, 600, col_dim(base, k), 0, 0, ALIGN_CENTER);
      if (on) gfx_ring_out(ox, by, bw, bh, 14, 3, ACC2); else gfx_ring(ox, by, bw, bh, 14, 1, WHITE_A(40)); }
    else { float bo = (float)atof(SUB_BGS[i]); gfx_rrect_v(ox, by, bw, bh, 14, RGB(120, 128, 150), RGB(60, 64, 82)); /* a scene behind, so the background shows as it would */
      float tw = text_width("Aa", 26, 600, 0); if (bo > 0) gfx_rrect(ox + bw / 2 - tw / 2 - 10, cy - 18, tw + 20, 36, 7, BLACK_A((int)(bo * 255)));
      for (int dd = 0; dd < 8; dd++) { static const float qx[8] = { -2, 2, 0, 0, -1, 1, -1, 1 }, qy[8] = { 0, 0, -2, 2, -1, 1, 1, -1 }; text_draw("Aa", ox + bw / 2 + qx[dd], cy - text_line_height(26) / 2 + qy[dd], 26, 600, st->ol, 0, 0, ALIGN_CENTER); }
      text_draw("Aa", ox + bw / 2, cy - text_line_height(26) / 2, 26, 600, st->col, 0, 0, ALIGN_CENTER);
      if (on) gfx_ring_out(ox, by, bw, bh, 14, 3, ACC2); else gfx_ring(ox, by, bw, bh, 14, 1, WHITE_A(40)); }
    ox += bw + 12; }
  float sep = ox - 12 + 28; gfx_rect(sep, cy - 30, 1.5f, 60, WHITE_A(40));
  float dx = sep + 2 + 28; lglass_accent(dx, cy - B / 2, dw, B, B / 2, 1); text_draw(dn, dx + dw / 2, cy - text_line_height(26) / 2.0f, 26, 700, ACC_FG, 0, 0, ALIGN_CENTER);
  { float hy = y + h + 22; key_hint(SB_W / 2 - 60, hy, K_LEFT, NULL, WHITE_A(150), 0); key_hint(SB_W / 2 + 22, hy, K_RIGHT, NULL, WHITE_A(150), 0); } /* (the keys that change it) */
  gfx_offset_pop(); gfx_opacity_pop();
}
static void pos_draw(void) {
  float a = P.posA.v; if (a <= 0.004f) { PZ.h = 0; return; }
  if (P.styK) { PZ.h = 0; style_draw(a); return; }
  /* [ 12 % · Subtitle position ]  [Up Higher / Down Lower]  |  [ Done ]: the height large, the keys beside it */
  const float h = 120, VW = 250; static TMemo dW, hW1, hW2; const char *dn = T("Done"), *hu = T("Higher"), *hd = T("Lower");
  float dw = tm_width(&dW, dn, 26, 700, 0) + 64, hw = 38 + 10 + fmaxf(tm_width(&hW1, hu, 22, 600, 0), tm_width(&hW2, hd, 22, 600, 0));
  float w = 28 + VW + 24 + hw + 28 + 2 + 28 + dw + 28, x = SB_W / 2 - w / 2, y = 44 - 24 * (1 - a), cy = y + h / 2;
  gfx_opacity_push(a); gfx_offset_push(nudge_x(&P.nudge), nudge_y(&P.nudge));
  gfx_shadow(x, y + 14, w, h, 30, 40, BLACK_A(110)); gfx_rrect(x, y, w, h, 30, RGBA(18, 16, 28, 222)); gfx_rrect_v(x, y, w, h, 30, WHITE_A(18), WHITE_A(4)); gfx_ring(x, y, w, h, 30, 1, WHITE_A(36));
  { char v[16]; snprintf(v, sizeof v, "%d %%", pos_pct()); float vx = x + 28, e = (float)((now_() - P.posAt) / 0.22); e = e < 0 ? 0 : e > 1 ? 1 : e; float lift = 6 * (1 - ease_apply(EASE_OUT, e));
    text_draw(v, vx + VW / 2, cy - 30 - lift, 40, 800, WHITE_A(255), -0.4f, 0, ALIGN_CENTER);
    text_draw(T("Subtitle position"), vx + VW / 2, cy + 18, 20, 600, WHITE_A(160), 0.3f, VW, ALIGN_CENTER); }
  float hx = x + 28 + VW + 24; key_hint(hx, cy - 22, K_UP, hu, WHITE_A(pos_pct() < POS_MAX ? 235 : 110), 0); key_hint(hx, cy + 22, K_DOWN, hd, WHITE_A(pos_pct() > POS_MIN ? 235 : 110), 0);
  float sep = hx + hw + 28; gfx_rect(sep, cy - 30, 1.5f, 60, WHITE_A(40));
  float ox = sep + 2 + 28, B = 76; lglass_accent(ox, cy - B / 2, dw, B, B / 2, 1); text_draw(dn, ox + dw / 2, cy - text_line_height(26) / 2.0f, 26, 700, ACC_FG, 0, 0, ALIGN_CENTER);
  PZ.y = y; PZ.h = h; PZ.ux = hx; PZ.uw = hw; PZ.ox = ox; PZ.ow = dw;
  gfx_offset_pop(); gfx_opacity_pop();
}
static void dly_draw(void) {
  float a = P.dlyA.v; if (a <= 0.004f) { DG.h = 0; return; }
  /* [ - ]  +0.25 s  [ + ]  |  [ play ]  [ Done ]: one row, the value large between its two keys */
  const float B = 76, G = 18, VW = 230, h = 120; static TMemo dW; const char *dn = T("Done"); float dw = tm_width(&dW, dn, 26, 700, 0) + 64;
  float w = 28 + B + G + VW + G + B + 28 + 2 + 28 + B + G + dw + 28, x = SB_W / 2 - w / 2, y = 44 - 24 * (1 - a);
  gfx_opacity_push(a); gfx_offset_push(nudge_x(&P.nudge), 0);
  gfx_shadow(x, y + 14, w, h, 30, 40, BLACK_A(110)); gfx_rrect(x, y, w, h, 30, RGBA(18, 16, 28, 222)); gfx_rrect_v(x, y, w, h, 30, WHITE_A(18), WHITE_A(4)); gfx_ring(x, y, w, h, 30, 1, WHITE_A(36));
  float cy = y + h / 2, bx[4], bw[4]; bx[0] = x + 28; bw[0] = B; float vx = bx[0] + B + G; bx[1] = vx + VW + G; bw[1] = B;
  float sep = bx[1] + B + 28; bx[2] = sep + 2 + 28; bw[2] = B; bx[3] = bx[2] + B + G; bw[3] = dw;
  gfx_rect(sep, cy - 30, 1.5f, 60, WHITE_A(40)); /* the timing apart from the playing */
  for (int i = 0; i < 4; i++) { DG.x[i] = bx[i]; DG.w[i] = bw[i]; } DG.y = cy - B / 2; DG.h = B;
  /* the focus: one accent shape gliding between the buttons (a circle, stretching into the Done pill) */
  { float k = P.dlyR.v; int i0 = (int)floorf(k); if (i0 < 0) i0 = 0; if (i0 > 2) i0 = 2; float f = k - i0; if (f < 0) f = 0; if (f > 1) f = 1;
    float fx = bx[i0] + (bx[i0 + 1] - bx[i0]) * f, fw = bw[i0] + (bw[i0 + 1] - bw[i0]) * f; if (k >= 3) { fx = bx[3]; fw = bw[3]; }
    lglass_accent(fx, cy - B / 2, fw, B, B / 2, 1); }
  for (int i = 0; i < 4; i++) { int f = (int)floorf(P.dlyR.v + 0.5f) == i; uint32_t c = f ? ACC_FG : WHITE_A(240);
    if (!f) gfx_rrect(bx[i], cy - B / 2, bw[i], B, B / 2, WHITE_A(22));
    if (i < 2) { float s = 26, t = 4; gfx_rrect(bx[i] + B / 2 - s / 2, cy - t / 2, s, t, 2, c); if (i == 1) gfx_rrect(bx[i] + B / 2 - t / 2, cy - s / 2, t, s, 2, c); } /* - and + drawn: even strokes, centred */
    else if (i == 2) icon_draw(paused() ? "p_play" : "p_pause", bx[i] + B / 2 - 17, cy - 17, 34, c);
    else text_draw(dn, bx[i] + bw[i] / 2, cy - text_line_height(26) / 2.0f, 26, 700, c, 0, 0, ALIGN_CENTER); }
  /* the delay: large, a small lift when it changes; under it what it means */
  { double dv = P.dlyKind ? P.audDelay : P.subDelay; char v[32]; if (dv == 0) snprintf(v, sizeof v, "0.00 s"); else snprintf(v, sizeof v, "%s%.2f s", dv > 0 ? "+" : "\xe2\x88\x92", fabs(dv));
    float e = (float)((now_() - P.dlyAt) / 0.22); e = e < 0 ? 0 : e > 1 ? 1 : e; float lift = 6 * (1 - ease_apply(EASE_OUT, e));
    text_draw(v, vx + VW / 2, cy - 30 - lift, 40, 800, WHITE_A(255), -0.4f, 0, ALIGN_CENTER);
    const char *m = P.dlyKind ? (dv > 0 ? T("Sound later") : dv < 0 ? T("Sound earlier") : T("Audio delay")) : P.subDelay > 0 ? T("Subtitles later") : P.subDelay < 0 ? T("Subtitles earlier") : T("Subtitle delay");
    text_draw(m, vx + VW / 2, cy + 18, 20, 600, WHITE_A(160), 0.3f, VW, ALIGN_CENTER); }
  gfx_offset_pop(); gfx_opacity_pop();
}
