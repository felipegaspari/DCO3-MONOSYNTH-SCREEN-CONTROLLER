/**
 * @file lv_conf.h
 * Configuration file for v9.5.0
 */

/*
 * Copy this file as `lv_conf.h`
 * 1. simply next to the `lvgl` folder
 * 2. or any other places and
 *    - define `LV_CONF_INCLUDE_SIMPLE`
 *    - add the path as include path
 */

/* clang-format off */
#if 1 /*Set it to "1" to enable content*/

#ifndef LV_CONF_H
#define LV_CONF_H

//#define LV_CONF_INCLUDE_SIMPLE

/*====================
 *   COLOR SETTINGS
 *====================*/

/*Color depth: 8 (A8), 16 (RGB565), 24 (RGB888), 32 (XRGB8888)*/
#define LV_COLOR_DEPTH 16

/*=========================
 *   STDLIB WRAPPER SETTINGS
 *=========================*/

/* Possible values
 * - LV_STDLIB_BUILTIN:     LVGL's built in implementation
 * - LV_STDLIB_CLIB:        Standard C functions, like malloc, strlen, etc
 * - LV_STDLIB_MICROPYTHON: MicroPython implementation
 * - LV_STDLIB_RTTHREAD:    RT-Thread implementation
 * - LV_STDLIB_CUSTOM:      Implement the functions externally
 */
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_BUILTIN

#if LV_USE_STDLIB_MALLOC == LV_STDLIB_BUILTIN
/*Size of the memory available for `lv_malloc()` in bytes (>= 2kB)*/
#define LV_MEM_SIZE (128 * 1024U)          /*[bytes]*/

/*Size of the memory expand for `lv_malloc()` in bytes*/
#define LV_MEM_POOL_EXPAND_SIZE 0

/*Set an address for the memory pool instead of allocating it as a normal array. Can be in external SRAM too.*/
#define LV_MEM_ADR 0     /*0: unused*/
/*Instead of an address give a memory allocator that will be called to get a memory pool for LVGL. E.g. my_malloc*/
#if LV_MEM_ADR == 0
#undef LV_MEM_POOL_INCLUDE
#undef LV_MEM_POOL_ALLOC
#endif
#endif  /*LV_USE_STDLIB_MALLOC == LV_STDLIB_BUILTIN*/

/*====================
 *   HAL SETTINGS
 *====================*/

/*Default display refresh, input device read and animation step period.*/
#define LV_DEF_REFR_PERIOD  30      /*[ms]*/

/*Default Dot Per Inch. Used to initialize default sizes such as widgets sized, style paddings.
 *(Not so important, you can adjust it to modify default sizes and spaces)*/
#define LV_DPI_DEF 130     /*[px/inch]*/

/*=================
 * OPERATING SYSTEM
 *=================*/
/*Select an operating system to use. Possible options:
 * - LV_OS_NONE
 * - LV_OS_PTHREAD
 * - LV_OS_FREERTOS
 * - LV_OS_CMSIS_RTOS2
 * - LV_OS_RTTHREAD
 * - LV_OS_WINDOWS
 * - LV_OS_ZEPHYR
 * - LV_OS_CUSTOM */
#define LV_USE_OS   LV_OS_NONE

#if LV_USE_OS == LV_OS_CUSTOM
#define LV_OS_CUSTOM_INCLUDE <stdint.h>
#endif

/*========================
 * RENDERING CONFIGURATION
 *========================*/

/*Align the stride of all layers and images to this bytes*/
#define LV_DRAW_BUF_STRIDE_ALIGN                1

/*Align the start address of draw_buf addresses to this bytes*/
#define LV_DRAW_BUF_ALIGN                       4

/*The target buffer size for simple layer chunks.*/
#define LV_DRAW_LAYER_SIMPLE_BUF_SIZE    (24 * 1024)   /*[bytes]*/

#define LV_USE_DRAW_SW 1
#if LV_USE_DRAW_SW == 1
/* Set the number of draw unit.
 * > 1 requires an operating system enabled in `LV_USE_OS` */
#define LV_DRAW_SW_DRAW_UNIT_CNT    1

/* 0: use a simple renderer capable of drawing only simple rectangles with gradient, images, texts, and straight lines only
 * 1: use a complex renderer capable of drawing rounded corners, shadow, skew lines, and arcs too */
#define LV_DRAW_SW_COMPLEX          1

#if LV_DRAW_SW_COMPLEX == 1
#define LV_DRAW_SW_SHADOW_CACHE_SIZE 0
#define LV_DRAW_SW_CIRCLE_CACHE_SIZE 4
#endif
#endif

/*=======================
 * FEATURE CONFIGURATION
 *=======================*/

/*-------------
 * Logging
 *-----------*/

/*Enable the log module*/
#define LV_USE_LOG 0
#if LV_USE_LOG
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1
#define LV_LOG_USE_TIMESTAMP 1
#define LV_LOG_USE_FILE_LINE 1
#endif

/*==================
 *   DEMO USAGE
 *==================*/

/*Show some widgets. This is the main demo most people look for.*/
#define LV_USE_DEMO_WIDGETS        1
#if LV_USE_DEMO_WIDGETS
#define LV_DEMO_WIDGETS_SLIDESHOW  0
#endif

/*Demonstrate usage of encoder and keyboard*/
#define LV_USE_DEMO_KEYPAD_AND_ENCODER     0

/*Benchmark your system*/
#define LV_USE_DEMO_BENCHMARK              0

/*Stress test for objects*/
#define LV_USE_DEMO_STRESS                 0

/*Music player demo*/
#define LV_USE_DEMO_MUSIC                  0

#endif /*LV_CONF_H*/
#endif /*End of "Content enable"*/
