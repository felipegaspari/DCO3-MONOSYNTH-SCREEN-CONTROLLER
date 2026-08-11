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

Display-only: no touch/pointer input device is registered (the previous no-op touchpad stub was removed along with its LVGL indev).

---

## Runtime model

**Core 0:** open Serial1 IRQ + `init_screen_serial`; forever `serial_read_n` (slim LUT drain, RX-only — RX GP13 is fed by Input, and TX GP12 is unconnected); publish decoded fields + flags under `screen_state_lock()`. Core0 is the **sole writer** of `serialSignal`.

**Core 1:** LVGL + LovyanGFX; snapshot/consume flags under `screen_state_lock()`, release the lock, then update widgets; `lv_timer_handler()`. Core1 keeps its own local `currentMode` (latched from `serialSignal` in `handleScreenModeChange`) instead of writing `serialSignal` back.

Do not call LVGL from Core0 or UART parsers from Core1. Do not call any `lv_*` function while holding `screenStateMutex`.

---

## Modules

| File | Role |
|------|------|
| Main `.ino` | Dual-core entry, flush, `currentMode` + mode-change handling, UI update orchestration |
| `screen_mode.h` | `enum class ScreenMode` (1–8) + `screen_mode_raw()` helper |
| `Serial.h` / `Serial.ino` | `screenStateMutex` + lock wrappers, shared state, Serial1 slim LUT parser + handlers (LE, no finish) |
| `displayParams.h` / `displayParams.ino` | Param→label/model router (`screenParamTable`, `applyNavParam`), draw helpers |
| `serial_frame.h` / `serial_parser.h` / `serial_param_protocol.h` / `params_def.h` | Shared slim protocol (max payload 17) |

---

## External deps (edit carefully)

- **SquareLine `ui.h`** — regenerating overwrites widget names; keep draw helpers in sync.
- **`LGFX_RP2040_FELA.hpp`** — panel pins; outside this folder.
- **`params_def.h`** — keep ParamIds aligned with the DCO and Input boards; header guard text may still say "mainboard". IDs **156** (`PARAM_MANUAL_CALIBRATION_STORE`) and **170–173** (preset save/load/dump, cal dump) exist here for numeric parity only — no local handling on this board.

---

## Cross-core state rules (read before touching shared globals)

- Every field written by a Serial1 handler and read by `loop1` (or vice versa) must be inside a `screen_state_lock()`/`screen_state_unlock()` pair on **both** sides.
- Publish related fields together with their flag in one locked section (e.g. `paramNumber` + `paramValue` + `paramChangeFlag`) so the consumer never sees a half-updated set.
- Never call an `lv_*` function while the lock is held — snapshot into locals first, unlock, then draw.
- `paramName` is a `const char* volatile` pointing only at string literals — do not repoint it at a heap/stack buffer, or the cross-core safety guarantee breaks.
- `levelBarFlag` is an OR-accumulated bitmask consumed (and cleared) as one snapshot in `updateLevelBars` — don't reintroduce a single-value "last level changed" flag.

---

## Dead / misleading

- `fela_U8g2/`, `src/felanew_U8g2/` — unused (~70 MB)
- `tft_setup.h` — TFT_eSPI leftover, not included by the live sketch
- Removed entirely (no longer present): `globals.h`, `timers_millis.*`, `auxiliary.*`, `ui.ino`, `parameters.ino`, `presetNameString`/`presetNameBytesOLD` — their functionality either had no live callers or was folded into the `screenParamTable` router.
