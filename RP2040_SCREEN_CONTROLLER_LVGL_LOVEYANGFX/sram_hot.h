#ifndef __SRAM_HOT_H__
#define __SRAM_HOT_H__

// RP2040: Pico SDK / Arduino-Pico already define __not_in_flash_func.
// Fallback keeps non-Pico hosts (linter) compiling.
#ifndef __not_in_flash_func
#define __not_in_flash_func(fn) fn
#endif

// Master switch: build with -DSCREEN_SRAM_HOT=0 to A/B every pin at once.
#ifndef SCREEN_SRAM_HOT
#define SCREEN_SRAM_HOT 1
#endif

#if SCREEN_SRAM_HOT
#define SCREEN_HOT(fn) __not_in_flash_func(fn)
#else
#define SCREEN_HOT(fn) fn
#endif

#endif
