/*
Here is a complete C++ reference function demonstrating how to read every single field across all four structs together in code.

This can be used directly for a diagnostic UI dump, an LVGL "Patch Inspector" page, or a lock-free snapshot for Core 1:
code C++
*/

#include "displayParams.h"
#include <stdio.h>

void ui_inspect_full_patch_state() {
  // =========================================================================
  // Core 0 / Core 1 Safety: Take a local snapshot under the mutex lock
  // =========================================================================
  PatchOscBlock osc;
  PatchLfoBlock lfo;
  PatchModBlock mod;
  PatchMixBlock mix;

  screen_state_lock();
  osc = currentOscState;
  lfo = currentLfoState;
  mod = currentModState;
  mix = currentMixState;
  screen_state_unlock();

  // =========================================================================
  // 1. OSCILLATORS & VOICE ENGINE (PatchOscBlock)
  // =========================================================================
  // Unpack Wave Enables Bitmask
  bool osc1Saw   = (osc.wave_enables & (1u << 0)) != 0;
  bool osc1Pulse = (osc.wave_enables & (1u << 1)) != 0;
  bool osc1Tri   = (osc.wave_enables & (1u << 2)) != 0;
  bool osc2Saw   = (osc.wave_enables & (1u << 3)) != 0;
  bool osc2Pulse = (osc.wave_enables & (1u << 4)) != 0;
  bool osc2Tri   = (osc.wave_enables & (1u << 5)) != 0;
  bool osc3Saw   = (osc.wave_enables & (1u << 6)) != 0; // DCO3
  bool osc3Pulse = (osc.wave_enables & (1u << 7)) != 0; // DCO3
  bool osc3Tri   = (osc.wave_enables & (1u << 8)) != 0; // DCO3

  // Pitch, Intervals & Detune
  int8_t   octaveShift    = osc.osc1_interval;
  int8_t   osc2Interval   = osc.osc2_interval;
  int8_t   osc3Interval   = osc.osc3_interval;
  uint16_t osc2FineDetune = osc.osc2_detune;     // 0..511 (mid 256)
  int16_t  unisonSpread   = osc.unison_detune;

  // Voice Modes & Note Stealing Policy
  uint8_t voiceMode       = osc.voice_mode;       // 0=Mono, 1=Poly, 2=Unison
  uint8_t voiceAllocMode  = osc.voice_alloc_mode;  // 0=RoundRobin, 1=Oldest, 2=Quietest...
  uint8_t syncMode        = osc.sync_mode;        // 0=Off, 1=Hard Sync...
  uint8_t softSync        = osc.soft_sync;        // 0=Hard, 1..3=Soft
  uint8_t subDivide       = osc.subosc_divide;    // 0=Off, 2=-1 Oct, 4=-2 Oct

  // Analog Drift, Portamento & Character
  int8_t   driftDepth     = osc.analog_drift;
  int16_t  driftSpeed     = osc.analog_drift_speed;
  int8_t   driftSpread    = osc.analog_drift_spread;
  uint16_t portamentoTime = osc.portamento_time;
  uint8_t  portamentoMode = osc.portamento_mode;
  uint8_t  characterAmt   = osc.character;

  // =========================================================================
  // 2. LFOS & ENVELOPE MODULATION (PatchLfoBlock)
  // =========================================================================
  // Waveforms & Speeds
  uint8_t  lfo1Shape      = lfo.lfo1_waveform;
  uint8_t  lfo2Shape      = lfo.lfo2_waveform;
  uint16_t lfo1Speed      = lfo.lfo1_speed;
  uint16_t lfo2Speed      = lfo.lfo2_speed;

  // LFO Pitch & VCA Routings
  uint16_t lfo1VibratoDCO = lfo.lfo1_to_dco;
  uint8_t  lfo1ToOsc1     = lfo.lfo1_to_osc1;
  uint8_t  lfo1ToOsc2     = lfo.lfo1_to_osc2;
  uint8_t  lfo1ToOsc3     = lfo.lfo1_to_osc3;
  uint16_t lfo2ToOsc2Fine = lfo.lfo2_to_osc2;
  uint16_t lfo2ToOsc3Fine = lfo.lfo2_to_osc3;
  uint16_t lfo2ToOsc2Coar = lfo.lfo2_to_osc2_coarse;
  uint16_t lfo2ToOsc3Coar = lfo.lfo2_to_osc3_coarse;
  uint16_t lfo2PwmDepth   = lfo.lfo2_to_pw;
  uint16_t lfo1TremoloVCA = lfo.lfo1_to_vca;

  // Pulse Width & Envelope Routings
  uint16_t pulseWidth     = lfo.pw_value;
  int16_t  env1ToVCA      = lfo.adsr1_to_vca;
  int16_t  env3ToPwm      = lfo.adsr3_to_pwm;     // Centered at 512
  int16_t  env3ToPitch    = lfo.adsr3_to_detune1;
  uint8_t  adsr3Mode     = lfo.adsr3_mode; 
  uint8_t  adsr2Mode     = lfo.adsr2_mode;
  uint8_t  adsr1Mode     = lfo.adsr1_mode; 
  int8_t   env3OscTarget  = lfo.adsr3_to_osc_select;     // 0=Both, 1=OSC1, 2=OSC2

  // =========================================================================
  // 3. MODULATION MATRIX (PatchModBlock)
  // =========================================================================
  for (uint8_t i = 0; i < 8; ++i) {
    uint8_t src   = mod.slots[i].src;    // 0..15 (0xFF or 0 = Off)
    uint8_t dest  = mod.slots[i].dest;   // 0..11
    int16_t depth = mod.slots[i].depth;  // -2048..+2047 bipolar depth

    if (src != 0xFF && src != 0 && depth != 0) {
      // Slot is active: render connection line or table entry in LVGL
    }
  }

  // =========================================================================
  // 4. MIXER, FILTER, CURVES & SWITCHES (PatchMixBlock)
  // =========================================================================
  // Mix Levels
  uint8_t  osc1Vol        = mix.osc1_level;       // 0..128
  uint8_t  osc2Vol        = mix.osc2_level;       // 0..128
  uint8_t  osc3Vol        = mix.osc3_level;       // 0..128 (DCO3)
  uint8_t  subVol         = mix.sub_level;        // 0..128
  uint8_t  masterVcaBias  = mix.vca_level;        // 0..128
  uint8_t  filterMode     = mix.filter_mode;      // 0=4P LP, 1=2P LP, 2=BP, 3=HP

  // Dynamics & Keyboard Tracking
  int8_t   velToVcf       = mix.velocity_to_vcf;  // Cutoff velocity sensitivity
  int8_t   velToVca       = mix.velocity_to_vca;  // Volume velocity sensitivity
  int16_t  keytrack       = mix.vcf_keytrack;
  int16_t  envVcaDirect   = mix.adsr1_to_vca;
  uint16_t distDrive      = mix.dist_drive;       // 0..4095
  uint16_t distMix        = mix.dist_mix;         // 0..4095

  // Envelope Curve Profiles
  uint8_t  vcaAtkCurve    = mix.adsr1_attack_curve; // 0=Exp, 1=Soft, 7=Linear...
  uint8_t  vcaDecCurve    = mix.adsr1_decay_curve;
  uint8_t  vcfAtkCurve    = mix.adsr2_attack_curve;
  uint8_t  vcfDecCurve    = mix.adsr2_decay_curve;

  // Unpack Hardware Flags Bitmask
  bool     resoComp       = (mix.misc_flags & (1 << 0)) != 0;
  bool     vcaRestart     = (mix.misc_flags & (1 << 1)) != 0;
  bool     vcfRestart     = (mix.misc_flags & (1 << 2)) != 0;
  bool     adsr3Active    = (mix.misc_flags & (1 << 3)) != 0;
}

/*
Key Takeaways for UI Code
    Unlock: Always grab the snapshot with screen_state_lock() and copy the structs to local stack variables before updating LVGL labels or drawing canvas widgets.

    Bitmasks: wave_enables and misc_flags are decoded with standard bitwise shifts (1u << bit).

    Mod Matrix: Indexed directly with mod.slots[i].src, .dest, .depth across all 8 slots.

    Protocol Surface: Domain Block Structs (PatchOscBlock, PatchLfoBlock, PatchModBlock, PatchMixBlock) cached in RAM (currentOscState, currentLfoState, currentModState, currentMixState).

1. PatchOscBlock (currentOscState)

Wire Command: CMD_BLOCK_OSC ('v')  |  Payload Size: 22 Bytes
Controls oscillator pitch, wave selection, unison detune, voice allocation, analog drift, and portamento.
Field Name	Type	ParamId Equivalent	Values / Range	Description
wave_enables	uint16_t	PARAM_OSC*_ENABLE	Bitmask (see below)	Hardware wave switch enables (DG411 / 74HC595).
osc1_interval	int8_t	PARAM_OSC1_INTERVAL	Semitone offset	Global octave shift / root octave transposition.
osc2_interval	int8_t	PARAM_OSC2_INTERVAL	Semitone offset	OSC2 pitch interval offset relative to OSC1.
osc3_interval	int8_t	PARAM_OSC3_INTERVAL	Semitone offset	OSC3 pitch interval offset (DCO3 monosynth only).
osc2_detune	uint16_t	PARAM_OSC2_DETUNE_VAL	0..511 (mid 256)	Fine detune for OSC2.
unison_detune	int16_t	PARAM_UNISON_DETUNE	0..4095	Voice detune spread depth when in Unison mode.
voice_mode	uint8_t	PARAM_VOICE_MODE	0, 1, 2	0 = Mono, 1 = Poly, 2 = Unison.
voice_alloc_mode	uint8_t	PARAM_VOICE_ALLOC_MODE	0..5	Note stealing & priority policy (see below).
sync_mode	uint8_t	PARAM_SYNC_MODE	0..N	Oscillator master-to-slave hard sync routing.
soft_sync	uint8_t	PARAM_SOFT_SYNC	0..3	0 = Hard sync, 1..3 = Soft sync trailing chunk receptivity.
subosc_divide	uint8_t	PARAM_SUBOSC_DIVIDE	0, 2, 4	Sub-oscillator rate: 0 = Off, 2 = -1 Octave, 4 = -2 Octaves.
analog_drift	int8_t	PARAM_ANALOG_DRIFT_AMOUNT	-128..127	Analog pitch drift / temperature fluctuation depth.
analog_drift_speed	int16_t	PARAM_ANALOG_DRIFT_SPEED	0..4095	Speed/frequency of the drift LFO generator.
analog_drift_spread	int8_t	PARAM_ANALOG_DRIFT_SPREAD	0..127	Phase spread of drift across the 4 voices.
portamento_time	uint16_t	PARAM_PORTAMENTO_TIME	0..4095	Note glide time / slew rate.
portamento_mode	uint8_t	PARAM_PORTAMENTO_MODE	0..N	Portamento mode (e.g. Always vs Legato only).
character	uint8_t	PARAM_CHARACTER	0..128	Vintage DCO jitter / character amount.
wave_enables Bitmask Breakdown:

    1 << 0: OSC1 Saw

    1 << 1: OSC1 Pulse

    1 << 2: OSC1 Triangle

    1 << 3: OSC2 Saw

    1 << 4: OSC2 Pulse

    1 << 5: OSC2 Triangle (DCO3 only)

    1 << 6: OSC3 Saw (DCO3 only)

    1 << 7: OSC3 Pulse (DCO3 only)

    1 << 8: OSC3 Triangle (DCO3 only)

voice_alloc_mode Values:

    0: Round Robin (Poly LRU) / Last-Note Priority (Mono)

    1: Oldest Voice (Poly) / First-Note Priority (Mono)

    2: Quietest Voice (lowest EnvVCA level) / Last-Note Priority

    3: Quietest (spares lowest held note) / Low-Note Priority

    4: Quietest (spares highest held note) / High-Note Priority

    5: No Stealing (drops extra notes) / First-Note Priority

2. PatchLfoBlock (currentLfoState)

Wire Command: CMD_BLOCK_LFO ('l')  |  Payload Size: 33 Bytes
Controls LFO waveforms, speeds, routings to DCO/VCA/PWM, and Envelope 3 modulation.
Field Name	Type	ParamId Equivalent	Values / Range	Description
lfo1_waveform	uint8_t	PARAM_LFO1_WAVEFORM	0..N	LFO1 Shape: Triangle, Sine, Saw, Square, S&H, etc.
lfo2_waveform	uint8_t	PARAM_LFO2_WAVEFORM	0..N	LFO2 Shape.
lfo1_speed	uint16_t	PARAM_LFO1_SPEED	0..4095	Raw exponential speed parameter for LFO1.
lfo2_speed	uint16_t	PARAM_LFO2_SPEED	0..4095	Raw exponential speed parameter for LFO2.
lfo1_to_dco	uint16_t	PARAM_LFO1_TO_DCO	0..4095	Global vibrato / pitch depth from LFO1 to all oscillators.
lfo1_to_osc1	uint8_t	PARAM_LFO1_TO_OSC1	0..255	Additive pitch modulation from LFO1 onto OSC1 only.
lfo1_to_osc2	uint8_t	PARAM_LFO1_TO_OSC2	0..255	Additive pitch modulation from LFO1 onto OSC2 only.
lfo1_to_osc3	uint8_t	PARAM_LFO1_TO_OSC3	0..255	Additive pitch modulation from LFO1 onto OSC3 only (DCO3).
lfo2_to_osc2	uint16_t	PARAM_LFO2_TO_OSC2	0..4095	Fine pitch modulation from LFO2 to OSC2.
lfo2_to_osc3	uint16_t	PARAM_LFO2_TO_OSC3	0..4095	Fine pitch modulation from LFO2 to OSC3 (DCO3).
lfo2_to_osc2_coarse	uint16_t	PARAM_LFO2_TO_OSC2_COARSE	0..511	Wide/coarse pitch modulation from LFO2 to OSC2.
lfo2_to_osc3_coarse	uint16_t	PARAM_LFO2_TO_OSC3_COARSE	0..511	Wide/coarse pitch modulation from LFO2 to OSC3 (DCO3).
lfo2_to_pw	uint16_t	PARAM_LFO2_TO_PW	0..4095	Pulse Width Modulation (PWM) depth from LFO2.
lfo1_to_vca	uint16_t	PARAM_LFO1_TO_VCA	0..4095	Tremolo / Amplitude modulation depth from LFO1 to VCA.
pw_value	uint16_t	PARAM_PW_VALUE	0..4095	Base pulse width duty cycle.
adsr1_to_vca	int16_t	PARAM_ADSR1_TO_VCA	0..4095	Envelope 1 (EnvVCA) direct volume depth.
adsr3_to_pwm	int16_t	PARAM_ADSR3_TO_PWM	0..1023 (mid 512)	Bipolar EnvDCO modulation to Pulse Width.
adsr3_to_detune1	int16_t	PARAM_ADSR3_TO_DETUNE1	0..4095	EnvDCO pitch sweep depth.
adsr3_pitch_mode	uint8_t	PARAM_ADSR3_PITCH_MODE	0, 1	0 = Unipolar pitch tap, 1 = Centered pitch tap (±2 oct).
adsr3_to_osc_select	int8_t	PARAM_ADSR3_TO_OSC_SELECT	0..3	0 = Both, 1 = OSC1 only, 2 = OSC2 only.
3. PatchModBlock (currentModState)

Wire Command: CMD_BLOCK_MOD ('M')  |  Payload Size: 32 Bytes (8 Slots4 Bytes)
Holds the 8 modulation matrix routings.

Access via currentModState.slots[0..7]:

struct ModSlotPacked {
  uint8_t src;    // Modulation Source ID
  uint8_t dest;   // Modulation Destination ID
  int16_t depth;  // Bipolar depth (-2048..+2047)
};

Sub-Field	Type	Values	Description
src	uint8_t	0..15, 0xFF	Source: LFO1, LFO2, Env1, Env2, Env3, Velocity, Aftertouch, ModWheel, Keytrack. 0xFF or 0 = Off/Empty.
dest	uint8_t	0..11	Destination: Pitch, Cutoff, Resonance, VCA, PWM, LFO Speed, etc.
depth	int16_t	-2048..2047	Signed bipolar modulation amount.
4. PatchMixBlock (currentMixState)

Wire Command: CMD_BLOCK_MIX ('Q')  |  Payload Size: 21 Bytes
Controls mixer levels, analog filter topology, overdrive/distortion, dynamics, and ADSR curve profiles.
Field Name	Type	ParamId Equivalent	Values / Range	Description
osc1_level	uint8_t	PARAM_OSC1_LEVEL	0..128	Volume level for OSC1 (Drives UI Level Bar).
osc2_level	uint8_t	PARAM_OSC2_LEVEL	0..128	Volume level for OSC2 (Drives UI Level Bar).
osc3_level	uint8_t	PARAM_OSC3_LEVEL	0..128	Volume level for OSC3 (DCO3 monosynth only).
sub_level	uint8_t	PARAM_SUB_LEVEL	0..128	Volume level for Sub-oscillator (Drives UI Level Bar).
vca_level	uint8_t	PARAM_VCA_LEVEL	0..128	Initial master VCA bias level / manual open volume.
filter_mode	uint8_t	PARAM_FILTER_MODE	0..3	Filter mode: 0=4-Pole LP, 1=2-Pole LP, 2=BP, 3=HP.
velocity_to_vcf	int8_t	PARAM_VELOCITY_TO_VCF	-128..127	Keyboard velocity sensitivity to filter cutoff.
velocity_to_vca	int8_t	PARAM_VELOCITY_TO_VCA	-128..127	Keyboard velocity sensitivity to volume dynamics.
vcf_keytrack	int16_t	PARAM_VCF_KEYTRACK	0..8000	Filter cutoff tracking relative to keyboard pitch.
adsr1_to_vca	int16_t	PARAM_ADSR1_TO_VCA	0..4095	EnvVCA direct VCA scaling.
dist_drive	uint16_t	PARAM_DIST_DRIVE	0..4095	Post-filter analog distortion Drive VCA control.
dist_mix	uint16_t	PARAM_DIST_MIX	0..4095	Distortion dry/wet Mix VCA control.
adsr1_attack_curve	uint8_t	PARAM_ADSR1_ATTACK_CURVE	0..7	Env1 Attack profile: Exp, Soft, Steep, S-Curve, Linear.
adsr1_decay_curve	uint8_t	PARAM_ADSR1_DECAY_CURVE	0..8	Env1 Decay profile.
adsr2_attack_curve	uint8_t	PARAM_ADSR2_ATTACK_CURVE	0..7	Env2 (VCF) Attack profile.
adsr2_decay_curve	uint8_t	PARAM_ADSR2_DECAY_CURVE	0..8	Env2 (VCF) Decay profile.
misc_flags	uint8_t	Multiple	Bitmask (see below)	Boolean hardware switches and envelope restarts.
misc_flags Bitmask Breakdown:

    1 << 0: PARAM_RESONANCE_COMPENSATION (Bass retention under high resonance)

    1 << 1: PARAM_ADSR1_RESTART (Env1 resets to zero on retrigger)

    1 << 2: PARAM_ADSR2_RESTART (Env2 resets to zero on retrigger)

    1 << 3: PARAM_ADSR3_ENABLED (Env3 / Pitch envelope active flag)
*/
