/* Multi-screen: a second live channel beside the one playing (side by side, half the screen each); the sound is on one
   of them at a time and Left / Right passes it from one to the other. The second picture is a second decoder on the
   platform's video plane (Android TV: VideoPlane.java); where there is none, the button is not offered. */
#ifndef SB_MULTI_H
#define SB_MULTI_H
#include "core/live.h"
int  multi_available(void);               /* this TV can play two pictures */
int  multi_on(void);                      /* two screens are showing */
int  multi_armed(void);                   /* the next channel taken in the channels is the second screen */
void multi_button(void);                  /* the player's Multi-screen button: off -> pick a channel; on -> back to one screen */
void multi_pick(const LiveCh *ch);        /* the channel chosen for the second screen */
void multi_stop(void);
void multi_disarm(void);                  /* the channels closed without a pick */
void multi_audio(int which);              /* 0 = the first screen has the sound, 1 = the second */
int  multi_audio_on(void);
void multi_draw(void);                    /* the frame round the screen that has the sound */
#endif
