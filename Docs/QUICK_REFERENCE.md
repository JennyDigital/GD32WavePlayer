# Audio Engine - Quick Reference Card

Fast lookup for common audio engine operations.

## Initialization

```c
#include "audio_engine.h"

AudioEngine_Init(DAC_MasterSwitch, ReadVolume, MX_I2S2_Init);
```

## Playback

```c
// Play a sample (blocking until complete)
PlaySample(sample_ptr, sample_count, 22000, 16, Mode_mono);
WaitForSampleEnd();

// Play non-blocking
PlaySample(sample_ptr, sample_count, 22000, 16, Mode_mono);
// Do other work...
if (GetPlaybackState() == PB_Playing) { /* still playing */ }

// Pause/Resume
PausePlayback();      // Fade out and pause
ResumePlayback();     // Fade in and resume

// Stop with fade (asynchronous)
StopPlayback();       // Request stop (returns immediately)
while (GetPlaybackState() != PB_Idle) { /* wait for fade/stop to complete */ }

// Stop all audio
ShutDownAudio();
```

## DAC Power Control

```c
// Enable/disable automatic DAC power control
SetDAC_Control(1);  // Engine manages DAC power (default)
SetDAC_Control(0);  // Manual DAC control

// Query current state
if (GetDAC_Control()) {
  // DAC power is automatic
}
```

## Application Callbacks

```c
// Override this weak function to get playback end notifications
void AudioEngine_OnPlaybackEnd(void)
{
  // Called from ISR when playback ends
  playback_done_flag = 1;  // Set flag for main loop
}

// Usage patterns:
// 1. Simple flag
volatile uint8_t done = 0;
void AudioEngine_OnPlaybackEnd(void) { done = 1; }

// 2. RTOS event
void AudioEngine_OnPlaybackEnd(void) {
  osEventFlagsSet(audio_events, AUDIO_DONE);
}

// 3. Playlist management
void AudioEngine_OnPlaybackEnd(void) {
  next_track_ready = 1;
}
```

## Filter Configuration

```c
// Set by aggressiveness level
SetLpf16BitLevel(LPF_Soft);      // Very Soft, Soft, Medium, Firm, Aggressive
SetLpf8BitLevel(LPF_Medium);

// Query current settings
FilterConfig_TypeDef cfg;
GetFilterConfig(&cfg);
cfg.enable_soft_clipping = 1;
SetFilterConfig(&cfg);

// Get/Set soft clipping
GetSoftClippingEnable();
SetSoftClippingEnable(1);

// Makeup gain after filtering
SetLpfMakeupGain8Bit(1.15f);     // 15% boost
SetLpfMakeupGain16Bit(1.05f);    // 5% boost
GetLpfMakeupGain16Bit();
```

## Air Effect (Brightening)

```c
// Easiest: Use presets
SetAirEffectPresetDb(0);  // Off
SetAirEffectPresetDb(1);  // +1 dB
SetAirEffectPresetDb(2);  // +2 dB (recommended)
SetAirEffectPresetDb(3);  // +3 dB

// Cycle through presets
CycleAirEffectPresetDb();

// Manual dB control
SetAirEffectGainDb(1.5f);
GetAirEffectGainDb();

// Get preset info
GetAirEffectPresetIndex();      // Current preset
GetAirEffectPresetCount();      // Number of presets
GetAirEffectPresetDb(preset_i); // dB of specific preset
```

## Fade Times

```c
// Set fade durations (in seconds)
SetFadeInTime(0.150f);      // Play start fade
SetFadeOutTime(0.150f);     // Play end fade
SetPauseFadeTime(0.300f);   // Pause fade out
SetResumeFadeTime(0.200f);  // Resume fade in

// Get current values
GetFadeInTime();
GetFadeOutTime();
GetPauseFadeTime();
GetResumeFadeTime();

// Enable/disable faders
SetFadersEnabled(1);
GetFadersEnabled();
```

## Custom LPF Alpha

```c
// Calculate alpha from cutoff frequency
uint16_t alpha = GetLpf16BitCustomAlphaFromCutoff(5000.0f);  // 5 kHz

// Or with custom sample rate
uint16_t alpha = CalcLpf16BitAlphaFromCutoff(5000.0f, 22000.0f);

// Apply custom alpha
SetLpf16BitLevel(LPF_Custom);
SetLpf16BitCustomAlpha(alpha);

// 8-bit custom alpha
uint16_t alpha8 = CalcLpf8BitAlphaFromCutoff(4500.0f, 11000.0f);
SetLpf8BitLevel(LPF_Custom);
SetLpf8BitCustomAlpha(alpha8);
GetLpf8BitCustomAlpha();
```

## Status Queries

```c
GetPlaybackState();    // PB_Idle, PB_Playing, PB_Paused, PB_Error
GetPlaybackSpeed();    // Current sample rate (Hz)
```

## Volume Control

```c
// Read volume (1-65535 scale, non-linear if enabled)
uint16_t vol = ReadVolume();

// Non-linear response (logarithmic) in main.h:
#define VOLUME_RESPONSE_NONLINEAR
#define VOLUME_RESPONSE_GAMMA 2.0f  // 1.0=linear, 2.0=quadratic
```

## Common Patterns

### Pattern 1: Simple Play & Wait
```c
PlaySample(sound_data, sound_size, 22000, 16, Mode_mono);
WaitForSampleEnd();
```

### Pattern 2: Non-Blocking with Poll
```c
PlaySample(sound_data, sound_size, 22000, 16, Mode_mono);
while (GetPlaybackState() == PB_Playing) {
  // Do other work
  HAL_Delay(100);
}
```

### Pattern 3: Pause & Resume
```c
PlaySample(sound_data, sound_size, 22000, 16, Mode_mono);
HAL_Delay(1000);
PausePlayback();
HAL_Delay(2000);
ResumePlayback();
WaitForSampleEnd();
```

### Pattern 4: Configure Then Play
```c
FilterConfig_TypeDef cfg;
GetFilterConfig(&cfg);
cfg.enable_16bit_biquad_lpf = 1;
cfg.enable_soft_clipping = 1;
cfg.lpf_16bit_level = LPF_Soft;
SetFilterConfig(&cfg);

SetAirEffectPresetDb(2);      // +2 dB brightening
SetFadeInTime(0.200f);

PlaySample(sound_data, sound_size, 22000, 16, Mode_mono);
WaitForSampleEnd();
```

### Pattern 5: Interactive Control
```c
// On button press
CycleAirEffectPresetDb();
printf("Preset: %u\n", GetAirEffectPresetIndex());

// On slider change
SetFadeOutTime(slider_value / 100.0f);  // 0.0-1.0 seconds
```

### Pattern 6: Event-Driven Playback
```c
volatile uint8_t playback_done = 0;

void AudioEngine_OnPlaybackEnd(void) {
  playback_done = 1;  // Called from ISR when done
}

void main_loop(void) {
  PlaySample(sound_data, sound_size, 22000, 16, Mode_mono);
  
  // Continue other work while audio plays
  while (!playback_done) {
    UpdateDisplay();
    ProcessButtons();
    HAL_Delay(10);
  }
  
  printf("Audio finished!\n");
}
```

## Function Categories

**Playback** (6): `PlaySample`, `WaitForSampleEnd`, `PausePlayback`, `ResumePlayback`, `StopPlayback`, `ShutDownAudio`

**DAC Control** (2): `SetDAC_Control`, `GetDAC_Control`

**Callbacks** (1): `AudioEngine_OnPlaybackEnd`

**Configuration** (2): `SetFilterConfig`, `GetFilterConfig`

**Soft Clipping** (2): `SetSoftClippingEnable`, `GetSoftClippingEnable`

**8-bit LPF** (5): `SetLpf8BitLevel`, `GetLpf8BitLevel`, `SetLpf8BitCustomAlpha`, `GetLpf8BitCustomAlpha`, `CalcLpf8BitAlphaFromCutoff`

**16-bit LPF** (4): `SetLpf16BitLevel`, `SetLpf16BitCustomAlpha`, `CalcLpf16BitAlphaFromCutoff`, `GetLpf16BitCustomAlphaFromCutoff`

**Air Effect** (11): `SetAirEffectEnable`, `GetAirEffectEnable`, `SetAirEffectGainQ16`, `GetAirEffectGainQ16`, `SetAirEffectGainDb`, `GetAirEffectGainDb`, `SetAirEffectPresetDb`, `CycleAirEffectPresetDb`, `GetAirEffectPresetIndex`, `GetAirEffectPresetCount`, `GetAirEffectPresetDb`

**Fade Times** (8): `SetFadeInTime`, `GetFadeInTime`, `SetFadeOutTime`, `GetFadeOutTime`, `SetPauseFadeTime`, `GetPauseFadeTime`, `SetResumeFadeTime`, `GetResumeFadeTime`

**Faders** (2): `SetFadersEnabled`, `GetFadersEnabled`

**Makeup Gain** (3): `SetLpfMakeupGain8Bit`, `SetLpfMakeupGain16Bit`, `GetLpfMakeupGain16Bit`

**Status** (2): `GetPlaybackState`, `GetPlaybackSpeed`

**Volume Response** (4): `SetVolumeResponseNonlinear`, `GetVolumeResponseNonlinear`, `SetVolumeResponseGamma`, `GetVolumeResponseGamma`

---

## See Also

- **Full API Reference**: [API_REFERENCE.md](API_REFERENCE.md) - Complete documentation for all 60+ functions
- **User Manual**: [AUDIO_ENGINE_MANUAL.md](AUDIO_ENGINE_MANUAL.md) - Architecture, examples, troubleshooting
- **Air Effect Guide**: [AIR_EFFECT_QUICK_REFERENCE.md](AIR_EFFECT_QUICK_REFERENCE.md) - Air effect details
