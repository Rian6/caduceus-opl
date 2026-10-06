/*
  RetroAchievements unlock notice over the running game; see
  src/ra_overlay.c.
*/
#ifndef RA_OVERLAY_H
#define RA_OVERLAY_H

#include "../../modules/network/common/ra_features.h"

/* Where raudp DMAs events to (struct ra_event, 128 bytes, in two 64-byte
   lines of its own). iopmgr.c passes this address to the module. */
void *RA_OverlayEventBuffer(void);

/* Optional experimental packet workspace; NULL in the default recovery build. */
void RA_OverlaySetPacketBuffer(void *buffer);

/* Per-frame hook, called from RA_OnVblank() with the frame counter that
   ra.c keeps. Consumes events and handles reset even with the card disabled;
   rendering is experimental and disabled by default. */
void RA_OverlayOnVblank(unsigned int frames);

#endif
