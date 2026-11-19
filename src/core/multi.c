#include "platform/plat.h"
#include "core/multi.h"
#include "core/app.h"
#include "core/i18n.h"
#include "core/stremio.h"
#include "core/video.h"
#include "ui/gfx.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the second picture's platform side (video_atv.c); other platforms keep these no-ops and offer no button */
__attribute__((weak)) int  video2_available(void) { return 0; }
__attribute__((weak)) void video2_load(const char *url) { (void)url; }
__attribute__((weak)) void video2_layout(int split, int audio) { (void)split; (void)audio; }

static struct { int on, armed, aud; unsigned tok; } M;

int multi_available(void) { return video2_available(); }
int multi_on(void) { return M.on; }
int multi_armed(void) { return M.armed; }
int multi_audio_on(void) { return M.aud; }
void multi_disarm(void) { M.armed = 0; }
void multi_stop(void) { M.armed = 0; M.tok++; if (!M.on) return; M.on = 0; M.aud = 0; video2_layout(0, 0); }
void multi_audio(int which) { if (!M.on) return; M.aud = which ? 1 : 0; video2_layout(1, M.aud); }
void multi_button(void) {
  if (M.on) { multi_stop(); return; }
  M.armed = !M.armed;
  if (M.armed) toast(!strncmp(i18n_lang(), "pt", 2) ? "Escolha o canal da segunda tela" : "Pick the channel for the second screen", 2500, NULL);
}

/* the second screen's stream: the first one that plays on the web, as the channel list's own probe takes it */
static void on_part(StreamGroup *g, int gi, void *ud) { (void)gi;
  if ((unsigned)(uintptr_t)ud == M.tok) for (int i = 0; i < g->n; i++) { const Stream *s = &g->items[i]; if (!s->url[0] || s->ext[0] || s->p2p) continue;
    if (!M.on) { M.on = 1; M.aud = 0; video2_load(s->url); video2_layout(1, 0); fprintf(stderr, "[multi] second screen on\n"); } break; }
  free(g->items); }
static void on_done(int total, void *ud) { (void)total; if ((unsigned)(uintptr_t)ud == M.tok && !M.on) toast(!strncmp(i18n_lang(), "pt", 2) ? "Esse canal não abriu" : "That channel would not open", 2500, NULL); }
void multi_pick(const LiveCh *ch) {
  M.armed = 0; if (!ch) return; M.tok++; Item it; live_item(ch, &it);
  live_streams(it.type, it.id, on_part, on_done, (void *)(uintptr_t)M.tok); }

void multi_draw(void) {
  if (!M.on) return; float x = M.aud ? SB_W / 2 : 0, w = SB_W / 2; uint32_t c = RGBA(140, 120, 255, 255);
  gfx_rrect(x, 0, w, 6, 0, c); gfx_rrect(x, SB_H - 6, w, 6, 0, c); gfx_rrect(x, 0, 6, SB_H, 0, c); gfx_rrect(x + w - 6, 0, 6, SB_H, 0, c); }
