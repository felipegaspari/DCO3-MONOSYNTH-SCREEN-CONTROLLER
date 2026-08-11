## DCO4 – Screen Controller (LVGL + LovyanGFX)

Arduino sketch for the **DCO4 screen board**: RP2040 dual-core firmware that renders a **480×320 LVGL** UI via **LovyanGFX**, fed by high-speed UART from the **Input** controller (params, presets, calibration, ADSR/level bars).

**This documentation covers only this folder:**  
`DCO4_Screen_Controller/RP2040_SCREEN_CONTROLLER_LVGL_LOVEYANGFX/`

System context: [`docs/SYSTEM_OVERVIEW.md`](docs/SYSTEM_OVERVIEW.md) → canonical overview in sibling **DCO**.

---

## Documentation

| Doc | Status | Contents |
|-----|--------|----------|
| [`docs/SYSTEM_OVERVIEW.md`](docs/SYSTEM_OVERVIEW.md) | Current | Stub → three-board overview + local UART table |
| [`docs/UI_AND_SERIAL.md`](docs/UI_AND_SERIAL.md) | Current | ScreenMode 1–8, Serial1 cmds, Core0→Core1 flags + cross-core lock |
| [`docs/HARDWARE.md`](docs/HARDWARE.md) | Current | UART pins, LovyanGFX deps, unused U8g2/TFT leftovers |
| [`docs/FILE_INDEX.md`](docs/FILE_INDEX.md) | Current | Project files + functions + call sites |
| [`docs/REFERENCE_AI.md`](docs/REFERENCE_AI.md) | Current | Semantic map for developers / AI |

---

## Features

- Dual-core: Core0 serial RX; Core1 LVGL render/update. All state shared between cores is guarded by a pico-sdk mutex (`screenStateMutex`) — Core0 publishes fields+flag together, Core1 snapshots them and releases the lock before touching LVGL.
- Screens: main preset/param UI + manual calibration (SquareLine `ui.h`).
- Serial1 @ 2.5 Mbaud from Input — the only peer link, receive-only on RX GP13 (TX GP12 is unconnected; Input never reads from the Screen). Carries ADSR bars, params, `'y'` nav (routed through the same param-router table as `'p'/'w'/'x'`), 16-char preset names, mode signals, plus the DCO calibration gap `'x'` 154 relayed by Input.
- Param toast labels, OSC/SUB level bars (bitmask-driven so simultaneous updates aren't lost), ADSR bars, calibration stage/offset/gap.

**Not active here:** touch input (no LVGL indev registered); USB MIDI product identity (commented); vendored U8g2 trees; TFT_eSPI `tft_setup.h`.

---

## Architecture

| Layer | Location |
|-------|----------|
| Entry / dual-core | `RP2040_SCREEN_CONTROLLER_LVGL_LOVEYANGFX.ino` |
| ScreenMode enum | `screen_mode.h` |
| Serial parsers + cross-core lock | `Serial.ino`, `Serial.h`, `serial_*.h` |
| Param → UI model | `displayParams.h`, `displayParams.ino` |
| Widgets | External `<ui.h>` (SquareLine) |
| Panel | LovyanGFX + sketch `LGFX_RP2040_FELA.hpp` |
| Per-synth UI differences | `screen_target.h` (`CalTopology`) |

Details: [`docs/UI_AND_SERIAL.md`](docs/UI_AND_SERIAL.md).

---

## Building

1. Open **this folder** as the Arduino sketch (`RP2040_SCREEN_CONTROLLER_LVGL_LOVEYANGFX.ino`).
2. Install / configure: **lvgl**, **LovyanGFX**, sketch board file **`LGFX_RP2040_FELA.hpp`** (keep it here — do not put it under `libraries/LovyanGFX/src/lgfx_user/`, a library update deletes that folder), SquareLine export providing **`ui.h`**.
3. Board: RP2040 (Earle Philhower core or compatible).
4. Tools -> USB Stack must be **Pico SDK**, not *No USB*; the sketch `#error`s otherwise.
5. Do not rely on `fela_U8g2/`, `tft_setup.h`, or `tusb_config.h.legacy` for the live build.

### Flags (project)

| Define | Role |
|--------|------|
| `LGFX_USE_V1` | LovyanGFX v1 API |
| `LV_COLOR_16_SWAP 0` | No RGB565 swap in flush |
| `SCREEN_SRAM_HOT` | `sram_hot.h`; `0` disables every SRAM pin at once for A/B |
| `SCREEN_PERF_MONITOR` | Top of main `.ino`; `0` hides and pauses the LVGL FPS/CPU overlay |
| `SCREEN_CAL_TOPOLOGY_DEFAULT` | `screen_target.h`; `CalTopology::Voices4x2` for the 4x2 voice board |

---

## Contributing / hacking

- Start with [`docs/REFERENCE_AI.md`](docs/REFERENCE_AI.md) and [`docs/FILE_INDEX.md`](docs/FILE_INDEX.md).
- Keep LVGL off Core0; keep UART off Core1.
- Any field read on one core and written on the other must be inside `screen_state_lock()`/`screen_state_unlock()` on both sides; never call `lv_*` while the lock is held.
- Align ParamIds with the DCO and Input boards; sync SquareLine widget names with draw helpers.
- Prefer documenting over deleting the unused U8g2 trees unless you intentionally clean the repo.
