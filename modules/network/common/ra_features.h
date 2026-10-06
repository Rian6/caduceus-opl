#ifndef RA_FEATURES_H
#define RA_FEATURES_H

/* The GS breakpoint/IMAGE card is not validated on retail consoles and can
   prevent a game from booting. Keep telemetry and event/reset DMA independent
   of this experimental renderer. Opt-in requires the same flag in loader/core. */
#ifndef RA_STATIC_CARD_TEST
#define RA_STATIC_CARD_TEST 0
#endif

#ifndef RA_ENABLE_EXPERIMENTAL_CARD
#define RA_ENABLE_EXPERIMENTAL_CARD RA_STATIC_CARD_TEST
#endif

/* Shared geometry keeps the loader allocation and IMAGE payload identical. */
#define RA_CARD_WIDTH 192
#define RA_CARD_HEIGHT 40

#endif
