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
// call mutex_init() on it before either core runs sketch code.
mutex_t screenStateMutex __attribute__((section(".mutex_array")));

// Forward declaration from displayParams.ino (router-backed apply table).
void applyNavParam(uint8_t id, int32_t value);
void setDisplayParam();

// Internal helper to update display variables without re-locking mutex
static inline void SCREEN_HOT(screen_set_param_internal)(uint8_t id, int32_t value) {
  paramNumber = id;
  paramValue  = value;
  setDisplayParam();
}

// ---------------------------------------------------------------------------
// Screen Controller Domain Block Handlers
// ---------------------------------------------------------------------------

// 'v' : PatchOscBlock (Oscillators, Intervals, Detunes, Drift & Portamento)
static void SCREEN_HOT(screenSerial1_handle_patch_osc_block)(char, const uint8_t* payload, uint8_t) {
  const PatchOscBlock* blk = reinterpret_cast<const PatchOscBlock*>(payload);
  screen_state_lock();

  // Wave switches
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_OSC1_SAW_ENABLE),   (blk->wave_enables & (1u << 0)) != 0);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_OSC1_PULSE_ENABLE), (blk->wave_enables & (1u << 1)) != 0);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_OSC1_TRI_ENABLE),   (blk->wave_enables & (1u << 2)) != 0);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_OSC2_SAW_ENABLE),   (blk->wave_enables & (1u << 3)) != 0);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_OSC2_PULSE_ENABLE), (blk->wave_enables & (1u << 4)) != 0);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_OSC2_TRI_ENABLE),   (blk->wave_enables & (1u << 5)) != 0);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_OSC3_SAW_ENABLE),   (blk->wave_enables & (1u << 6)) != 0);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_OSC3_PULSE_ENABLE), (blk->wave_enables & (1u << 7)) != 0);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_OSC3_TRI_ENABLE),   (blk->wave_enables & (1u << 8)) != 0);

  // Intervals & Detune
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_OSC1_INTERVAL),       blk->osc1_interval);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_OSC2_INTERVAL),       blk->osc2_interval);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_OSC3_INTERVAL),       blk->osc3_interval);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_OSC2_DETUNE_VAL),     blk->osc2_detune);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_UNISON_DETUNE),       blk->unison_detune);

  // Modes & Drift
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_VOICE_MODE),          blk->voice_mode);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_VOICE_ALLOC_MODE),    blk->voice_alloc_mode);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_SYNC_MODE),           blk->sync_mode);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_SOFT_SYNC),           blk->soft_sync);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_SUBOSC_DIVIDE),       blk->subosc_divide);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_ANALOG_DRIFT_AMOUNT), blk->analog_drift);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_ANALOG_DRIFT_SPEED),  blk->analog_drift_speed);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_ANALOG_DRIFT_SPREAD), blk->analog_drift_spread);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_PORTAMENTO_TIME),     blk->portamento_time);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_PORTAMENTO_MODE),     blk->portamento_mode);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_CHARACTER),           blk->character);

  screen_state_unlock();
}

// 'l' : PatchLfoBlock (LFOs, Depths, PWM & Pitch Mods)
static void SCREEN_HOT(screenSerial1_handle_patch_lfo_block)(char, const uint8_t* payload, uint8_t) {
  const PatchLfoBlock* blk = reinterpret_cast<const PatchLfoBlock*>(payload);
  screen_state_lock();

  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_LFO1_WAVEFORM),        blk->lfo1_waveform);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_LFO2_WAVEFORM),        blk->lfo2_waveform);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_LFO1_SPEED),           blk->lfo1_speed);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_LFO2_SPEED),           blk->lfo2_speed);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_LFO1_TO_DCO),          blk->lfo1_to_dco);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_LFO1_TO_OSC1),         blk->lfo1_to_osc1);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_LFO1_TO_OSC2),         blk->lfo1_to_osc2);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_LFO1_TO_OSC3),         blk->lfo1_to_osc3);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_LFO2_TO_OSC2),         blk->lfo2_to_osc2);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_LFO2_TO_OSC3),         blk->lfo2_to_osc3);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_LFO2_TO_OSC2_COARSE),  blk->lfo2_to_osc2_coarse);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_LFO2_TO_OSC3_COARSE),  blk->lfo2_to_osc3_coarse);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_LFO2_TO_PW),           blk->lfo2_to_pw);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_LFO1_TO_VCA),          blk->lfo1_to_vca);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_PW_VALUE),             blk->pw_value);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_ADSR1_TO_VCA),         blk->adsr1_to_vca);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_ADSR3_TO_PWM),         blk->adsr3_to_pwm);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_ADSR3_TO_DETUNE1),     blk->adsr3_to_detune1);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_ADSR3_PITCH_MODE),     blk->adsr3_pitch_mode);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_ADSR3_TO_OSC_SELECT),  blk->adsr3_to_osc_select);

  screen_state_unlock();
}

// 'M' : PatchModBlock (8 Mod Matrix Slots)
static void SCREEN_HOT(screenSerial1_handle_patch_mod_block)(char, const uint8_t* payload, uint8_t) {
  const PatchModBlock* blk = reinterpret_cast<const PatchModBlock*>(payload);
  screen_state_lock();

  for (uint8_t i = 0; i < 8; ++i) {
    screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_MOD_SLOT0_SOURCE) + i * 3, blk->slots[i].src);
    screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_MOD_SLOT0_DEST)   + i * 3, blk->slots[i].dest);
    screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_MOD_SLOT0_DEPTH)  + i * 3, blk->slots[i].depth);
  }

  screen_state_unlock();
}

// 'X' : PatchMixBlock (Mixer Levels, Filter Topology, Velocity & Curves)
static void SCREEN_HOT(screenSerial1_handle_patch_mix_block)(char, const uint8_t* payload, uint8_t) {
  const PatchMixBlock* blk = reinterpret_cast<const PatchMixBlock*>(payload);
  screen_state_lock();

  // Levels & Routing
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_OSC1_LEVEL),          blk->osc1_level);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_OSC2_LEVEL),          blk->osc2_level);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_OSC3_LEVEL),          blk->osc3_level);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_SUB_LEVEL),           blk->sub_level);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_VCA_LEVEL),           blk->vca_level);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_FILTER_MODE),         blk->filter_mode);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_VELOCITY_TO_VCF),     blk->velocity_to_vcf);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_VELOCITY_TO_VCA),     blk->velocity_to_vca);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_VCF_KEYTRACK),        blk->vcf_keytrack);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_ADSR1_TO_VCA),        blk->adsr1_to_vca);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_DIST_DRIVE),          blk->dist_drive);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_DIST_MIX),            blk->dist_mix);
  
  // Curves
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_ADSR1_ATTACK_CURVE),  blk->adsr1_attack_curve);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_ADSR1_DECAY_CURVE),   blk->adsr1_decay_curve);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_ADSR2_ATTACK_CURVE),  blk->adsr2_attack_curve);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_ADSR2_DECAY_CURVE),   blk->adsr2_decay_curve);

  // Flags
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_RESONANCE_COMPENSATION), (blk->misc_flags & (1 << 0)) != 0);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_VCA_ADSR_RESTART),       (blk->misc_flags & (1 << 1)) != 0);
  screen_set_param_internal(static_cast<uint8_t>(ParamId::PARAM_VCF_ADSR_RESTART),       (blk->misc_flags & (1 << 2)) != 0);
  
  screen_state_unlock();
}

// ---------------------------------------------------------------------------
// Standard Serial Frame Handlers
// ---------------------------------------------------------------------------

// 'a' : ADSR1 block (attack/decay/sustain/release).
static void SCREEN_HOT(screenSerial1_handle_adsr1)(char, const uint8_t* payload, uint8_t) {
  screen_state_lock();
  ADSR1Attack  = decode_u16_le(payload + 0);
  ADSR1Decay   = decode_u16_le(payload + 2);
  ADSR1Sustain = decode_u16_le(payload + 4);
  ADSR1Release = decode_u16_le(payload + 6);
  updateADSR1Flag = true;
  screen_state_unlock();
}

// 'b' : ADSR2 block.
static void SCREEN_HOT(screenSerial1_handle_adsr2)(char, const uint8_t* payload, uint8_t) {
  screen_state_lock();
  ADSR2Attack  = decode_u16_le(payload + 0);
  ADSR2Decay   = decode_u16_le(payload + 2);
  ADSR2Sustain = decode_u16_le(payload + 4);
  ADSR2Release = decode_u16_le(payload + 6);
  updateADSR2Flag = true;
  screen_state_unlock();
}

// Shared helper for 'p'/'x' frames.
static inline void SCREEN_HOT(screenSerial1_apply_param_from_frame)(const ParamFrame& frame) {
  screen_state_lock();
  paramNumber = frame.id;
  paramValue  = frame.value;

  // Manual calibration stage/offset go through the shared param router
  if (frame.id == static_cast<uint8_t>(ParamId::PARAM_MANUAL_CALIBRATION_STAGE) ||
      frame.id == static_cast<uint8_t>(ParamId::PARAM_MANUAL_CALIBRATION_OFFSET)) {
    applyNavParam(frame.id, frame.value);
  }

  // Input announces its oscillator count
  if (frame.id == static_cast<uint8_t>(ParamId::PARAM_UI_VOICE_TOPOLOGY)) {
    screenCalTopology = screen_topology_from_osc_count((uint8_t)frame.value);
  }

  setDisplayParam();

  // ONLY trigger toast if the param name was recognized and we are not silent
  if (paramName && paramName[0] != '\0') {
    if (serialSignal != screen_mode_raw(ScreenMode::Silent) ||
        (frame.id >= static_cast<uint8_t>(ParamId::PARAM_CALIBRATION_FLAG) &&
         frame.id <= static_cast<uint8_t>(ParamId::PARAM_MANUAL_CALIBRATION_OFFSET_FROM_DCO))) {
      paramChangeFlag = true;
    }
  }
  screen_state_unlock();
}

// 'p' : PARAM 16-bit
static void SCREEN_HOT(screenSerial1_handle_param16)(char, const uint8_t* payload, uint8_t) {
  ParamFrame frame;
  decode_param_p(payload, frame);
  screenSerial1_apply_param_from_frame(frame);
}

// 'x' : PARAM 32-bit (gap 154 relayed from DCO).
static void SCREEN_HOT(screenSerial1_handle_param32)(char, const uint8_t* payload, uint8_t) {
  ParamFrame frame;
  decode_param_x(payload, frame);
  screenSerial1_apply_param_from_frame(frame);
}

// 'q' : preset scroll (presetNumber + 16 chars).
static void SCREEN_HOT(screenSerial1_handle_preset_scroll)(char, const uint8_t* payload, uint8_t) {
  screen_state_lock();
  presetNumber = payload[0];
  for (int i = 0; i < 16; ++i) {
    uint8_t c = payload[i + 1];
    if (c < 32) c = 32;
    presetNameBytes[i] = (char)c;
  }
  presetNameBytes[16] = '\0';

  presetScrollFlag = true;
  paramChangeFlag  = false; // FIX: Prevent 2-second parameter toasts from blocking the preset view!
  screen_state_unlock();
}

// 's' : screen mode / signal.
static void SCREEN_HOT(screenSerial1_handle_signal)(char, const uint8_t* payload, uint8_t) {
  screen_state_lock();
  serialSignal = payload[0];
  signalFlag   = true;
  screen_state_unlock();
}

// 'c' : preset char index.
static void SCREEN_HOT(screenSerial1_handle_char_select)(char, const uint8_t* payload, uint8_t) {
  uint8_t idx = payload[0];
  if (idx > 15) idx = 15;
  screen_state_lock();
  presetChar     = idx;
  presetCharFlag = true;
  screen_state_unlock();
}

// 'd' : filter block. Scanned just to consume the payload, UI handles filter via 'p'
static void SCREEN_HOT(screenSerial1_handle_filter_block)(char, const uint8_t*, uint8_t) {}

static const SerialCommandDef screenSerial1Commands[] = {
  { CMD_ADSR1_BLOCK,   SERIAL_LEN_ADSR_BLOCK,           screenSerial1_handle_adsr1           },
  { CMD_ADSR2_BLOCK,   SERIAL_LEN_ADSR_BLOCK,           screenSerial1_handle_adsr2           },
  { CMD_FILTER_BLOCK,  SERIAL_LEN_FILTER_BLOCK,         screenSerial1_handle_filter_block    },
  { CMD_PARAM_16,      SERIAL_LEN_PARAM_16,             screenSerial1_handle_param16         },
  { CMD_PARAM_32,      SERIAL_LEN_PARAM_32,             screenSerial1_handle_param32         },
  { CMD_PRESET_NAME,   SERIAL_LEN_SCREEN_PRESET_SCROLL, screenSerial1_handle_preset_scroll   },
  { CMD_SCREEN_SIGNAL, SERIAL_LEN_SCREEN_SIGNAL,        screenSerial1_handle_signal          },
  { CMD_CHAR_SELECT,   SERIAL_LEN_CHAR_SELECT,          screenSerial1_handle_char_select     },
  { CMD_BLOCK_OSC,     SERIAL_LEN_BLOCK_OSC,            screenSerial1_handle_patch_osc_block },
  { CMD_BLOCK_LFO,     SERIAL_LEN_BLOCK_LFO,            screenSerial1_handle_patch_lfo_block },
  { CMD_BLOCK_MOD,     SERIAL_LEN_BLOCK_MOD,            screenSerial1_handle_patch_mod_block },
  { CMD_BLOCK_MIX,     SERIAL_LEN_BLOCK_MIX,            screenSerial1_handle_patch_mix_block },  
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