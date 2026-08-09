# Screen Controller — Reference (AI / developers)

Semantic map for **`RP2040_SCREEN_CONTROLLER_LVGL_LOVEYANGFX`** only.

- System: [`SYSTEM_OVERVIEW.md`](SYSTEM_OVERVIEW.md) → DCO canonical  
- UI/serial: [`UI_AND_SERIAL.md`](UI_AND_SERIAL.md)  
- Pins/deps: [`HARDWARE.md`](HARDWARE.md)  
- Call sites: [`FILE_INDEX.md`](FILE_INDEX.md)  
- Entry: [`../README.md`](../README.md)

---

## What this sketch owns

| Owns | Does not own |
|------|----------------|
| LVGL presentation of params, presets, cal UI | Voice engine / CV generation |
| UART RX from Input — the only peer, which also relays the DCO gap (154) | Preset file storage (Input LittleFS) |
| ScreenMode UI state machine | Panel scanning |

Display-only: touch callback is empty.

---

## Runtime model

**Core 0:** open Serial1 IRQ + `init_screen_serial`; forever `serial_read_n` (slim LUT drain, RX-only — RX GP13 is fed by Input, and TX GP12 is unconnected); set volatiles/flags.

**Core 1:** LVGL + LovyanGFX; consume flags; update widgets; `lv_timer_handler()`.

Do not call LVGL from Core0 or UART parsers from Core1.

---

## Modules

| File | Role |
|------|------|
| Main `.ino` | Dual-core entry, flush, ScreenMode helpers, UI update orchestration |
| `Serial.ino` | Serial1 slim LUT parser + handlers (LE, no finish) |
| `displayParams.ino` | Param→label/model, draw helpers |
| `parameters.ino` | `'y'` nav apply |
| `serial_frame.h` / `serial_parser.h` / `serial_param_protocol.h` / `params_def.h` | Shared slim protocol (max payload 17) |
| `timers_millis.*` | Soft timers (used lightly) |
| `auxiliary.*` | Blink helpers (**unused** in live loop) |

---

## External deps (edit carefully)

- **SquareLine `ui.h`** — regenerating overwrites widget names; keep draw helpers in sync.
- **`LGFX_RP2040_FELA.hpp`** — panel pins; outside this folder.
- **`params_def.h`** — keep ParamIds aligned with the DCO and Input boards; header guard text may still say “mainboard”.

---

## Dead / misleading

- `fela_U8g2/`, `src/felanew_U8g2/` — unused (~70 MB)  
- `ui.ino`, `tft_setup.h` — leftovers  
- Stale ParamId/signal comments at bottom of `Serial.h` / old lists — trust `ScreenMode` + `params_def.h`
