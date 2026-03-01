# Audio Engine Function Index

Complete alphabetical index of all 60+ public functions in the Audio Engine API, organized by category with quick descriptions.

<!-- Anchor targets for internal links -->
<a id="advancesamplepointer"></a>
<a id="audioengine_init"></a>
<a id="audioengine_onplaybackend"></a>
<a id="calclpf16bitalphafromotoff"></a>
<a id="calclpf8bitalphafromcutoff"></a>
<a id="calcsampleoffsetsamples"></a>
<a id="cycleaireffectpresetdb"></a>
<a id="getaireffectenable"></a>
<a id="getaireffectgaindb"></a>
<a id="getaireffectgainq16"></a>
<a id="getaireffectpresetcount"></a>
<a id="getaireffectpresetdb"></a>
<a id="getaireffectpresetindex"></a>
<a id="getdac_control"></a>
<a id="getfadeintime"></a>
<a id="getfadeouttime"></a>
<a id="getfadersenabled"></a>
<a id="getfilterconfig"></a>
<a id="gethalftofill"></a>
<a id="getlpf8bitcustomalpha"></a>
<a id="getlpf8bitlevel"></a>
<a id="getlpf16bitcustomalphafromotoff"></a>
<a id="getlpfmakeupgain16bit"></a>
<a id="getpausefadetime"></a>
<a id="getplaybackspeed"></a>
<a id="getplaybackstate"></a>
<a id="getresumefadetime"></a>
<a id="getsoftclippingenable"></a>
<a id="getvolumeresponsegamma"></a>
<a id="getvolumeresponsenonlinear"></a>
<a id="hal_i2s_txcpltcallback"></a>
<a id="hal_i2s_txhalfcpltcallback"></a>
<a id="pauseplayback"></a>
<a id="playsample"></a>
<a id="processnextwavechunk"></a>
<a id="processnextwavechunk_8_bit"></a>
<a id="resumeplayback"></a>
<a id="setaireffectenable"></a>
<a id="setaireffectgaindb"></a>
<a id="setaireffectgainq16"></a>
<a id="setdac_control"></a>
<a id="setfadeintime"></a>
<a id="setfadeouttime"></a>
<a id="setfadersenabled"></a>
<a id="setfilterconfig"></a>
<a id="sethalftofill"></a>
<a id="setlpf8bitcustomalpha"></a>
<a id="setlpf8bitlevel"></a>
<a id="setlpf16bitcustomalpha"></a>
<a id="setlpf16bitlevel"></a>
<a id="setlpfmakeupgain8bit"></a>
<a id="setlpfmakeupgain16bit"></a>
<a id="setpausefadetime"></a>
<a id="setplaybackspeed"></a>
<a id="setplaybackstate"></a>
<a id="setresumefadetime"></a>
<a id="setsoftclippingenable"></a>
<a id="setvolumeresponsegamma"></a>
<a id="setvolumeresponsenonlinear"></a>
<a id="shutdownaudio"></a>
<a id="stopplayback"></a>
<a id="waitforsampleend"></a>

## 📑 Alphabetical Index

| Function | Category | Description |
|----------|----------|-------------|
| [`AdvanceSamplePointer()`](#advancesamplepointer) | Chunk Processing | Advance to next DMA buffer position |
| [`AudioEngine_Init()`](#audioengine_init) | Initialization | Initialize engine with hardware callbacks |
| [`AudioEngine_OnPlaybackEnd()`](#audioengine_onplaybackend) | Callback | Weak callback for playback end event |
| [`CalcLpf16BitAlphaFromCutoff()`](#calclpf16bitalphafromotoff) | 16-bit LPF | Calculate alpha from cutoff frequency |
| [`CalcLpf8BitAlphaFromCutoff()`](#calclpf8bitalphafromcutoff) | 8-bit LPF | Calculate alpha from cutoff frequency |
| [`CalcSampleOffsetSamples()`](#calcsampleoffsetsamples) | Playback Control | Calculate sample offset from time, rate, and mode |
| [`CycleAirEffectPresetDb()`](#cycleaireffectpresetdb) | Air Effect | Cycle through air effect presets |
| [`GetAirEffectEnable()`](#getaireffectenable) | Air Effect | Read air effect enable state |
| [`GetAirEffectGainDb()`](#getaireffectgaindb) | Air Effect | Get air effect gain in dB |
| [`GetAirEffectGainQ16()`](#getaireffectgainq16) | Air Effect | Get air effect gain in Q16 format |
| [`GetAirEffectPresetCount()`](#getaireffectpresetcount) | Air Effect | Get number of available presets |
| [`GetAirEffectPresetDb()`](#getaireffectpresetdb) | Air Effect | Get dB value for a preset index |
| [`GetAirEffectPresetIndex()`](#getaireffectpresetindex) | Air Effect | Get current preset index |
| [`GetDAC_Control()`](#getdac_control) | DAC Control | Read DAC power control state |
| [`GetFadeInTime()`](#getfadeintime) | Fade Time | Get fade-in duration in seconds |
| [`GetFadeOutTime()`](#getfadeouttime) | Fade Time | Get fade-out duration in seconds |
| [`GetFadersEnabled()`](#getfadersenabled) | Fade Time | Get fader enable state |
| [`GetFilterConfig()`](#getfilterconfig) | Filter Config | Read current filter configuration |
| [`GetHalfToFill()`](#gethalftofill) | Internal State | Get which buffer half to fill next |
| [`GetLpf8BitCustomAlpha()`](#getlpf8bitcustomalpha) | 8-bit LPF | Get custom 8-bit LPF alpha |
| [`GetLpf8BitLevel()`](#getlpf8bitlevel) | 8-bit LPF | Get 8-bit LPF aggressiveness level |
| [`GetLpf16BitCustomAlphaFromCutoff()`](#getlpf16bitcustomalphafromotoff) | 16-bit LPF | Get alpha from cutoff frequency |
| [`GetLpfMakeupGain16Bit()`](#getlpfmakeupgain16bit) | LPF | Get 16-bit LPF makeup gain |
| [`GetPauseFadeTime()`](#getpausefadetime) | Fade Time | Get pause fade duration in seconds |
| [`GetPlaybackSpeed()`](#getplaybackspeed) | Internal State | Get current sample rate |
| [`GetPlaybackState()`](#getplaybackstate) | Internal State | Get current playback state |
| [`GetResumeFadeTime()`](#getresumefadetime) | Fade Time | Get resume fade duration in seconds |
| [`GetSoftClippingEnable()`](#getsoftclippingenable) | Filter Config | Read soft clipping enable state |
| [`GetVolumeResponseGamma()`](#getvolumeresponsegamma) | Volume Response | Get volume response gamma |
| [`GetVolumeResponseNonlinear()`](#getvolumeresponsenonlinear) | Volume Response | Get non-linear volume response mode |
| [`HAL_I2S_TxCpltCallback()`](#hal_i2s_txcpltcallback) | DMA Callbacks | DMA complete callback |
| [`HAL_I2S_TxHalfCpltCallback()`](#hal_i2s_txhalfcpltcallback) | DMA Callbacks | DMA half-complete callback |
| [`PausePlayback()`](#pauseplayback) | Playback Control | Pause with fade-out |
| [`PlaySample()`](#playsample) | Playback Control | Start sample playback |
| [`ProcessNextWaveChunk()`](#processnextwavechunk) | Chunk Processing | Process 16-bit samples |
| [`ProcessNextWaveChunk_8_bit()`](#processnextwavechunk_8_bit) | Chunk Processing | Process 8-bit samples |
| [`ResumePlayback()`](#resumeplayback) | Playback Control | Resume from pause with fade-in |
| [`SetAirEffectEnable()`](#setaireffectenable) | Air Effect | Enable/disable air effect |
| [`SetAirEffectGainDb()`](#setaireffectgaindb) | Air Effect | Set air effect gain in dB |
| [`SetAirEffectGainQ16()`](#setaireffectgainq16) | Air Effect | Set air effect gain in Q16 format |
| [`SetAirEffectPresetDb()`](#setaireffectpresetdb) | Air Effect | Set air effect preset (auto-enable) |
| [`SetDAC_Control()`](#setdac_control) | DAC Control | Enable/disable DAC power control |
| [`SetFadeInTime()`](#setfadeintime) | Fade Time | Set fade-in duration in seconds |
| [`SetFadeOutTime()`](#setfadeouttime) | Fade Time | Set fade-out duration in seconds |
| [`SetFadersEnabled()`](#setfadersenabled) | Fade Time | Enable/disable faders |
| [`SetFilterConfig()`](#setfilterconfig) | Filter Config | Apply complete filter configuration |
| [`SetHalfToFill()`](#sethalftofill) | Internal State | Set which buffer half to fill next |
| [`SetLpf8BitCustomAlpha()`](#setlpf8bitcustomalpha) | 8-bit LPF | Set custom alpha coefficient |
| [`SetLpf8BitLevel()`](#setlpf8bitlevel) | 8-bit LPF | Set 8-bit LPF aggressiveness level |
| [`SetLpf16BitCustomAlpha()`](#setlpf16bitcustomalpha) | 16-bit LPF | Set custom alpha coefficient |
| [`SetLpf16BitLevel()`](#setlpf16bitlevel) | 16-bit LPF | Set 16-bit LPF aggressiveness level |
| [`SetLpfMakeupGain8Bit()`](#setlpfmakeupgain8bit) | 8-bit LPF | Set makeup gain after 8-bit LPF |
| [`SetLpfMakeupGain16Bit()`](#setlpfmakeupgain16bit) | LPF | Set makeup gain after 16-bit LPF |
| [`SetPauseFadeTime()`](#setpausefadetime) | Fade Time | Set pause fade duration in seconds |
| [`SetPlaybackSpeed()`](#setplaybackspeed) | Internal State | Set sample rate |
| [`SetPlaybackState()`](#setplaybackstate) | Internal State | Set playback state |
| [`SetResumeFadeTime()`](#setresumefadetime) | Fade Time | Set resume fade duration in seconds |
| [`SetSoftClippingEnable()`](#setsoftclippingenable) | Filter Config | Enable/disable soft clipping |
| [`SetVolumeResponseGamma()`](#setvolumeresponsegamma) | Volume Response | Set volume response gamma |
| [`SetVolumeResponseNonlinear()`](#setvolumeresponsenonlinear) | Volume Response | Enable/disable non-linear volume response |
| [`ShutDownAudio()`](#shutdownaudio) | Playback Control | Shut down all audio hardware |
| [`StopPlayback()`](#stopplayback) | Playback Control | Stop playback with fade-out |
| [`WaitForSampleEnd()`](#waitforsampleend) | Playback Control | Block until playback completes |

## 📚 By Category

### Initialization (1 function)
- [`AudioEngine_Init()`](#audioengine_init) - Initialize engine with hardware callbacks

### Playback Control (7 functions)
- [`CalcSampleOffsetSamples()`](#calcsampleoffsetsamples) - Calculate sample offset from time, rate, and mode
- [`PlaySample()`](#playsample) - Start sample playback
- [`WaitForSampleEnd()`](#waitforsampleend) - Block until playback completes
- [`PausePlayback()`](#pauseplayback) - Pause with fade-out
- [`ResumePlayback()`](#resumeplayback) - Resume from pause with fade-in
- [`StopPlayback()`](#stopplayback) - Stop playback with fade-out
- [`ShutDownAudio()`](#shutdownaudio) - Shut down all audio hardware

### DAC Power Control (2 functions)
- [`SetDAC_Control()`](#setdac_control) - Enable/disable automatic DAC power control
- [`GetDAC_Control()`](#getdac_control) - Read current DAC power control state

### Application Callbacks (1 function)
- [`AudioEngine_OnPlaybackEnd()`](#audioengine_onplaybackend) - Weak callback invoked when playback ends

### Filter Configuration (5 functions)
- [`SetFilterConfig()`](#setfilterconfig) - Apply complete filter configuration
- [`GetFilterConfig()`](#getfilterconfig) - Read current filter configuration
- [`SetSoftClippingEnable()`](#setsoftclippingenable) - Enable/disable soft clipping
- [`GetSoftClippingEnable()`](#getsoftclippingenable) - Read soft clipping enable state
- [`SetLpfMakeupGain8Bit()`](#setlpfmakeupgain8bit) - Set makeup gain after 8-bit LPF

### 8-bit LPF Control (5 functions)
- [`SetLpf8BitLevel()`](#setlpf8bitlevel) - Set 8-bit LPF aggressiveness level
- [`GetLpf8BitLevel()`](#getlpf8bitlevel) - Get 8-bit LPF aggressiveness level
- [`SetLpf8BitCustomAlpha()`](#setlpf8bitcustomalpha) - Set custom 8-bit LPF alpha
- [`GetLpf8BitCustomAlpha()`](#getlpf8bitcustomalpha) - Get custom 8-bit LPF alpha
- [`CalcLpf8BitAlphaFromCutoff()`](#calclpf8bitalphafromcutoff) - Calculate alpha from cutoff frequency

### 16-bit LPF Control (4 functions)
- [`SetLpf16BitLevel()`](#setlpf16bitlevel) - Set 16-bit LPF aggressiveness level
- [`SetLpf16BitCustomAlpha()`](#setlpf16bitcustomalpha) - Set custom alpha coefficient
- [`CalcLpf16BitAlphaFromCutoff()`](#calclpf16bitalphafromotoff) - Calculate alpha from cutoff frequency
- [`GetLpf16BitCustomAlphaFromCutoff()`](#getlpf16bitcustomalphafromotoff) - Get alpha from cutoff frequency

### Makeup Gain (3 functions)
- [`SetLpfMakeupGain8Bit()`](#setlpfmakeupgain8bit) - Set makeup gain after 8-bit LPF
- [`SetLpfMakeupGain16Bit()`](#setlpfmakeupgain16bit) - Set makeup gain after 16-bit LPF
- [`GetLpfMakeupGain16Bit()`](#getlpfmakeupgain16bit) - Get 16-bit LPF makeup gain

### Air Effect Control (11 functions)
- [`SetAirEffectEnable()`](#setaireffectenable) - Enable/disable air effect
- [`GetAirEffectEnable()`](#getaireffectenable) - Read air effect enable state
- [`SetAirEffectGainQ16()`](#setaireffectgainq16) - Set air effect gain in Q16 format
- [`GetAirEffectGainQ16()`](#getaireffectgainq16) - Get air effect gain in Q16 format
- [`SetAirEffectGainDb()`](#setaireffectgaindb) - Set air effect gain in dB
- [`GetAirEffectGainDb()`](#getaireffectgaindb) - Get air effect gain in dB
- [`SetAirEffectPresetDb()`](#setaireffectpresetdb) - Set air effect preset (auto-enable)
- [`CycleAirEffectPresetDb()`](#cycleaireffectpresetdb) - Cycle through air effect presets
- [`GetAirEffectPresetIndex()`](#getaireffectpresetindex) - Get current preset index
- [`GetAirEffectPresetCount()`](#getaireffectpresetcount) - Get number of available presets
- [`GetAirEffectPresetDb()`](#getaireffectpresetdb) - Get dB value for a preset index

### Fade Time Configuration (8 functions)
- [`SetFadeInTime()`](#setfadeintime) - Set fade-in duration in seconds
- [`GetFadeInTime()`](#getfadeintime) - Get fade-in duration in seconds
- [`SetFadeOutTime()`](#setfadeouttime) - Set fade-out duration in seconds
- [`GetFadeOutTime()`](#getfadeouttime) - Get fade-out duration in seconds
- [`SetPauseFadeTime()`](#setpausefadetime) - Set pause fade duration in seconds
- [`GetPauseFadeTime()`](#getpausefadetime) - Get pause fade duration in seconds
- [`SetResumeFadeTime()`](#setresumefadetime) - Set resume fade duration in seconds
- [`GetResumeFadeTime()`](#getresumefadetime) - Get resume fade duration in seconds

### Faders (2 functions)
- [`SetFadersEnabled()`](#setfadersenabled) - Enable/disable faders
- [`GetFadersEnabled()`](#getfadersenabled) - Get fader enable state

### Chunk Processing (3 functions)
- [`ProcessNextWaveChunk()`](#processnextwavechunk) - Process 16-bit samples
- [`ProcessNextWaveChunk_8_bit()`](#processnextwavechunk_8_bit) - Process 8-bit samples
- [`AdvanceSamplePointer()`](#advancesamplepointer) - Advance to next DMA buffer position

### DMA Callbacks (2 functions)
- [`HAL_I2S_TxHalfCpltCallback()`](#hal_i2s_txhalfcpltcallback) - DMA half-complete callback
- [`HAL_I2S_TxCpltCallback()`](#hal_i2s_txcpltcallback) - DMA complete callback

### Internal State (4+ functions)
- [`GetPlaybackState()`](#getplaybackstate) - Get current playback state
- [`SetPlaybackState()`](#setplaybackstate) - Set playback state
- [`GetHalfToFill()`](#gethalftofill) - Get which buffer half to fill next
- [`SetHalfToFill()`](#sethalftofill) - Set which buffer half to fill next
- [`GetPlaybackSpeed()`](#getplaybackspeed) - Get current sample rate
- [`SetPlaybackSpeed()`](#setplaybackspeed) - Set sample rate

### Volume Response Control (4 functions)
- [`SetVolumeResponseNonlinear()`](#setvolumeresponsenonlinear) - Enable/disable non-linear response
- [`GetVolumeResponseNonlinear()`](#getvolumeresponsenonlinear) - Get non-linear response mode
- [`SetVolumeResponseGamma()`](#setvolumeresponsegamma) - Set gamma exponent
- [`GetVolumeResponseGamma()`](#getvolumeresponsegamma) - Get gamma exponent

## 🔍 Quick Lookups

### By Task

**Play Audio:**
- Simple blocking: [`PlaySample()`](#playsample) → [`WaitForSampleEnd()`](#waitforsampleend)
- Non-blocking: [`PlaySample()`](#playsample) → poll [`GetPlaybackState()`](#getplaybackstate)
- Pause/Resume: [`PausePlayback()`](#pauseplayback) → [`ResumePlayback()`](#resumeplayback)
- Stop: [`StopPlayback()`](#stopplayback) → poll [`GetPlaybackState()`](#getplaybackstate)

**Configure Filters:**
- Batch config: [`GetFilterConfig()`](#getfilterconfig) → modify → [`SetFilterConfig()`](#setfilterconfig)
- Single setting: [`SetLpf16BitLevel()`](#setlpf16bitlevel), [`SetSoftClippingEnable()`](#setsoftclippingenable), etc.

**Control Air Effect:**
- Simple: [`SetAirEffectPresetDb(preset)`](#setaireffectpresetdb) (auto-enables when preset > 0)
- Advanced: [`SetAirEffectGainDb()`](#setaireffectgaindb) + [`SetAirEffectEnable()`](#setaireffectenable)
- UI cycling: [`CycleAirEffectPresetDb()`](#cycleaireffectpresetdb)

**Adjust Fade Times:**
- Get/Set pairs: [`GetFadeInTime()`](#getfadeintime) / [`SetFadeInTime()`](#setfadeintime), etc.
- Typical range: 0.0–5.0 seconds

### Most Common Functions

| Function | Usage Frequency | Description |
|----------|----------------|-------------|
| `PlaySample()` | ⭐⭐⭐⭐⭐ | Start audio playback (primary API) |
| `WaitForSampleEnd()` | ⭐⭐⭐⭐⭐ | Block until complete (simple use case) |
| `SetFilterConfig()` | ⭐⭐⭐⭐ | Batch filter configuration |
| `SetAirEffectPresetDb()` | ⭐⭐⭐⭐ | Air effect presets with auto-enable |
| `SetLpf16BitLevel()` | ⭐⭐⭐ | Adjust 16-bit filter aggressiveness |
| `PausePlayback()` | ⭐⭐⭐ | Pause with fade |
| `ResumePlayback()` | ⭐⭐⭐ | Resume from pause |
| `GetPlaybackState()` | ⭐⭐ | Non-blocking playback monitoring |
| `AudioEngine_Init()` | ⭐ | One-time initialization |

## 📖 Full Documentation

For complete function signatures, parameters, return values, and code examples:

- **Comprehensive Reference**: [API_REFERENCE.md](API_REFERENCE.md) - All 60+ functions with detailed examples
- **Quick Guide**: [QUICK_REFERENCE.md](QUICK_REFERENCE.md) - Common patterns and fast lookup
- **Header File**: [audio_engine.h](../Core/Libraries/audio_engine.h) - Doxygen-documented signatures (IDE integration)
- **Manual**: [AUDIO_ENGINE_MANUAL.md](AUDIO_ENGINE_MANUAL.md) - Architecture and integration guide

## 📝 Function Details

### AudioEngine_Init()
```c
PB_StatusTypeDef AudioEngine_Init(
  DAC_SwitchFunc dac_switch,
  ReadVolumeFunc read_volume,
  I2S_InitFunc i2s_init
);
```
Initialize the audio engine with hardware callbacks. Must be called before any playback.

**See**: [API_REFERENCE.md - Initialization](API_REFERENCE.md#initialization)

### PlaySample()
```c
PB_StatusTypeDef PlaySample(
  const void *sample_to_play,
  uint32_t sample_set_sz,
  uint32_t playback_speed,
  uint8_t sample_depth,
  PB_ModeTypeDef mode
);
```
Start playback of an audio sample from memory.

**See**: [API_REFERENCE.md - Playback Control](API_REFERENCE.md#playback-control)

### SetFilterConfig()
```c
void SetFilterConfig(const FilterConfig_TypeDef *cfg);
```
Apply a complete filter configuration atomically.

**See**: [API_REFERENCE.md - Filter Configuration](API_REFERENCE.md#filter-configuration)

### SetAirEffectPresetDb()
```c
void SetAirEffectPresetDb(uint8_t preset_index);
```
Set air effect using a preset index. Automatically enables air effect if preset > 0, disables if preset == 0.

**See**: [API_REFERENCE.md - Air Effect Control](API_REFERENCE.md#air-effect-control)

---

For all other function details, see the comprehensive documentation files listed above.

## 🔗 Related Documentation

- [AUDIO_ENGINE_MANUAL.md](AUDIO_ENGINE_MANUAL.md) - Complete system architecture
- [API_REFERENCE.md](API_REFERENCE.md) - Detailed function reference
- [QUICK_REFERENCE.md](QUICK_REFERENCE.md) - Fast lookup guide
- [HEADER_DOCUMENTATION.md](HEADER_DOCUMENTATION.md) - Guide to Doxygen header
- [audio_engine.h](../Core/Libraries/audio_engine.h) - Doxygen-documented header

---

*Last updated: 2026-03-01*
*Part of the Audio Engine Documentation Suite*
