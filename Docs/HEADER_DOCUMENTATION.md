# Audio Engine Header Documentation Guide

## Overview

The `audio_engine.h` header file now includes comprehensive Doxygen-style documentation for all public functions. This guide explains the documentation structure and how to use it effectively.

## Documentation Format

Each public function in `audio_engine.h` is documented using standard Doxygen format:

```c
/**
 * @brief One-line summary of what the function does
 * @param[in] param_name Description of input parameter with type/range
 * @param[out] param_name Description of output parameter
 * @param[in,out] param_name Description of parameter that is both input and output
 * @return Description of return value and what it means
 * @note Any special usage notes, caveats, or important information
 */
function_declaration;
```

### Section Organization

The header is organized into logical groups of related functions:

1. **Initialization** - `AudioEngine_Init()`
   - Sets up the audio engine with hardware callbacks
   - Must be called before any playback

2. **Application Callbacks** (1 function)
   - `AudioEngine_OnPlaybackEnd()` - Playback end notification callback

3. **Filter Configuration** (9 functions)
   - `SetFilterConfig()` / `GetFilterConfig()` - Batch configuration
   - `SetLpfMakeupGain8Bit()` - 8-bit LPF makeup gain
   - `SetLpfMakeupGain16Bit()` / `GetLpfMakeupGain16Bit()` - 16-bit LPF makeup gain
   - `SetAirEffectEnable()` / `GetAirEffectEnable()` - Air effect enable
   - `SetSoftClippingEnable()` / `GetSoftClippingEnable()` - Clipping control

4. **8-bit LPF Control** (5 functions)
   - `SetLpf8BitLevel()` / `GetLpf8BitLevel()` - 8-bit LPF aggressiveness
   - `SetLpf8BitCustomAlpha()` / `GetLpf8BitCustomAlpha()` - Custom alpha control
   - `CalcLpf8BitAlphaFromCutoff()` - Frequency-based alpha helper

5. **16-bit LPF Control** (4 functions)
   - `SetLpf16BitLevel()` / `SetLpf16BitCustomAlpha()` - 16-bit LPF control
   - `CalcLpf16BitAlphaFromCutoff()` - Frequency-based LPF configuration
   - `GetLpf16BitCustomAlphaFromCutoff()` - Query LPF cutoff frequency

6. **Fade Time Configuration** (10 functions)
   - `SetFadeInTime()` / `GetFadeInTime()` - Playback start fade
   - `SetFadeOutTime()` / `GetFadeOutTime()` - Playback stop fade
   - `SetPauseFadeTime()` / `GetPauseFadeTime()` - Pause fade
   - `SetResumeFadeTime()` / `GetResumeFadeTime()` - Resume fade
   - `SetFadersEnabled()` / `GetFadersEnabled()` - Enable/disable faders

7. **Playback Control** (7 functions)
   - `CalcSampleOffsetSamples()` - Compute sample offset from time, rate, and mode
   - `PlaySample()` - Start playback
   - `WaitForSampleEnd()` - Block until playback completes
   - `PausePlayback()` - Pause with fade-out
   - `ResumePlayback()` - Resume with fade-in
   - `StopPlayback()` - Stop with fade-out
   - `ShutDownAudio()` - Stop all audio hardware

8. **Chunk Processing (DMA Callbacks)** (3 functions)
   - `ProcessNextWaveChunk()` - 16-bit sample processing
   - `ProcessNextWaveChunk_8_bit()` - 8-bit sample processing
   - `AdvanceSamplePointer()` - Update playback position

9. **DAC Power Control** (2 functions)
   - `SetDAC_Control()` / `GetDAC_Control()` - DAC power control

10. **Volume Response Control** (3 functions)
   - `SetVolumeResponseNonlinear()` / `GetVolumeResponseNonlinear()` - Enable/disable non-linear response
   - `SetVolumeResponseGamma()` / `GetVolumeResponseGamma()` - Gamma curve control

11. **Air Effect Control** (9 functions)
   - `SetAirEffectGainQ16()` / `GetAirEffectGainQ16()` - Fine-grain gain control
   - `SetAirEffectGainDb()` / `GetAirEffectGainDb()` - Decibel-based control
   - `SetAirEffectPresetDb()` - Preset selection with auto-enable
   - `CycleAirEffectPresetDb()` - Cycle through presets (UI control)
   - `GetAirEffectPresetIndex()` - Query current preset
   - `GetAirEffectPresetCount()` - Query available presets
   - `GetAirEffectPresetDb()` - Query specific preset's dB level

12. **DMA Callbacks** (2 functions)
   - `HAL_I2S_TxHalfCpltCallback()` - Half-buffer complete
   - `HAL_I2S_TxCpltCallback()` - Full-buffer complete

13. **Playback State (Internal)** (6 functions)
   - `GetPlaybackState()` / `SetPlaybackState()`
   - `GetHalfToFill()` / `SetHalfToFill()`
   - `GetPlaybackSpeed()` / `SetPlaybackSpeed()`

## Key Documentation Elements

### @brief Tags
One-line summaries describe what each function does in user-friendly language:
- "Set air effect gain using decibels"
- "Pause playback with fade-out"
- "Process next 16-bit PCM chunk from DMA half-complete callback"

### @param[in] Tags
Input parameters are documented with:
- Type information (e.g., "float", "uint8_t", "pointer")
- Range or valid values (e.g., "1-65535", "0.0-5.0", "Mode_mono or Mode_stereo")
- Special meaning (e.g., "0 = off, 1 = on")

### @param[out] Tags
Output parameters indicate data returned by pointer:
- `cfg` in `GetFilterConfig()` receives current settings
- `alpha` in `CalcLpf16BitAlphaFromCutoff()` provides computed value

### @return Tags
Return values explain what the function produces:
- Status codes: `PB_Playing`, `PB_Idle`, `PB_Error`
- Numeric values: Sample count, preset index, gain in Q16
- Boolean-like: Enabled state (0 or 1)

### @note Tags
Special usage notes include:
- Preconditions (e.g., "Must be called after `AudioEngine_Init()`")
- Auto-control behavior (e.g., "Automatically enables air effect if preset > 0")
- Performance notes (e.g., "Called from DMA ISR")
- Alternative approaches (e.g., "For non-blocking use, poll `GetPlaybackState()`")

## Using the Documentation

### In Code Editors
Most modern IDEs (Visual Studio, CrossWorks, VS Code, CLion) will display this documentation as:
- **Hover tooltips**: When you hover over a function name
- **IntelliSense popups**: When you type function names
- **Parameter hints**: While typing function arguments

### With Doxygen
To generate HTML/PDF documentation:

```bash
# Install Doxygen if needed
sudo apt-get install doxygen graphviz

# Generate documentation
cd /path/to/project
doxygen Doxyfile
```

This creates a `docs/html/index.html` with searchable API reference.

### In Documentation Tools
The same Doxygen format can be used by:
- **Sphinx** (with Breathe extension) for integrated documentation
- **Read the Docs** for online hosting
- **GitHub Wiki** (by copying relevant parts)
- **Documentation generators** that parse C headers

## Common Patterns in Documentation

### Configuration Setter/Getter Pairs
```c
/**
 * @brief Set the <feature>
 * @param[in] <param> <Description with valid values>
 */
void Set<Feature>(<Type> <param>);

/**
 * @brief Get the current <feature> setting
 * @return Current <feature>
 */
<ReturnType> Get<Feature>(void);
```

Example: `SetFadeInTime()` / `GetFadeInTime()`

### Enumeration-Based Functions
```c
/**
 * @brief Set <feature> to a predefined level
 * @param[in] level One of: LPF_Off, LPF_VerySoft, LPF_Soft, ... (enumeration values)
 * @note [Explanation of how levels affect behavior]
 */
void Set<Feature>Level(LPF_Level level);
```

Example: `SetLpf8BitLevel()`

### Dual-Unit Functions (Q16 and dB)
```c
/**
 * @brief Set <feature> gain using <unit>
 * @param[in] gain_<unit> Gain in <unit> (<range>)
 * @note [Relationship to other unit, e.g., "6.0 dB ≈ 2x gain"]
 */
void Set<Feature>Gain<Unit>(<Type> gain_<unit>);
```

Examples:
- `SetAirEffectGainQ16()` (65536 = 1.0x)
- `SetAirEffectGainDb()` (0.0 dB = 1.0x)

### Status Codes
Functions return:
- `PB_Playing` - Playback is active
- `PB_Paused` - Playback is paused
- `PB_Idle` - No playback, ready to start
- `PB_Error` - Playback failed or invalid operation

### Volume Control Note
The `ReadVolume` callback returns 1-65535:
- 0 is never returned (would mute audio)
- 1-65535 maps linearly to the I2S DMA processing
- See [AUDIO_ENGINE_MANUAL.md](AUDIO_ENGINE_MANUAL.md) for non-linear response details

## Integration with Other Documentation

### Cross-References
- **Header** (`audio_engine.h`) - Function signatures with Doxygen docs
- **API Reference** (`API_REFERENCE.md`) - Detailed explanations and code examples
- **Quick Reference** (`QUICK_REFERENCE.md`) - Fast lookup by task
- **Audio Engine Manual** (`AUDIO_ENGINE_MANUAL.md`) - Architecture and concepts
- **Code Comments** - Implementation details in `audio_engine.c`

### Using All Together
1. **First time learning**: Read [AUDIO_ENGINE_MANUAL.md](AUDIO_ENGINE_MANUAL.md)
2. **Implementing a feature**: Check [API_REFERENCE.md](API_REFERENCE.md) for examples
3. **Quick lookup**: Reference [QUICK_REFERENCE.md](QUICK_REFERENCE.md)
4. **IDE integration**: Hover over functions to see header documentation
5. **Deep dive**: Read source code in `audio_engine.c`

## Function Count Summary

Total documented functions: **63**

- **Initialization**: 1
- **Application Callbacks**: 1
- **Filter Configuration**: 9
- **8-bit LPF Control**: 5
- **16-bit LPF Control**: 4
- **Faders**: 2
- **Fade Control**: 8
- **Playback Control**: 7
- **DAC Power Control**: 2
- **Volume Response Control**: 4
- **Chunk Processing**: 3
- **Air Effect**: 9
- **DMA Callbacks**: 2
- **Internal State**: 6

## Standards Used

The documentation follows these standards:

1. **Doxygen 1.8.x** - Standard C documentation tool
2. **JavaDoc** style comments - `/** ... */` with `@tag` markers
3. **Parameter conventions**:
   - `[in]` - Input parameters
   - `[out]` - Output parameters (received by pointer)
   - `[in,out]` - Both input and output
4. **Consistent formatting** - Parallel structure for similar functions

## Benefits of This Documentation

✓ **IDE Integration** - Automatic tooltips and IntelliSense
✓ **Searchability** - Doxygen generates searchable HTML
✓ **Maintainability** - Changes documented in source code
✓ **Completeness** - Every public function documented
✓ **Consistency** - Uniform format across all functions
✓ **Accessibility** - Accessible to tools and developers

## See Also

- [API_REFERENCE.md](API_REFERENCE.md) - Comprehensive function reference with examples
- [QUICK_REFERENCE.md](QUICK_REFERENCE.md) - Fast lookup guide
- [AUDIO_ENGINE_MANUAL.md](AUDIO_ENGINE_MANUAL.md) - System architecture and design
- Source: [audio_engine.h](../Core/Libraries/audio_engine.h)

---

*Last updated: 2026-03-01*
*Part of the Audio Engine Documentation Suite*
