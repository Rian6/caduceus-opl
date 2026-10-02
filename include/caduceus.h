#ifndef __CADUCEUS_H
#define __CADUCEUS_H

/* Caduceus src/styles.css: bg, panel, line, muted, accent. GS alpha is 0..128. */
#define CAD_BG      GS_SETREG_RGBA(0x11, 0x13, 0x11, 0x80)
#define CAD_PANEL   GS_SETREG_RGBA(0x19, 0x1c, 0x18, 0x80)
#define CAD_BORDER  GS_SETREG_RGBA(0x2a, 0x2e, 0x27, 0x80)
#define CAD_MUTED   GS_SETREG_RGBA(0x8b, 0x92, 0x84, 0x80)
#define CAD_ACCENT  GS_SETREG_RGBA(0xd4, 0xef, 0x8a, 0x80)
#define CAD_TEXT    GS_SETREG_RGBA(0xee, 0xf1, 0xe8, 0x80)
#define CAD_OVERLAY GS_SETREG_RGBA(0x11, 0x13, 0x11, 0x58)
#define CAD_SHADOW  GS_SETREG_RGBA(0, 0, 0, 0x38)

#endif
