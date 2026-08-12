// Definitions for serial-related shared state (extern declarations in Serial.h).
volatile byte    presetNumber       = 0;

volatile bool    presetScrollFlag   = false;

volatile byte    paramNumber        = 0;
volatile int32_t paramValue         = 0;
const char* volatile paramName      = "";

volatile bool    paramChangeFlag    = false;

volatile bool    updateADSR1Flag    = false;
volatile bool    updateADSR2Flag    = false;

volatile bool    signalFlag         = false;
volatile byte    serialSignal       = screen_mode_raw(ScreenMode::PresetScroll);

volatile bool    presetCharFlag     = false;
volatile byte    presetChar         = 0;

volatile byte    levelBarFlag       = 0;

// +1 for null terminator so LVGL/string APIs see a clean C-string.
volatile char    presetNameBytes[17];

// Cross-core state lock. The .mutex_array section makes the pico-sdk runtime
// call mutex_init() on it before either core runs sketch code, so there is no
// setup()/setup1() ordering hazard.
mutex_t screenStateMutex __attribute__((section(".mutex_array")));

// Forward declaration from displayParams.ino (router-backed apply table).
void applyNavParam(uint8_t id, int32_t value);

// ---------------------------------------------------------------------------
// Screen controller serial parsers.
// DCO3: Input → Serial1 GP13.
// DCO4: Mainboard PA9 → Serial2 GP21, and Input → Serial1 GP13 (both drained).
// Same UI frame set on every peer; handlers run on Core0 under screen_state_lock.
// ---------------------------------------------------------------------------

static const uint8_t SCREEN_SERIAL_LEN_PARAM_16          = 3;   // [id, i16 LE]
static const uint8_t SCREEN_SERIAL_LEN_PARAM_8           = 2;   // [id, u8]
static const uint8_t SCREEN_SERIAL_LEN_PARAM_32          = 5;   // [id, u32 LE]
static const uint8_t SCREEN_SERIAL1_LEN_PRESET_SCROLL    = 17;  // [preset#, 16 chars]
static const uint8_t SCREEN_SERIAL_LEN_SIGNAL            = 1;   // [signal]
static const uint8_t SCREEN_SERIAL_LEN_CHAR_SELECT       = 1;   // [char index]
static const uint8_t SCREEN_SERIAL_LEN_ADSR_BLOCK        = 8;   // 4×u16 LE
static const uint8_t SCREEN_SERIAL_LEN_FILTER_BLOCK      = 8;   // 4×u16 LE, consumed only
static const uint8_t SCREEN_SERIAL_LEN_PARAM_BYTE_TO_NAV = 2;   // 'y': [paramId, value]

// ---------------------------
// Peer UART handlers
// ---------------------------

// 'a' : ADSR1 block (attack/decay/sustain/release).
static void SCREEN_HOT(screenSerial1_handle_adsr1)(char, const uint8_t* payload, uint8_t len) {
  if (len != SCREEN_SERIAL_LEN_ADSR_BLOCK) {
    return;
  }

  screen_state_lock();
  ADSR1Attack  = decode_u16_le(payload + 0);
  ADSR1Decay   = decode_u16_le(payload + 2);
  ADSR1Sustain = decode_u16_le(payload + 4);
  ADSR1Release = decode_u16_le(payload + 6);
  updateADSR1Flag = true;
  screen_state_unlock();
}

// 'b' : ADSR2 block from input controller.
static void SCREEN_HOT(screenSerial1_handle_adsr2)(char, const uint8_t* payload, uint8_t len) {
  if (len != SCREEN_SERIAL_LEN_ADSR_BLOCK) {
    return;
  }

  screen_state_lock();
  ADSR2Attack  = decode_u16_le(payload + 0);
  ADSR2Decay   = decode_u16_le(payload + 2);
  ADSR2Sustain = decode_u16_le(payload + 4);
  ADSR2Release = decode_u16_le(payload + 6);
  updateADSR2Flag = true;
  screen_state_unlock();
}

// Shared helper for 'p'/'w'/'x' coming from input controller.
static inline void SCREEN_HOT(screenSerial1_apply_param_from_frame)(const ParamFrame& frame) {
  screen_state_lock();
  paramNumber = frame.id;
  paramValue  = frame.value;

  setDisplayParam();

  if (serialSignal != screen_mode_raw(ScreenMode::Silent)) {
    paramChangeFlag = true;
  }
  screen_state_unlock();
}

// 'p' : PARAM 16-bit from input controller.
static void SCREEN_HOT(screenSerial1_handle_param16)(char, const uint8_t* payload, uint8_t len) {
  if (len != SCREEN_SERIAL_LEN_PARAM_16) {
    return;
  }
  ParamFrame frame;
  decode_param_p(payload, frame);
  screenSerial1_apply_param_from_frame(frame);
}

// 'w' : PARAM 8-bit from input controller.
static void SCREEN_HOT(screenSerial1_handle_param8)(char, const uint8_t* payload, uint8_t len) {
  if (len != SCREEN_SERIAL_LEN_PARAM_8) {
    return;
  }
  ParamFrame frame;
  decode_param_w(payload, frame);
  // Ignore manual calibration stage/offset on the generic 'w' path.
  // The screen's manual calibration UI is driven solely by 'y' nav frames
  // from the input controller so stage and offset updates are tightly
  // coupled and under input's control.
  if (frame.id == static_cast<uint8_t>(ParamId::PARAM_MANUAL_CALIBRATION_STAGE) ||
      frame.id == static_cast<uint8_t>(ParamId::PARAM_MANUAL_CALIBRATION_OFFSET)) {
    return;
  }

  // Default case for other 8-bit params:
  // We want 0..255 display semantics for most 8‑bit parameters (levels,
  // menu flags, etc.). decode_param_w() interprets the payload as int8_t,
  // which would make values >127 appear negative. Reinterpret the underlying
  // 8-bit pattern as unsigned here and promote to int32_t so the UI sees 0..255.
  uint8_t raw = static_cast<uint8_t>(frame.value);
  frame.value = static_cast<int32_t>(raw);
  screenSerial1_apply_param_from_frame(frame);
}

// 'x' : PARAM 32-bit from input controller (gap 154 relayed from DCO).
static void SCREEN_HOT(screenSerial1_handle_param32)(char, const uint8_t* payload, uint8_t len) {
  if (len != SCREEN_SERIAL_LEN_PARAM_32) {
    return;
  }
  ParamFrame frame;
  decode_param_x(payload, frame);
  screenSerial1_apply_param_from_frame(frame);
}

// 'y' : small navigation / calibration param from input controller.
// payload: [paramId, value]
static void SCREEN_HOT(screenSerial1_handle_param_nav_byte)(char, const uint8_t* payload, uint8_t len) {
  if (len != SCREEN_SERIAL_LEN_PARAM_BYTE_TO_NAV) {
    return;
  }

  uint8_t id  = payload[0];
  int32_t val = (int8_t)payload[1];

  screen_state_lock();
  paramNumber = id;
  paramValue  = val;

  // Manual calibration stage/offset go through the shared param router so the
  // clamp/derive logic lives in one place (displayParams.ino apply table).
  if (id == static_cast<uint8_t>(ParamId::PARAM_MANUAL_CALIBRATION_STAGE) ||
      id == static_cast<uint8_t>(ParamId::PARAM_MANUAL_CALIBRATION_OFFSET)) {
    applyNavParam(id, val);
  }

  // 157: Input announces its oscillator count (board_model.h NUM_OSCILLATORS)
  // so this screen's whole calibration UI (screen_target.h) tracks whichever
  // synth it's attached to. Deliberately outside the 150..155 range below,
  // so it never raises paramChangeFlag / shows a toast.
  if (id == static_cast<uint8_t>(ParamId::PARAM_UI_VOICE_TOPOLOGY)) {
    screenCalTopology = screen_topology_from_osc_count((uint8_t)val);
  }

  // Calibration-related 'y' updates (stage/offset/gap, 150..155) trigger a
  // redraw of the calibration UI so on-screen values update immediately.
  if (id >= static_cast<uint8_t>(ParamId::PARAM_CALIBRATION_FLAG) &&
      id <= static_cast<uint8_t>(ParamId::PARAM_MANUAL_CALIBRATION_OFFSET_FROM_DCO)) {
    paramChangeFlag = true;
  }
  screen_state_unlock();
}

// 'q' : preset scroll from input controller.
// payload: [presetNumber, 16 chars]
static void SCREEN_HOT(screenSerial1_handle_preset_scroll)(char, const uint8_t* payload, uint8_t len) {
  if (len != SCREEN_SERIAL1_LEN_PRESET_SCROLL) {
    return;
  }

  screen_state_lock();
  presetNumber = payload[0];
  for (int i = 0; i < 16; ++i) {
    uint8_t c = payload[i + 1];
    if (c < 32) {
      c = 32;
    }
    presetNameBytes[i] = (char)c;
  }
  presetNameBytes[16] = '\0';

  presetScrollFlag = true;
  screen_state_unlock();
}

// 's' : screen mode / signal from input controller.
static void SCREEN_HOT(screenSerial1_handle_signal)(char, const uint8_t* payload, uint8_t len) {
  if (len != SCREEN_SERIAL_LEN_SIGNAL) {
    return;
  }
  screen_state_lock();
  serialSignal = payload[0];
  signalFlag   = true;
  screen_state_unlock();
}

// 'c' : preset char index from input controller.
static void SCREEN_HOT(screenSerial1_handle_char_select)(char, const uint8_t* payload, uint8_t len) {
  if (len != SCREEN_SERIAL_LEN_CHAR_SELECT) {
    return;
  }
  uint8_t idx = payload[0];
  if (idx > 15) {           // index into the 16-char preset name
    idx = 15;
  }
  screen_state_lock();
  presetChar     = idx;
  presetCharFlag = true;
  screen_state_unlock();
}

// 'd' : filter block. Nothing is displayed from it — cutoff and resonance reach
// this board as the 191-194 UI 'p' ids — but it is registered so the eight
// payload bytes are consumed as a payload. An unregistered command byte is
// dropped and its payload is then scanned as commands, and a cutoff low byte of
// 'a' or 'p' would start a bogus frame.
static void SCREEN_HOT(screenSerial1_handle_filter_block)(char, const uint8_t*, uint8_t) {
}

static const SerialCommandDef screenSerial1Commands[] = {
  { 'a', SCREEN_SERIAL_LEN_ADSR_BLOCK,        screenSerial1_handle_adsr1          },
  { 'b', SCREEN_SERIAL_LEN_ADSR_BLOCK,        screenSerial1_handle_adsr2          },
  { 'd', SCREEN_SERIAL_LEN_FILTER_BLOCK,      screenSerial1_handle_filter_block   },
  { 'p', SCREEN_SERIAL_LEN_PARAM_16,          screenSerial1_handle_param16        },
  { 'w', SCREEN_SERIAL_LEN_PARAM_8,           screenSerial1_handle_param8         },
  { 'x', SCREEN_SERIAL_LEN_PARAM_32,          screenSerial1_handle_param32        },
  { 'y', SCREEN_SERIAL_LEN_PARAM_BYTE_TO_NAV, screenSerial1_handle_param_nav_byte },
  { 'q', SCREEN_SERIAL1_LEN_PRESET_SCROLL,    screenSerial1_handle_preset_scroll  },
  { 's', SCREEN_SERIAL_LEN_SIGNAL,            screenSerial1_handle_signal         },
  { 'c', SCREEN_SERIAL_LEN_CHAR_SELECT,       screenSerial1_handle_char_select    },
};

static SerialCommandTable screenPeerLut;
#if SCREEN_HAS_INPUT_PEER
static SerialParserContext screenInputParser = {};
#endif
#if SCREEN_HAS_MB_PEER
static SerialParserContext screenMbParser = {};
#endif

void init_screen_serial() {
  serial_command_table_init(
    screenPeerLut,
    screenSerial1Commands,
    sizeof(screenSerial1Commands) / sizeof(screenSerial1Commands[0])
  );
}

// Core0: non-blocking peer UART parser pump(s).
void SCREEN_HOT(serial_read_n)() {
#if SCREEN_HAS_MB_PEER
  serial_parser_drain(screenMbParser, screenPeerLut, SCREEN_MB_PORT, SERIAL_DRAIN_BYTE_BUDGET);
#endif
#if SCREEN_HAS_INPUT_PEER
  serial_parser_drain(screenInputParser, screenPeerLut, SCREEN_INPUT_PORT, SERIAL_DRAIN_BYTE_BUDGET);
#endif
}
