// Definitions for serial-related shared state.
// These were previously defined in Serial.h; they now live here with
// extern declarations in the header to avoid multiple-definition issues.
volatile byte    presetNumber       = 0;
String           presetNameString   = "Vacio";

volatile bool    presetScrollFlag   = false;

volatile byte    paramNumber        = 0;
volatile int32_t paramValue         = 0;
String           paramName;

volatile bool    paramChangeFlag    = false;

volatile bool    updateADSR1Flag    = false;
volatile bool    updateADSR2Flag    = false;

volatile bool    signalFlag         = false;
volatile byte    serialSignal       = 1;

volatile bool    presetCharFlag     = false;
volatile byte    presetChar         = 0;

volatile byte    levelBarFlag       = true;

// +1 for null terminator so LVGL/string APIs see a clean C-string.
volatile char    presetNameBytes[17];
volatile char    presetNameBytesOLD[17];

// Forward declaration from parameters.ino.
void updateParameters(byte paramNumberNavigation, int32_t paramValueNavigation);

// ---------------------------------------------------------------------------
// Screen controller serial parser (Serial1: input -> screen).
// Input is the only peer: it sends UI frames and relays the DCO gap 'x' (154).
// ---------------------------------------------------------------------------

static const uint8_t SCREEN_SERIAL_LEN_PARAM_16          = 3;   // [id, i16 LE]
static const uint8_t SCREEN_SERIAL_LEN_PARAM_8           = 2;   // [id, u8]
static const uint8_t SCREEN_SERIAL_LEN_PARAM_32          = 5;   // [id, u32 LE]
static const uint8_t SCREEN_SERIAL1_LEN_PRESET_SCROLL    = 17;  // [preset#, 16 chars]
static const uint8_t SCREEN_SERIAL_LEN_SIGNAL            = 1;   // [signal]
static const uint8_t SCREEN_SERIAL_LEN_CHAR_SELECT       = 1;   // [char index]
static const uint8_t SCREEN_SERIAL_LEN_ADSR_BLOCK        = 8;   // 4×u16 LE
static const uint8_t SCREEN_SERIAL_LEN_PARAM_BYTE_TO_NAV = 2;   // 'y': [paramId, value]

// ---------------------------
// Serial1 (input -> screen)
// ---------------------------

// 'a' : ADSR1 block (attack/decay/sustain/release) from input controller.
static void screenSerial1_handle_adsr1(char, const uint8_t* payload, uint8_t len) {
  if (len != SCREEN_SERIAL_LEN_ADSR_BLOCK) {
    return;
  }

  ADSR1Attack  = decode_u16_le(payload + 0);
  ADSR1Decay   = decode_u16_le(payload + 2);
  ADSR1Sustain = decode_u16_le(payload + 4);
  ADSR1Release = decode_u16_le(payload + 6);

  updateADSR1Flag = true;
}

// 'b' : ADSR2 block from input controller.
static void screenSerial1_handle_adsr2(char, const uint8_t* payload, uint8_t len) {
  if (len != SCREEN_SERIAL_LEN_ADSR_BLOCK) {
    return;
  }

  ADSR2Attack  = decode_u16_le(payload + 0);
  ADSR2Decay   = decode_u16_le(payload + 2);
  ADSR2Sustain = decode_u16_le(payload + 4);
  ADSR2Release = decode_u16_le(payload + 6);

  updateADSR2Flag = true;
}

// Shared helper for 'p'/'w'/'x' coming from input controller.
static inline void screenSerial1_apply_param_from_frame(const ParamFrame& frame) {
  paramNumber = frame.id;
  paramValue  = frame.value;

  setDisplayParam();

  if (serialSignal != 6) {
    paramChangeFlag = true;
  }
}

// 'p' : PARAM 16-bit from input controller.
static void screenSerial1_handle_param16(char, const uint8_t* payload, uint8_t len) {
  if (len != SCREEN_SERIAL_LEN_PARAM_16) {
    return;
  }
  ParamFrame frame;
  decode_param_p(payload, frame);
  screenSerial1_apply_param_from_frame(frame);
}

// 'w' : PARAM 8-bit from input controller.
static void screenSerial1_handle_param8(char, const uint8_t* payload, uint8_t len) {
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
static void screenSerial1_handle_param32(char, const uint8_t* payload, uint8_t len) {
  if (len != SCREEN_SERIAL_LEN_PARAM_32) {
    return;
  }
  ParamFrame frame;
  decode_param_x(payload, frame);
  screenSerial1_apply_param_from_frame(frame);
}

// 'y' : small navigation / calibration param from input controller.
// payload: [paramId, value]
static void screenSerial1_handle_param_nav_byte(char, const uint8_t* payload, uint8_t len) {
  if (len != SCREEN_SERIAL_LEN_PARAM_BYTE_TO_NAV) {
    return;
  }

  byte id    = payload[0];
  int8_t val = (int8_t)payload[1];

  paramNumber = id;
  paramValue  = (int16_t)val;

  updateParameters(paramNumber, (int32_t)paramValue);

  // For calibration-related 'y' updates coming from the input controller
  // (manual stage/offset, etc.), trigger a redraw of the calibration UI so
  // the on-screen values update immediately when changing oscillators or
  // tweaking the offset.
  if (paramNumber >= 150 && paramNumber <= 155) {
    paramChangeFlag = true;
  }
}

// 'q' : preset scroll from input controller.
// payload: [presetNumber, 16 chars]
static void screenSerial1_handle_preset_scroll(char, const uint8_t* payload, uint8_t len) {
  if (len != SCREEN_SERIAL1_LEN_PRESET_SCROLL) {
    return;
  }

  presetNumber = payload[0];
  for (int i = 0; i < 16; ++i) {
    uint8_t c = payload[i + 1];
    if (c < 32) {
      c = 32;
    }
    presetNameBytes[i] = (char)c;
  }
  presetNameBytes[16] = '\0';

  presetNameString = String((char*)presetNameBytes);
  presetScrollFlag = true;
}

// 's' : screen mode / signal from input controller.
static void screenSerial1_handle_signal(char, const uint8_t* payload, uint8_t len) {
  if (len != SCREEN_SERIAL_LEN_SIGNAL) {
    return;
  }
  serialSignal = payload[0];
  signalFlag   = true;
}

// 'c' : preset char index from input controller.
static void screenSerial1_handle_char_select(char, const uint8_t* payload, uint8_t len) {
  if (len != SCREEN_SERIAL_LEN_CHAR_SELECT) {
    return;
  }
  presetChar     = payload[0];
  presetCharFlag = true;
}

static const SerialCommandDef screenSerial1Commands[] = {
  { 'a', SCREEN_SERIAL_LEN_ADSR_BLOCK,        screenSerial1_handle_adsr1          },
  { 'b', SCREEN_SERIAL_LEN_ADSR_BLOCK,        screenSerial1_handle_adsr2          },
  { 'p', SCREEN_SERIAL_LEN_PARAM_16,          screenSerial1_handle_param16        },
  { 'w', SCREEN_SERIAL_LEN_PARAM_8,           screenSerial1_handle_param8         },
  { 'x', SCREEN_SERIAL_LEN_PARAM_32,          screenSerial1_handle_param32        },
  { 'y', SCREEN_SERIAL_LEN_PARAM_BYTE_TO_NAV, screenSerial1_handle_param_nav_byte },
  { 'q', SCREEN_SERIAL1_LEN_PRESET_SCROLL,    screenSerial1_handle_preset_scroll  },
  { 's', SCREEN_SERIAL_LEN_SIGNAL,            screenSerial1_handle_signal         },
  { 'c', SCREEN_SERIAL_LEN_CHAR_SELECT,       screenSerial1_handle_char_select    },
};

static SerialCommandTable screenSerial1Lut;
static SerialParserContext screenSerial1Parser = {};

void init_screen_serial() {
  serial_command_table_init(
    screenSerial1Lut,
    screenSerial1Commands,
    sizeof(screenSerial1Commands) / sizeof(screenSerial1Commands[0])
  );
}

// Core0: non-blocking Input Serial1 parser pump.
void serial_read_n() {
  serial_parser_drain(screenSerial1Parser, screenSerial1Lut, Serial1, SERIAL_DRAIN_BYTE_BUDGET);
}
