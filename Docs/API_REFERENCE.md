# Audio Engine API Reference

Complete function reference for the GD32 Audio Engine with all getters, setters, and control functions.

## Table of Contents

1. [Initialization](#initialization)
2. [Playback Control](#playback-control)
3. [DAC Power Control](#dac-power-control)
4. [Volume Response Control](#volume-response-control)
5. [Filter Configuration](#filter-configuration)
6. [Low-Pass Filter (LPF) Control](#low-pass-filter-lpf-control)
7. [Air Effect Control](#air-effect-control)
8. [Fade Time Control](#fade-time-control)
9. [Chunk Processing (DMA Callbacks)](#chunk-processing-dma-callbacks)
10. [Application Callbacks](#application-callbacks)
11. [Status & Queries](#status-queries)

---

## Initialization

### `AudioEngine_Init()`

Initialize the audio engine with required hardware interface callbacks.

```c
PB_StatusTypeDef AudioEngine_Init(
  DAC_SwitchFunc dac_switch,      // Function to control amplifier on/off
  ReadVolumeFunc read_volume,     // Function to read volume setting (1-65535)
  I2S_InitFunc i2s_init           // Function to re-initialize I2S if needed
);
```

**Parameters:**
- `dac_switch`: Callback to control DAC/amplifier GPIO (GPIO_PIN_SET or GPIO_PIN_RESET)
- `read_volume`: Callback returning volume level 1-65535
- `i2s_init`: Callback to initialize I2S peripheral (e.g., `MX_I2S2_Init`)

**Returns:** `PB_Idle` on success, `PB_Error` if any callback is NULL

**Notes:**
- **Must be called once** before any playback functions
- Initializes all filter state and playback variables
- Sets up default filter configuration (overridable with `SetFilterConfig()`)
- Does not start audio playback

**Example:**
```c
PB_StatusTypeDef status = AudioEngine_Init(
  DAC_MasterSwitch,
  ReadVolume,
  MX_I2S2_Init
);

if (status != PB_Idle) {
  printf("Audio engine initialization failed!\n");
  return;
}
```

---

## Playback Control

### `PlaySample()`

Start playback of an audio sample.

```c
PB_StatusTypeDef PlaySample(
  const void *sample_to_play,     // Pointer to audio data (8-bit or 16-bit)
  uint32_t sample_set_sz,         // Total samples (all channels combined)
  uint32_t playback_speed,        // Sample rate in Hz (typically 22000)
  uint8_t sample_depth,           // 8 or 16 (bits per sample)
  PB_ModeTypeDef mode             // Mode_mono or Mode_stereo
);
```

**Parameters:**
- `sample_to_play`: Audio data pointer (must remain valid during playback)
- `sample_set_sz`: Total samples (all channels combined)
- `playback_speed`: Sample rate in Hz (22000 recommended, up to 48000)
- `sample_depth`: 8 or 16
- `mode`: `Mode_mono` or `Mode_stereo`

**Returns:** `PB_Playing` if successful, `PB_Error` on invalid parameters

**Notes:**
- Audio data must be in accessible memory (not discarded until playback completes)
- DMA directly accesses the buffer; no copy is made
- For 16-bit mono: `sample_set_sz = num_samples`
- For 16-bit stereo (interleaved): `sample_set_sz = 2 × num_frames`
- For 8-bit mono: `sample_set_sz = num_samples`
- For 8-bit stereo (interleaved): `sample_set_sz = 2 × num_frames`
- Briefly blocks while starting DMA

**Example:**
```c
extern const uint8_t doorbell_16bit_mono[];
extern const uint32_t doorbell_16bit_mono_samples;

PB_StatusTypeDef result = PlaySample(
  doorbell_16bit_mono,
  doorbell_16bit_mono_samples,
  22000,
  16,
  Mode_mono
);

if (result == PB_Playing) {
  WaitForSampleEnd();
}
```

### `CalcSampleOffsetSamples()`

Calculate a sample offset from time, sample rate, and playback mode.

```c
uint32_t CalcSampleOffsetSamples(
  float seconds,                 // Desired offset in seconds (>= 0)
  uint32_t sample_rate_hz,        // Sample rate in Hz
  PB_ModeTypeDef mode             // Mode_mono or Mode_stereo
);
```

**Parameters:**
- `seconds`: Desired offset in seconds (>= 0)
- `sample_rate_hz`: Sample rate in Hz
- `mode`: `Mode_mono` or `Mode_stereo`

**Returns:** Sample offset in interleaved samples

**Notes:**
- Returns 0 if `seconds <= 0` or `sample_rate_hz == 0`
- For stereo, returns 2x the mono sample count

**Example:**
```c
uint32_t offset = CalcSampleOffsetSamples(1.25f, 22000, Mode_stereo);
// offset is 55000 samples (1.25 * 22000 * 2)
```

### `WaitForSampleEnd()`

Block until playback completes.

```c
PB_StatusTypeDef WaitForSampleEnd(void);
```

**Returns:** `PB_Idle` when playback finishes, `PB_Error` if no playback active

**Notes:**
- Blocking call; use with caution in interrupt-sensitive contexts
- Safe to call even if no playback is active
- Blocks while playback is paused or pausing

### `PausePlayback()`

Pause active playback with smooth fade-out.

```c
PB_StatusTypeDef PausePlayback(void);
```

**Returns:** `PB_Paused` if successful, `PB_Error` if no playback active

**Notes:**
- Fade-out duration set by `SetPauseFadeTime()`
- Preserves playback position for resume
- Handles edge cases smoothly:
  - **Pausing during fade-in:** Scales pause fadeout to start from current volume level
  - **Pausing during end-of-file fadeout:** Scales pause fadeout proportionally to maintain smooth volume continuity
- All fade transitions use quadratic volume curves to prevent audible discontinuities
- Handles edge cases smoothly:
  - **Pausing during fade-in:** Scales pause fadeout to start from current volume level
  - **Pausing during end-of-file fadeout:** Scales pause fadeout proportionally to maintain smooth volume continuity
- All fade transitions use quadratic volume curves to prevent audible discontinuities

### `ResumePlayback()`

Resume previously paused playback with smooth fade-in.

```c
PB_StatusTypeDef ResumePlayback(void);
```

**Returns:** `PB_Playing` if successful, `PB_Error` if not paused

**Notes:**
- Fade-in duration set by `SetResumeFadeTime()`
- Resumes from the exact paused position
- Audio fades in smoothly from silence using quadratic curve

### `StopPlayback()`

Request asynchronous stop with normal end-of-play fade-out.

```c
PB_StatusTypeDef StopPlayback(void);
```

**Returns:** Current playback state, `PB_Idle` if already idle

**Notes:**
- Returns immediately (non-blocking)
- **Thread-safe:** Simply sets a flag; all stop logic runs in DMA callback context
- Uses the normal end-of-play fade-out duration
- DMA callback initiates fade-out and completes the stop when fade finishes
- Fade-out duration set by `SetFadeOutTime()`
- Use `GetPlaybackState()` to poll for completion
- `AudioEngine_OnPlaybackEnd()` callback invoked once when stop completes

**Example:**
```c
StopPlayback();  // Request stop (returns immediately)

// Wait for stop to complete
while (GetPlaybackState() != PB_Idle) {
  // Can do other work here while fading out
}
```

### `ShutDownAudio()`

Stop playback and disable amplifier.

```c
void ShutDownAudio(void);
```

**Notes:**
- Waits for DMA buffer to drain before stopping
- Calls `AudioEngine_DACSwitch(DAC_OFF)` if DAC control is enabled
- Safe to call during or after playback

**Example:**
```c
ShutDownAudio();
// Amplifier is now off and I2S is stopped
```

---

## DAC Power Control

### `SetDAC_Control()`

Enable or disable optional DAC power control during playback.

```c
void SetDAC_Control(uint8_t state);
```

**Parameters:**
- `state`: `1` to enable DAC power control, `0` to disable

**Notes:**
- When enabled, `AudioEngine_DACSwitch()` is called automatically during playback
- When disabled, DAC power management is handled externally by the application
- Default is enabled (`1`)
- Useful when sharing the audio engine across multiple applications with different power requirements

**Example:**
```c
// Disable automatic DAC control (manual control)
SetDAC_Control(0);

// Enable automatic DAC control (engine manages power)
SetDAC_Control(1);
```

### `GetDAC_Control()`

Query current DAC power control state.

```c
uint8_t GetDAC_Control(void);
```

**Returns:** `1` if DAC control is enabled, `0` if disabled

**Example:**
```c
if (GetDAC_Control()) {
  printf("DAC power control is automatic\n");
} else {
  printf("DAC power control is manual\n");
}
```

---

## Volume Response Control

Control how raw volume input values (0–65535) are mapped to actual audio gain. The audio engine supports two response modes:

- **Linear**: Direct proportional mapping (default when non-linear is disabled)
- **Non-linear (Gamma Curve)**: Perceptually uniform response matching human loudness perception

The gamma curve applies: `output = input^(1/gamma)`, where gamma = 2.0 (typical) provides a quadratic response curve.

### `SetVolumeResponseNonlinear()`

Enable or disable non-linear volume response curve.

```c
void SetVolumeResponseNonlinear( uint8_t enable );
```

**Parameters:**
- `enable`: 1 to enable non-linear (gamma curve) response, 0 for linear response

**Notes:**
- Non-linear response (enabled by default) matches human loudness perception for more intuitive volume control
- Linear response provides direct proportional scaling (input/65535 × max_gain)
- Change takes effect immediately on next sample processing

**Example:**
```c
// Enable non-linear (perceptual) volume response
SetVolumeResponseNonlinear(1);

// Switch to linear volume response
SetVolumeResponseNonlinear(0);
```

### `GetVolumeResponseNonlinear()`

Get current volume response mode.

```c
uint8_t GetVolumeResponseNonlinear( void );
```

**Returns:** 1 if non-linear mode enabled, 0 if linear mode

### `SetVolumeResponseGamma()`

Set the gamma exponent for non-linear volume response curve.

```c
void SetVolumeResponseGamma( float gamma );
```

**Parameters:**
- `gamma`: Gamma exponent (typical range: 1.0–4.0)
  - 1.0: Linear response (same as linear mode)
  - 2.0: Quadratic curve (recommended, matches human perception)
  - 3.0–4.0: More aggressive curve (flattens low volumes, steepens high volumes)

**Notes:**
- Higher gamma values create a more aggressive curve where small volume adjustments have smaller effect at low settings and larger effect at high settings
- Typical value: 2.0 (provides balanced perceptual uniformity)
- Change takes effect immediately on next sample processing
- Only used when non-linear mode is enabled

**Example:**
```c
// Set quadratic response (recommended)
SetVolumeResponseGamma(2.0f);

// Set cubic response (more aggressive)
SetVolumeResponseGamma(3.0f);

// Match human loudness perception preferences
SetVolumeResponseGamma(2.2f);
```

### `GetVolumeResponseGamma()`

Get current volume response gamma exponent.

```c
float GetVolumeResponseGamma( void );
```

**Returns:** Current gamma value (typical: 1.0–4.0)

### Volume Response Usage Example

```c
// Initialize with defaults (non-linear, gamma=2.0)
AudioEngine_Init(DAC_MasterSwitch, ReadVolume, MX_I2S2_Init);

// Enable non-linear response with custom gamma for specific perception
SetVolumeResponseNonlinear(1);           // Enable perceptual volume
SetVolumeResponseGamma(2.2f);            // Slightly more aggressive curve

// Optional: switch to linear for testing
// SetVolumeResponseNonlinear(0);        // Linear mode (direct scaling)

// Check current configuration
uint8_t is_nonlinear = GetVolumeResponseNonlinear();  // Returns 1
float current_gamma = GetVolumeResponseGamma();       // Returns 2.2f
```

---

## Filter Configuration

### `SetFilterConfig()`

Apply a complete filter configuration at once.

```c
void SetFilterConfig(const FilterConfig_TypeDef *cfg);
```

**Parameters:**
- `cfg`: Pointer to `FilterConfig_TypeDef` structure containing:
  - `enable_16bit_biquad_lpf`: 1 to enable 16-bit LPF, 0 to disable
  - `enable_8bit_lpf`: 1 to enable 8-bit one-pole LPF, 0 to disable
  - `enable_soft_dc_filter_16bit`: 1 to enable DC blocking, 0 to disable
  - `enable_noise_gate`: 1 to suppress low-level noise, 0 to disable
  - `enable_soft_clipping`: 1 to prevent distortion, 0 to disable
  - `enable_air_effect`: 1 to enable brightening, 0 to disable
  - `lpf_makeup_gain_q16`: Makeup gain after 8-bit LPF (Q16 format)
  - `lpf_makeup_gain_16bit_q16`: Makeup gain after 16-bit LPF (Q16 format)
  - `lpf_8bit_level`: Filter aggressiveness for 8-bit (LPF_VerySoft to LPF_Aggressive)
  - `lpf_16bit_level`: Filter aggressiveness for 16-bit (LPF_VerySoft to LPF_Aggressive)
  - `lpf_16bit_custom_alpha`: Custom alpha coefficient (used if `lpf_16bit_level = LPF_Custom`)
  - `lpf_8bit_custom_alpha`: Custom alpha coefficient (used if `lpf_8bit_level = LPF_Custom`)
  - `enable_filter_chain_16bit`: 1 to enable the full 16-bit filter chain, 0 to bypass
  - `enable_filter_chain_8bit`: 1 to enable the full 8-bit filter chain, 0 to bypass

**Example:**
```c
FilterConfig_TypeDef cfg = {
  .enable_16bit_biquad_lpf = 1,
  .enable_8bit_lpf = 1,
  .enable_soft_clipping = 1,
  .lpf_16bit_level = LPF_Soft,
  .lpf_16bit_custom_alpha = LPF_16BIT_SOFT,
  .lpf_8bit_level = LPF_Medium,
  .lpf_8bit_custom_alpha = LPF_MEDIUM,
  .lpf_makeup_gain_q16 = 65536,        // 1.0x 8-bit LPF gain
  .lpf_makeup_gain_16bit_q16 = 65536,  // 1.0x 16-bit LPF gain
  .enable_filter_chain_16bit = 1,
  .enable_filter_chain_8bit = 1
};
SetFilterConfig(&cfg);
```

### `GetFilterConfig()`

Retrieve the current filter configuration.

```c
void GetFilterConfig(FilterConfig_TypeDef *cfg);
```

**Parameters:**
- `cfg`: Pointer to structure to populate with current settings

**Example:**
```c
FilterConfig_TypeDef current_cfg;
GetFilterConfig(&current_cfg);

// Modify one field
current_cfg.enable_soft_clipping = 0;

// Apply changes
SetFilterConfig(&current_cfg);
```

### `SetSoftClippingEnable()`

Enable or disable soft clipping (distortion prevention).

```c
void SetSoftClippingEnable(uint8_t enabled);
```

**Parameters:**
- `enabled`: Non-zero to enable, zero to disable

### `GetSoftClippingEnable()`

Query soft clipping state.

```c
uint8_t GetSoftClippingEnable(void);
```

**Returns:** 1 if enabled, 0 if disabled

---

## Low-Pass Filter (LPF) Control

### 8-bit LPF Functions

#### `SetLpf8BitLevel()`

Set filter aggressiveness for 8-bit audio paths.

```c
void SetLpf8BitLevel(LPF_Level level);
```

**Parameters:**
- `level`: One of:
  - `LPF_VerySoft`: Minimal filtering (~3200 Hz cutoff)
  - `LPF_Soft`: Gentle filtering (~2800 Hz cutoff)
  - `LPF_Medium`: Balanced (~2300 Hz cutoff)
  - `LPF_Firm`: Firm filtering (~2000 Hz cutoff)
  - `LPF_Aggressive`: Strong filtering (~1800 Hz cutoff)
  - `LPF_Custom`: Use custom alpha (set via `SetLpf8BitCustomAlpha()`)
  - `LPF_Off`: Disable 8-bit LPF

**Notes:**
- Setting to `LPF_Off` disables the filter
- Setting any other level enables the filter
- 8-bit uses one-pole architecture (narrower alpha range than 16-bit)

#### `GetLpf8BitLevel()`

Query current 8-bit LPF aggressiveness.

```c
LPF_Level GetLpf8BitLevel(void);
```

**Returns:** Current `LPF_Level` setting

#### `SetLpf8BitCustomAlpha()`

Set a custom alpha coefficient for the 8-bit one-pole LPF.

```c
void SetLpf8BitCustomAlpha(uint16_t alpha);
```

**Parameters:**
- `alpha`: Q16 fixed-point coefficient (0-65535)

**Notes:**
- Automatically sets `lpf_8bit_level` to `LPF_Custom`

#### `GetLpf8BitCustomAlpha()`

Read the current 8-bit LPF custom alpha.

```c
uint16_t GetLpf8BitCustomAlpha(void);
```

**Returns:** Q16 alpha coefficient

**Notes:**
- Value is meaningful when `GetLpf8BitLevel()` returns `LPF_Custom`

#### `CalcLpf8BitAlphaFromCutoff()`

Compute an 8-bit LPF alpha from cutoff frequency and sample rate.

```c
uint16_t CalcLpf8BitAlphaFromCutoff(float cutoff_hz, float sample_rate_hz);
```

**Returns:** Q16 alpha coefficient

**Example:**
```c
uint16_t alpha = CalcLpf8BitAlphaFromCutoff(4500.0f, 11000.0f);
SetLpf8BitLevel(LPF_Custom);
SetLpf8BitCustomAlpha(alpha);
uint16_t current_alpha = GetLpf8BitCustomAlpha();
```

#### `SetLpfMakeupGain8Bit()`

Set makeup gain after 8-bit LPF (compensates for filter attenuation).

```c
void SetLpfMakeupGain8Bit(float gain);
```

**Parameters:**
- `gain`: Boost factor (0.1 to 2.0, clamped)

**Notes:**
- Typical range: 0.5x to 1.5x
- Higher values restore energy lost in filtering
 - Higher values restore energy lost in filtering

#### `SetLpfMakeupGain16Bit()`

Set makeup gain after 16-bit LPF (compensates for filter attenuation).

```c
void SetLpfMakeupGain16Bit(float gain);
```

**Parameters:**
- `gain`: Boost factor (0.1 to 2.0, clamped)

#### `GetLpfMakeupGain16Bit()`

Read current 16-bit LPF makeup gain.

```c
float GetLpfMakeupGain16Bit(void);
```

**Returns:** Current linear gain factor

### 16-bit LPF Functions

#### `SetLpf16BitLevel()`

Set filter aggressiveness for 16-bit audio paths.

```c
void SetLpf16BitLevel(LPF_Level level);
```

**Parameters:**
- `level`: One of:
  - `LPF_VerySoft`: Lightest filtering (α ≈ 0.625)
  - `LPF_Soft`: Gentle filtering (α ≈ 0.80)
  - `LPF_Medium`: Balanced (α ≈ 0.875)
  - `LPF_Firm`: Firm filtering (α ≈ 0.92)
  - `LPF_Aggressive`: Strong filtering (α ≈ 0.97)
  - `LPF_Custom`: Use custom alpha set by `SetLpf16BitCustomAlpha()`
  - `LPF_Off`: Disable 16-bit LPF

**Notes:**
- 16-bit uses biquad architecture (higher alpha = more filtering)
- Setting to any non-Off level enables the filter

#### `SetLpf16BitCustomAlpha()`

Set custom alpha coefficient for 16-bit LPF (advanced).

```c
void SetLpf16BitCustomAlpha(uint16_t alpha);
```

**Parameters:**
- `alpha`: Q16 fixed-point value (0 to 65535)
  - 0.0 (off) = 0
  - 0.5 ≈ 32768
  - 1.0 (full) = 65535

**Notes:**
- Only used when `SetLpf16BitLevel(LPF_Custom)` is set
- Typical range: 40000 to 64000

**Example:**
```c
// Set custom alpha for ~4 kHz cutoff @ 22 kHz
uint16_t custom_alpha = 50000;  // Approximate
SetLpf16BitLevel(LPF_Custom);
SetLpf16BitCustomAlpha(custom_alpha);
```

#### `CalcLpf16BitAlphaFromCutoff()`

Calculate alpha from desired cutoff frequency and sample rate (helper).

```c
uint16_t CalcLpf16BitAlphaFromCutoff(
  float cutoff_hz,
  float sample_rate_hz
);
```

**Parameters:**
- `cutoff_hz`: Desired -3dB cutoff frequency in Hz
- `sample_rate_hz`: Sample rate in Hz

**Returns:** Q16 alpha coefficient

**Notes:**
- Uses formula: `alpha = exp(-2π·fc/fs)`
- Clamped to safe range (0.0 to 0.99998)

**Example:**
```c
// Calculate alpha for 5 kHz cutoff @ 22 kHz
uint16_t alpha = CalcLpf16BitAlphaFromCutoff(5000.0f, 22000.0f);
SetLpf16BitLevel(LPF_Custom);
SetLpf16BitCustomAlpha(alpha);
```

#### `GetLpf16BitCustomAlphaFromCutoff()`

Calculate alpha using current playback speed.

```c
uint16_t GetLpf16BitCustomAlphaFromCutoff(float cutoff_hz);
```

**Parameters:**
- `cutoff_hz`: Desired -3dB cutoff in Hz

**Returns:** Q16 alpha coefficient (using `I2S_PlaybackSpeed` as sample rate)

**Example:**
```c
uint16_t alpha = GetLpf16BitCustomAlphaFromCutoff(4000.0f);
SetLpf16BitLevel(LPF_Custom);
SetLpf16BitCustomAlpha(alpha);
```

---

## Air Effect Control

### `SetAirEffectEnable()`

Enable or disable the air effect (high-shelf brightening).

```c
void SetAirEffectEnable(uint8_t enabled);
```

**Parameters:**
- `enabled`: Non-zero to enable, zero to disable

**Notes:**
- Automatically called by `SetAirEffectPresetDb()`
- Can be called directly for manual control

### `GetAirEffectEnable()`

Query air effect state.

```c
uint8_t GetAirEffectEnable(void);
```

**Returns:** 1 if enabled, 0 if disabled

### `SetAirEffectGainQ16()`

Set air effect shelf gain using Q16 fixed-point format (advanced).

```c
void SetAirEffectGainQ16(uint32_t gain_q16);
```

**Parameters:**
- `gain_q16`: Shelf gain in Q16 format
  - 65536 = 1.0x (unity gain)
  - 98304 ≈ 1.5x (default, ≈ +1.6 dB)
  - 131072 = 2.0x (max safe, ≈ +6 dB)

**Notes:**
- Clamped to `AIR_EFFECT_SHELF_GAIN_MAX`
- Prefer `SetAirEffectGainDb()` for dB-based control

**Example:**
```c
// Set to 1.5x gain (moderate brightening)
SetAirEffectGainQ16(98304);
SetAirEffectEnable(1);
```

### `GetAirEffectGainQ16()`

Read current air effect gain in Q16 format.

```c
uint32_t GetAirEffectGainQ16(void);
```

**Returns:** Current gain in Q16 format

### `SetAirEffectGainDb()`

Set air effect shelf gain using dB (recommended).

```c
void SetAirEffectGainDb(float db);
```

**Parameters:**
- `db`: Desired boost in dB (positive = brightening, clamped to safe range)

**Notes:**
- Converts dB to Q16 internally
- Typical range: 0.0 to +3.0 dB
- Safer and more intuitive than Q16 control

**Example:**
```c
SetAirEffectGainDb(2.0f);  // +2 dB boost
SetAirEffectEnable(1);
```

### `GetAirEffectGainDb()`

Read current air effect gain in dB.

```c
float GetAirEffectGainDb(void);
```

**Returns:** Current gain in dB

### `SetAirEffectPresetDb()`

Select and apply an air effect preset by index (recommended usage).

```c
void SetAirEffectPresetDb(uint8_t preset_index);
```

**Parameters:**
- `preset_index`: 
  - `0`: Disable air effect
  - `1`: +1 dB preset
  - `2`: +2 dB preset
  - `3`: +3 dB preset
  - Out-of-range: Clamps to 0

**Notes:**
- **Auto-enables/disables**: Preset 0 = off, preset > 0 = on
- No separate `SetAirEffectEnable()` call needed

**Example:**
```c
SetAirEffectPresetDb(2);  // Enable +2 dB preset
// ... playback ...
SetAirEffectPresetDb(0);  // Disable air effect
```

### `CycleAirEffectPresetDb()`

Cycle to the next preset (useful for buttons/menus).

```c
uint8_t CycleAirEffectPresetDb(void);
```

**Returns:** New preset index (0, 1, 2, 3, then wraps to 0)

**Example:**
```c
// User presses "Air Effect" button
uint8_t new_preset = CycleAirEffectPresetDb();
printf("Air preset: %d\n", new_preset);
```

### `GetAirEffectPresetIndex()`

Query current preset index.

```c
uint8_t GetAirEffectPresetIndex(void);
```

**Returns:** Current preset index (0–3)

### `GetAirEffectPresetCount()`

Get total number of available presets.

```c
uint8_t GetAirEffectPresetCount(void);
```

**Returns:** Number of presets (typically 4: off + 3 presets)

### `GetAirEffectPresetDb()`

Read the dB value of a specific preset.

```c
float GetAirEffectPresetDb(uint8_t preset_index);
```

**Parameters:**
- `preset_index`: Preset to query (0–3, out-of-range clamps to current)

**Returns:** dB value of the preset

**Example:**
```c
for (uint8_t i = 0; i < GetAirEffectPresetCount(); i++) {
  float db = GetAirEffectPresetDb(i);
  printf("Preset %d: %.1f dB\n", i, db);
}
// Output:
// Preset 0: 0.0 dB    (disabled)
// Preset 1: 1.0 dB
// Preset 2: 2.0 dB
// Preset 3: 3.0 dB
```

---

## Fade Time Control

### `SetFadersEnabled()`

Enable or disable fade-in/fade-out processing.

```c
void SetFadersEnabled(uint8_t fader_setting);
```

**Parameters:**
- `fader_setting`: 1 to enable faders, 0 to bypass

### `GetFadersEnabled()`

Query current fader enable state.

```c
uint8_t GetFadersEnabled(void);
```

**Returns:** 1 if faders are enabled, 0 if disabled

### `SetFadeInTime()`

Set the duration of fade-in effects (start of playback, resume).

```c
void SetFadeInTime(float seconds);
```

**Parameters:**
- `seconds`: Duration in seconds (clamped to 0.001–5.0)

**Notes:**
- Automatically recalculated when playback speed changes
- Default: ~0.150 seconds (150 ms)

### `GetFadeInTime()`

Query current fade-in time.

```c
float GetFadeInTime(void);
```

**Returns:** Fade-in duration in seconds

### `SetFadeOutTime()`

Set the duration of fade-out effects (end of playback, pause).

```c
void SetFadeOutTime(float seconds);
```

**Parameters:**
- `seconds`: Duration in seconds (clamped to 0.001–5.0)

**Notes:**
- Default: ~0.150 seconds (150 ms)

### `GetFadeOutTime()`

Query current fade-out time.

```c
float GetFadeOutTime(void);
```

**Returns:** Fade-out duration in seconds

### `SetPauseFadeTime()`

Set the duration of fade-out when pausing playback.

```c
void SetPauseFadeTime(float seconds);
```

**Parameters:**
- `seconds`: Duration in seconds (clamped to 0.001–5.0)

**Notes:**
- Separate from main fade-out; typically longer for smooth pause
- Default: ~1.0 seconds (1000 ms)
- Applied intelligently when pausing:
  - During fade-in: Scaled proportionally to current volume
  - During end-of-file fadeout: Scaled to maintain continuity from current level
- Applied intelligently when pausing:
  - During fade-in: Scaled proportionally to current volume
  - During end-of-file fadeout: Scaled to maintain continuity from current level

### `GetPauseFadeTime()`

Query current pause fade-out duration.

```c
float GetPauseFadeTime(void);
```

**Returns:** Pause fade-out duration in seconds

### `SetResumeFadeTime()`

Set the duration of fade-in when resuming playback.

```c
void SetResumeFadeTime(float seconds);
```

**Parameters:**
- `seconds`: Duration in seconds (clamped to 0.001–5.0)

**Notes:**
- Applied when `ResumePlayback()` is called
- Default: ~0.100 seconds (100 ms)

### `GetResumeFadeTime()`

Query current resume fade-in time.

```c
float GetResumeFadeTime(void);
```

**Returns:** Resume fade-in duration in seconds

**Example:**
```c
// Customize fade times for a smooth experience
SetFadeInTime(0.200f);      // 200 ms fade in
SetFadeOutTime(0.150f);     // 150 ms fade out
SetPauseFadeTime(0.300f);   // 300 ms pause fade
SetResumeFadeTime(0.200f);  // 200 ms resume fade

PlaySample(sample_data, size, 22000, 16, Mode_mono);
```

---

## Chunk Processing (DMA Callbacks)

These functions are called from I2S DMA interrupt handlers. **Do not call directly from application code.**

### `ProcessNextWaveChunk()`

Process one chunk of 16-bit samples from DMA buffer.

```c
PB_StatusTypeDef ProcessNextWaveChunk(int16_t *chunk_p);
```

**Parameters:**
- `chunk_p`: Pointer to 16-bit sample chunk

**Returns:** `PB_Playing` if playback continues, `PB_Idle` if complete

**Called by:** I2S DMA complete callback (`HAL_I2S_TxCpltCallback`)

### `ProcessNextWaveChunk_8_bit()`

Process one chunk of 8-bit samples from DMA buffer.

```c
PB_StatusTypeDef ProcessNextWaveChunk_8_bit(uint8_t *chunk_p);
```

**Parameters:**
- `chunk_p`: Pointer to 8-bit sample chunk

**Returns:** `PB_Playing` if playback continues, `PB_Idle` if complete

**Called by:** I2S DMA complete callback for 8-bit audio

### `AdvanceSamplePointer()`

Advance internal sample pointer to next chunk.

```c
void AdvanceSamplePointer(void);
```

**Notes:**
- Called automatically by chunk processing functions
- Internal use only

---

## Application Callbacks

### `AudioEngine_OnPlaybackEnd()`

Weak callback invoked when playback ends naturally or via `StopPlayback()`.

```c
void AudioEngine_OnPlaybackEnd(void);
```

**Notes:**
- **Weak symbol** - default implementation does nothing
- Override in your application to receive playback end notifications
- Called from **ISR context** (DMA callback) - keep implementation short and non-blocking
- **Invoked exactly once per playback session** - internal guard prevents repeated calls
- Triggered when:
  - Sample reaches end naturally
  - `StopPlayback()` completes its fade-out
  - `StopPlayback()` called while paused (immediate stop)
- Perfect for:
  - Setting event flags
  - Posting to RTOS queues
  - Triggering state machine transitions
  - Starting next sample in a playlist

**Restrictions:**
- Do NOT call blocking functions (delays, mutexes, etc.)
- Do NOT call complex audio engine functions
- Keep execution time under 10 microseconds
- Use volatile flags or RTOS primitives for communication

**Example (Simple Flag):**
```c
volatile uint8_t playback_complete = 0;

// Override the weak callback
void AudioEngine_OnPlaybackEnd(void)
{
  playback_complete = 1;  // Set flag for main loop
}

// In main loop:
void main(void)
{
  PlaySample(sample, size, 22000, 16, Mode_mono);
  
  while (!playback_complete) {
    // Do other work
    UpdateDisplay();
    CheckButtons();
  }
  
  printf("Playback finished!\n");
}
```

**Example (RTOS Event):**
```c
extern osEventFlagsId_t audio_events;

void AudioEngine_OnPlaybackEnd(void)
{
  osEventFlagsSet(audio_events, AUDIO_DONE_FLAG);
}
```

**Example (Playlist):**
```c
volatile uint8_t current_track = 0;
const uint8_t max_tracks = 5;

void AudioEngine_OnPlaybackEnd(void)
{
  current_track++;
  if (current_track < max_tracks) {
    // Signal main loop to start next track
    next_track_ready = 1;
  }
}
```

---

## Status & Queries

### `GetPlaybackState()`

Query current playback status (external declaration).

```c
uint8_t GetPlaybackState(void);
```

**Returns:** One of:
- `PB_Idle`: No playback active
- `PB_Playing`: Playback in progress
- `PB_Paused`: Paused
- `PB_Error`: Error state

**Example:**
```c
if (GetPlaybackState() == PB_Playing) {
  printf("Audio is playing...\n");
}
```

### `GetPlaybackSpeed()`

Query current playback sample rate.

```c
uint32_t GetPlaybackSpeed(void);
```

**Returns:** Sample rate in Hz (typically 22000)

**Notes:**
- Set by most recent `PlaySample()` call
- Used internally for fade time calculations

**Example:**
```c
uint32_t sr = GetPlaybackSpeed();
printf("Playback speed: %lu Hz\n", sr);
```

---

## Complete Example: Interactive Audio Control

```c
#include "audio_engine.h"

void demo_interactive_control(void) {
  // Initialize
  AudioEngine_Init(DAC_MasterSwitch, ReadVolume, MX_I2S2_Init);
  
  // Configure filters
  FilterConfig_TypeDef cfg;
  GetFilterConfig(&cfg);
  cfg.enable_16bit_biquad_lpf = 1;
  cfg.enable_soft_clipping = 1;
  SetFilterConfig(&cfg);
  
  // Load audio
  extern const uint8_t my_sound[];
  extern const uint32_t my_sound_size;
  
  // Play
  PlaySample(my_sound, my_sound_size, 22000, 16, Mode_mono);
  
  // User controls (e.g., in button ISR)
  HAL_Delay(500);
  
  // Pause
  printf("Pausing...\n");
  PausePlayback();
  HAL_Delay(1000);
  
  // Resume with custom fade
  SetResumeFadeTime(0.500f);
  printf("Resuming...\n");
  ResumePlayback();
  HAL_Delay(1000);
  
  // Cycle air effect presets
  for (int i = 0; i < 4; i++) {
    printf("Air preset: %u (%.1f dB)\n", 
         GetAirEffectPresetIndex(), 
         GetAirEffectGainDb());
    CycleAirEffectPresetDb();
    HAL_Delay(500);
  }
  
  // Wait for completion
  WaitForSampleEnd();
  
  // Shutdown
  ShutDownAudio();
  printf("Done\n");
}
```

---

## Function Index (Alphabetical)

| Function                             | Category   | Purpose                               |
|--------------------------------------|------------|---------------------------------------|
| `AdvanceSamplePointer()`             | Internal   | Advance to next sample chunk          |
| `AudioEngine_Init()`                 | Init       | Initialize audio engine               |
| `CalcLpf16BitAlphaFromCutoff()`      | LPF        | Calculate alpha from cutoff frequency |
| `CycleAirEffectPresetDb()`           | Air Effect | Cycle to next air effect preset       |
| `GetAirEffectEnable()`               | Air Effect | Query air effect enabled state        |
| `GetAirEffectGainDb()`               | Air Effect | Read air effect gain in dB            |
| `GetAirEffectGainQ16()`              | Air Effect | Read air effect gain in Q16 format    |
| `GetAirEffectPresetCount()`          | Air Effect | Get number of available presets       |
| `GetAirEffectPresetDb()`             | Air Effect | Get dB value of a preset              |
| `GetAirEffectPresetIndex()`          | Air Effect | Get current preset index              |
| `GetDAC_Control()`                   | DAC        | Query DAC power control state         |
| `GetFadeInTime()`                    | Fade       | Get fade-in duration                  |
| `GetFadeOutTime()`                   | Fade       | Get fade-out duration                 |
| `GetFilterConfig()`                  | Filter     | Get complete filter configuration     |
| `GetLpf16BitCustomAlphaFromCutoff()` | LPF        | Calculate 16-bit alpha from cutoff    |
| `GetLpf8BitLevel()`                  | LPF        | Get 8-bit filter aggressiveness       |
| `GetPauseFadeTime()`                 | Fade       | Get pause fade-out duration           |
| `GetPlaybackSpeed()`                 | Status     | Get current playback sample rate      |
| `GetPlaybackState()`                 | Status     | Get current playback state            |
| `GetResumeFadeTime()`                | Fade       | Get resume fade-in duration           |
| `GetSoftClippingEnable()`            | Filter     | Query soft clipping state             |

| `PausePlayback()`                    | Playback   | Pause playback with fade              |
| `PlaySample()`                       | Playback   | Start playback of audio sample        |
| `ProcessNextWaveChunk()`             | Internal   | Process 16-bit sample chunk           |
| `ProcessNextWaveChunk_8_bit()`       | Internal   | Process 8-bit sample chunk            |
| `ResumePlayback()`                   | Playback   | Resume paused playback with fade      |
| `SetAirEffectEnable()`               | Air Effect | Enable/disable air effect             |
| `SetAirEffectGainDb()`               | Air Effect | Set air effect gain in dB             |
| `SetAirEffectGainQ16()`              | Air Effect | Set air effect gain in Q16 format     |
| `SetAirEffectPresetDb()`             | Air Effect | Select air effect preset by index     |
| `SetDAC_Control()`                   | DAC        | Enable/disable DAC power control      |
| `SetFadeInTime()`                    | Fade       | Set fade-in duration                  |
| `SetFadeOutTime()`                   | Fade       | Set fade-out duration                 |
| `SetFilterConfig()`                  | Filter     | Apply complete filter configuration   |
| `SetLpf16BitCustomAlpha()`           | LPF        | Set custom 16-bit LPF alpha           |
| `SetLpf16BitLevel()`                 | LPF        | Set 16-bit filter aggressiveness      |
| `SetLpf8BitCustomAlpha()`            | LPF        | Set custom 8-bit LPF alpha            |
| `GetLpf8BitCustomAlpha()`            | LPF        | Get custom 8-bit LPF alpha            |
| `SetLpf8BitLevel()`                  | LPF        | Set 8-bit filter aggressiveness       |
| `SetLpfMakeupGain8Bit()`             | LPF        | Set 8-bit LPF makeup gain             |
| `SetLpfMakeupGain16Bit()`            | LPF        | Set 16-bit LPF makeup gain            |
| `GetLpfMakeupGain16Bit()`            | LPF        | Get 16-bit LPF makeup gain            |
| `SetFadersEnabled()`                 | Fade       | Enable/disable faders                 |
| `GetFadersEnabled()`                 | Fade       | Get fader enable state                |
| `CalcLpf8BitAlphaFromCutoff()`       | LPF        | Calculate 8-bit LPF alpha from cutoff |
| `SetPauseFadeTime()`                 | Fade       | Set pause fade-out duration           |
| `SetResumeFadeTime()`                | Fade       | Set resume fade-in duration           |
| `SetSoftClippingEnable()`            | Filter     | Enable/disable soft clipping          |
| `StopPlayback()`                     | Playback   | Request asynchronous stop with fade   |
| `ShutDownAudio()`                    | Playback   | Stop playback and disable amplifier   |
| `WaitForSampleEnd()`                 | Playback   | Block until playback completes        |
| `AudioEngine_OnPlaybackEnd()`        | Callback   | Weak callback for playback end event  |
