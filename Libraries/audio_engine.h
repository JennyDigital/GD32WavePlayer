/**
  ******************************************************************************
  * @file           : audio_engine.h
  * @brief          : Audio playback engine and DSP filter chain
  ******************************************************************************
  *
  * MIT License
  *
  * Copyright (c) 2024-2026 Jennifer A. Gunn (with technological assistance).
  *
  * Permission is hereby granted, free of charge, to any person obtaining a copy
  * of this software and associated documentation files (the "Software"), to deal
  * in the Software without restriction, including without limitation the rights
  * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  * copies of the Software, and to permit persons to whom the Software is
  * furnished to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in all
  * copies or substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  * SOFTWARE.
  *
  * @attention
  *
  * Reusable audio playback engine with runtime-configurable DSP filters.
  * Supports 8-bit and 16-bit samples, mono/stereo playback, and various
  * audio processing effects (LPF, DC blocking, fade in/out, soft clipping).
  *
  ******************************************************************************
  */


/** WARNING!
  *
  * The GD32 audio engine no longer depends on SysTick callbacks for playback stop or delay timing.
  * Delay timing is handled by polling the SysTick hardware counter directly, so it remains functional
  * even when interrupts are masked.
  *
  * DMA callback timing is still critical: DMA/I2S IRQ handlers must run often enough to refill
  * buffers before underrun.
  *
  * The DMA I2S callbacks just need to beat the buffer refill time, so that they can keep the buffer supplied
  * with processed audio samples.
*/
#ifndef __AUDIO_ENGINE_H
#define __AUDIO_ENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <gd32f30x.h>

/* Our chosen SPI port */
#define PROJECT_SPI SPI1

/* Set to 0 to avoid inlining ProcessDMACallback (reduces code size). */
#ifndef AUDIO_ENGINE_INLINE_DMA_CALLBACK
#define AUDIO_ENGINE_INLINE_DMA_CALLBACK 0
#endif

/* Set to 0 to compile out Air Effect support and save flash. */
#ifndef AUDIO_ENGINE_ENABLE_AIR_EFFECT
#define AUDIO_ENGINE_ENABLE_AIR_EFFECT 1
#endif

/* DAC control values */
#define DAC_OFF               false
#define DAC_ON                true

/* Playback engine buffer configuration */
// Larger buffer sizes can reduce CPU load but increase latency and RAM usage. Adjust as needed.
// and must be a multiple of 2 correct DMA buffer size requirements.
#define PB_BUFF_SZ                  2048U
#define CHUNK_SZ                    ( PB_BUFF_SZ / 2 )
#define HALFCHUNK_SZ                ( CHUNK_SZ / 2 )
#define FIRST                       0U
#define SECOND                      1U

/* Fade configuration */
#define PAUSE_FADEOUT_SAMPLES       5512U    // About 0.5 second at 11kHz
#define FADEIN_SAMPLES              2048U    // About 93ms at 22kHz

/* DC blocking filter configuration */
#define DC_FILTER_ALPHA             64225    // 0.98 in fixed-point (64225/65536)
#define DC_FILTER_SHIFT             16       // Right shift for fixed-point division
#define SOFT_DC_FILTER_ALPHA        65216    // 0.995 in fixed-point (65216/65536)

/* 16-bit biquad low-pass filter aggressiveness levels (alpha coefficients) */
/* Lower alpha is heavier filtering for this biquad, so values are ordered heavy -> light. */
#define LPF_16BIT_VERY_SOFT         40960    // 0.625 - minimal filtering / highest cutoff
#define LPF_16BIT_SOFT              52429    // ~0.80 - gentle filtering
#define LPF_16BIT_MEDIUM            57344    // 0.875 - balanced filtering
#define LPF_16BIT_FIRM              60416    // ~0.92 - firm filtering
#define LPF_16BIT_AGGRESSIVE        63488    // ~0.97 - strongest filtering / lowest cutoff

/* Number of cycles to warm up biquad filter state */
#define BIQUAD_WARMUP_CYCLES        16

/* Low-pass filter for 8-bit samples */
#define LPF_MAKEUP_GAIN_Q16         70779   // ~1.08x post-LPF makeup (default)
/* Low-pass filter for 16-bit samples */
#define LPF_16BIT_MAKEUP_GAIN_Q16   65536   // 1.00x post-LPF makeup (default)

/* 8-bit low-pass filter aggressiveness levels (alpha coefficients in fixed-point) */
#define LPF_VERY_SOFT           61440       // 0.9375 - very gentle filtering
#define LPF_SOFT                57344       // 0.875 - gentle filtering
#define LPF_MEDIUM              49152       // 0.75 - balanced filtering
#define LPF_FIRM                45056       // 0.6875 - firm filtering
#define LPF_AGGRESSIVE          40960       // 0.625 - strong filtering

/* Noise gate configuration */
#define NOISE_GATE_THRESHOLD    512         // ~1.5% of full scale

/* Audio silence midpoints */
#define SAMPLE8_MIDPOINT        128U        // Midpoint for unsigned 8-bit unsigned samples
#define SAMPLE16_MIDPOINT       0           // Midpoint for signed 16-bit samples

/* Fill half buffer macro */
#define MIDPOINT_FILL_BUFFER() memset( pb_buffer, SAMPLE16_MIDPOINT, sizeof( pb_buffer ) );

/* Attribute to put code into fast memory */
#define PUT_IN_FASTMEM __attribute__((section(".fast")))

/* Playback status type */
typedef enum {
  PB_Idle,
  PB_Error,
  PB_Playing,
  PB_Pausing,
  PB_Paused,
  PB_PlayingFailed
} PB_StatusTypeDef;

/* Playback mode type */
typedef enum {
  Mode_stereo,
  Mode_mono
} PB_ModeTypeDef;

/* Audio channel identifier */
typedef enum {
  CHANNEL_LEFT = 0U,
  CHANNEL_RIGHT = 1U,
  CHANNEL_COUNT = 2U
} AudioChannelId;

/* Low-pass filter aggressiveness type */
typedef enum {
  LPF_Off,
  LPF_VerySoft,
  LPF_Soft,
  LPF_Medium,
  LPF_Firm,
  LPF_Aggressive,
  LPF_Custom
} LPF_Level;

/* Air Effect (High-Shelf Brightening) Filter */
#define AIR_EFFECT_SHELF_GAIN       98304     // ~1.5 in Q16 (high-frequency shelf ~ +1.6 dB)
#define AIR_EFFECT_SHELF_GAIN_MAX   131072    // Cap runtime boost at ~2.0x to avoid harsh clipping
#define AIR_EFFECT_CUTOFF           49152     // ~0.75 alpha (cutoff around 5-6 kHz @ 22kHz)

/* Filter chain runtime configuration */
typedef struct {
  uint8_t enable_16bit_biquad_lpf;            // Biquad low-pass filter for 16-bit samples
  uint8_t enable_soft_dc_filter_16bit;        // Soft DC blocking filter for 16-bit samples
  uint8_t enable_8bit_lpf;                    // Biquad low-pass filter for 8-bit samples
  uint8_t enable_noise_gate;                  // Noise gate. Removes low-level noise.
  uint8_t enable_soft_clipping;               // Soft clipping.
  uint8_t enable_air_effect;                  // High-shelf brightening filter
  uint32_t lpf_makeup_gain_q16;               // Q16 gain applied after LPF
  uint32_t lpf_makeup_gain_16bit_q16;         // Q16 gain applied after 16-bit LPF
  LPF_Level lpf_16bit_level;                  // Filter level for 16-bit biquad LPF
  uint16_t lpf_16bit_custom_alpha;            // Q16 alpha for custom 16-bit LPF
  LPF_Level lpf_8bit_level;                   // Filter level for 8-bit LPF
  uint16_t lpf_8bit_custom_alpha;             // Q16 alpha for custom 8-bit LPF
  uint8_t enable_filter_chain_16bit;          // Master enable for entire 16-bit filter chain
  uint8_t enable_filter_chain_8bit;           // Master enable for entire 8-bit filter chain
} FilterConfig_TypeDef;

/* Global audio engine state exposed for hardware initialization */
extern uint32_t I2S_PlaybackSpeed;

/* Hardware interface function pointer types */
typedef void        ( *DAC_SwitchFunc )         ( uint8_t setting );
typedef uint16_t    ( *ReadVolumeFunc )         ( void );
typedef void        ( *I2S_InitFunc )           ( uint32_t speed );
typedef void        ( *PlaybackEndCallback )    ( void );

extern DAC_SwitchFunc AudioEngine_DACSwitch;
extern ReadVolumeFunc AudioEngine_ReadVolume;
extern I2S_InitFunc   AudioEngine_I2SInit;

/**
 * @brief Weak callback invoked when playback ends naturally or via stop
 * @note Define this function in your application to receive playback end notifications
 * @note Called from ISR context - keep implementation short and non-blocking
 */
void AudioEngine_OnPlaybackEnd( void );

/**
 * @brief Initialize the audio engine with hardware interface callbacks
 * @param[in] dac_switch   Function to control DAC amplifier power (GPIO)
 * @param[in] read_volume  Function to read current volume setting (0-255)
 * @param[in] i2s_init     Function to initialize I2S peripheral
 * @return PB_Idle on success, PB_Error on failure
 */
PB_StatusTypeDef    AudioEngine_Init                  (
                                                        DAC_SwitchFunc dac_switch,
                                                        ReadVolumeFunc read_volume,
                                                        I2S_InitFunc i2s_init
                                                      );

/* Filter configuration functions */
/**
 * @brief Apply a complete filter configuration to the audio engine
 * @param[in] cfg Pointer to FilterConfig_TypeDef with desired settings
 * @note All filter settings in cfg are applied atomically
 */
void                SetFilterConfig                   ( const FilterConfig_TypeDef *cfg );

/**
 * @brief Read current filter configuration from the audio engine
 * @param[out] cfg Pointer to FilterConfig_TypeDef to receive current settings
 */
void                GetFilterConfig                   ( FilterConfig_TypeDef *cfg );

/**
 * @brief Set makeup gain applied after 8-bit low-pass filter
 * @param[in] gain Linear gain (1.0 = no gain, 2.0 = 2x, etc.)
 * @note Compensates for attenuation from aggressive filtering
 */
void                SetLpfMakeupGain8Bit              ( float gain );

/**
 * @brief Set makeup gain applied after 16-bit low-pass filter
 * @param[in] gain Linear gain (1.0 = no gain, 2.0 = 2x, etc.)
 * @note Compensates for attenuation from aggressive filtering
 */
void                SetLpfMakeupGain16Bit             ( float gain );

/**
 * @brief Get makeup gain applied after 16-bit low-pass filter
 * @return Linear gain (1.0 = no gain, 2.0 = 2x, etc.)
 */
float               GetLpfMakeupGain16Bit             ( void );

/**
 * @brief Enable/disable the high-shelf air effect brightening filter
 * @param[in] enabled 1 to enable, 0 to disable
 * @note Air effect is automatically controlled by SetAirEffectPresetDb()
 */
void                SetAirEffectEnable                ( uint8_t enabled );

/**
 * @brief Read current air effect enable state
 * @return 1 if enabled, 0 if disabled
 */
uint8_t             GetAirEffectEnable                ( void );

/**
 * @brief Enable/disable soft clipping cubic curve above ±28,000
 * @param[in] enabled 1 to enable, 0 to disable
 */
void                SetSoftClippingEnable             ( uint8_t enabled );

/**
 * @brief Read current soft clipping enable state
 * @return 1 if enabled, 0 if disabled
 */
uint8_t             GetSoftClippingEnable             ( void );

/* 8-bit sample low-pass filter configuration */
/**
 * @brief Set the aggressiveness level of the 8-bit low-pass filter
 * @param[in] level One of: LPF_Off, LPF_VerySoft, LPF_Soft, LPF_Medium, LPF_Firm, LPF_Aggressive, LPF_Custom
 * @note Custom alpha can be set separately via SetLpf8BitCustomAlpha()
 */
void                SetLpf8BitLevel                   ( LPF_Level level );

/**
 * @brief Get the current aggressiveness level of the 8-bit low-pass filter
 * @return Current LPF_Level (Off, VerySoft, Soft, Medium, Firm, Aggressive, or Custom)
 */
LPF_Level           GetLpf8BitLevel                   ( void );

/**
 * @brief Set a custom alpha coefficient for the 8-bit low-pass filter
 * @param[in] alpha Q16 fixed-point alpha value (0-65535), where 65535 = 0.99999, 32768 = 0.5
 * @note This sets level to LPF_Custom and uses the provided alpha directly
 */
void                SetLpf8BitCustomAlpha             ( uint16_t alpha );

/**
 * @brief Get the current custom alpha coefficient for the 8-bit low-pass filter
 * @return Q16 fixed-point alpha value
 */
uint16_t            GetLpf8BitCustomAlpha             ( void );

/* 16-bit sample low-pass filter configuration */
/**
 * @brief Set the aggressiveness level of the 16-bit biquad low-pass filter
 * @param[in] level One of: LPF_Off, LPF_VerySoft, LPF_Soft, LPF_Medium, LPF_Firm, LPF_Aggressive, LPF_Custom
 */
void                SetLpf16BitLevel                  ( LPF_Level level );

/**
 * @brief Set a custom alpha coefficient for the 16-bit low-pass filter
 * @param[in] alpha Q16 fixed-point alpha value (0-65535), where 65535 = 0.99999, 32768 = 0.5
 * @note This sets level to LPF_Custom and uses the provided alpha directly
 */
void                SetLpf16BitCustomAlpha            ( uint16_t alpha );

/**
 * @brief Calculate Q16 alpha coefficient from desired cutoff frequency
 * @param[in] cutoff_hz Cutoff frequency in Hz
 * @param[in] sample_rate_hz Sample rate in Hz
 * @return Q16 alpha value suitable for SetLpf16BitCustomAlpha()
 * @note Formula: alpha = 2*pi*fc / sr, clamped to [0, 65535]
 */
uint16_t            CalcLpf16BitAlphaFromCutoff       ( float cutoff_hz, float sample_rate_hz );

/**
 * @brief Calculate Q16 alpha coefficient for 8-bit LPF from desired cutoff frequency
 * @param[in] cutoff_hz Cutoff frequency in Hz
 * @param[in] sample_rate_hz Sample rate in Hz
 * @return Q16 alpha value suitable for SetLpf8BitCustomAlpha()
 * @note Formula: alpha = 1 - exp(-2*pi*fc / fs), clamped to [0, 65535]
 */
uint16_t            CalcLpf8BitAlphaFromCutoff        ( float cutoff_hz, float sample_rate_hz );

/**
 * @brief Get the cutoff frequency corresponding to the current 16-bit LPF custom alpha
 * @param[in] cutoff_hz Cutoff frequency in Hz
 * @return Q16 alpha value, or 0 if not in custom mode
 */
uint16_t            GetLpf16BitCustomAlphaFromCutoff  ( float cutoff_hz );

/* Fade time configuration functions */

/** @brief Set whether the faders are enabled or not
 *  @param[in] 1 (Enabled) or 0 (Disabled).
 */
void                SetFadersEnabled                  ( uint8_t fader_setting );

/**
 * @brief Get the state of whether the faders are enabled or not.
 * @return Faders enabled (1) or not (0)
 */
  uint8_t             GetFadersEnabled                ( void );

/**
 * @brief Set the duration of fade-in ramp at playback start
 * @param[in] seconds Fade-in time in seconds (0.0-5.0 typical range)
 * @note Fade uses quadratic curve for smooth volume ramp
 */
void                SetFadeInTime                     ( float seconds );

/**
 * @brief Get the current fade-in time setting
 * @return Fade-in time in seconds
 */
float               GetFadeInTime                     ( void );

/**
 * @brief Set the duration of fade-out ramp when stopping playback
 * @param[in] seconds Fade-out time in seconds (0.0-5.0 typical range)
 */
void                SetFadeOutTime                    ( float seconds );

/**
 * @brief Get the current fade-out time setting
 * @return Fade-out time in seconds
 */
float               GetFadeOutTime                    ( void );

/**
 * @brief Set the duration of fade-out when pausing playback
 * @param[in] seconds Pause fade time in seconds (0.0-5.0 typical range)
 * @note Typically longer than normal fade-out for smooth pause
 */
void                SetPauseFadeTime                  ( float seconds );

/**
 * @brief Get the current pause fade time setting
 * @return Pause fade time in seconds
 */
float               GetPauseFadeTime                  ( void );

/**
 * @brief Set the duration of fade-in ramp when resuming from pause
 * @param[in] seconds Resume fade time in seconds (0.0-5.0 typical range)
 */
void                SetResumeFadeTime                 ( float seconds );

/**
 * @brief Get the current resume fade time setting
 * @return Resume fade time in seconds
 */
float               GetResumeFadeTime                 ( void );

/** Set whether the 16-bit filter chain is enabled or not.
 * @param[in] 1 = Filter chain enabled, 0 = filter chain disabled
 */
void                SetFilterChain16BitEnable         ( uint8_t enabled );

/** Set whether the 8-bit filter chain is enabled or not.
 * @param[in] 1 = Filter chain enabled, 0 = filter chain disabled
 */
void                SetFilterChain8BitEnable         ( uint8_t enabled );

/* Playback control functions */
/**
 * @brief Calculate a sample offset from time, sample rate, and mode
 * @param[in] seconds Desired offset in seconds (>= 0)
 * @param[in] sample_rate_hz Sample rate in Hz
 * @param[in] mode Playback mode: Mode_mono or Mode_stereo
 * @return Sample offset in interleaved samples
 * @note For stereo, the returned value is multiplied by 2 (left+right).
 */
uint32_t            CalcSampleOffsetSamples           (
                                                        float seconds,
                                                        uint32_t sample_rate_hz,
                                                        PB_ModeTypeDef mode 
                                                      );
/**
 * @brief Start playback of a sample from memory
 * @param[in] sample_to_play Pointer to sample data in memory
 * @param[in] sample_set_sz Total number of samples to play (all channels combined)
 * @param[in] playback_speed Sample rate in Hz (e.g., 22000, 44100)
 * @param[in] sample_depth Bits per sample: 8 or 16
 * @param[in] mode Playback mode: Mode_mono or Mode_stereo
 * @return PB_Playing on success, PB_Error on failure
 */
PB_StatusTypeDef    PlaySample                        ( 
                                                        const void *sample_to_play, 
                                                        uint32_t sample_set_sz, 
                                                        uint32_t playback_speed, 
                                                        uint8_t sample_depth, 
                                                        PB_ModeTypeDef mode 
                                                      ); 

/**
 * @brief Block until current sample playback completes
 * @return PB_Idle when playback finished, PB_Error on playback failure
 * @note This blocks while paused/pausing as well. For non-blocking, poll GetPlaybackState().
 */
PB_StatusTypeDef    WaitForSampleEnd                  ( void );

/**
 * @brief Pause playback with fade-out
 * @return PB_Paused on success, PB_Error if not playing
 * @note Applies pause fade time before silencing. Use ResumePlayback() to resume.
 */
PB_StatusTypeDef    PausePlayback                     ( void );

/**
 * @brief Resume playback after pause with fade-in
 * @return PB_Playing on success, PB_Error if not paused
 * @note Applies resume fade time to smoothly restore volume
 */
PB_StatusTypeDef    ResumePlayback                    ( void );

/**
 * @brief Stop playback asynchronously with normal end-of-play fade-out
 * @return Current playback state, PB_Idle if already idle
 * @note Returns immediately. Use GetPlaybackState() to poll for completion.
 */
PB_StatusTypeDef    StopPlayback                      ( void );

/**
 * @brief Shut down all audio hardware (I2S, DAC, amplifier)
 * @note Call when shutting down application or before changing configurations
 */
void                ShutDownAudio                     ( void );

/* DAC power control */
/**
 * @brief Enable or disable DAC power control
 * @param[in] state 1 to enable DAC, 0 to disable DAC
 * @note Controls the optional DAC power management feature
 */
void                SetDAC_Control                    ( uint8_t state );

/**
 * @brief Get current DAC power control state
 * @return 1 if DAC control enabled, 0 if disabled
 */
uint8_t             GetDAC_Control                    ( void );

/* Volume response control */
/**
 * @brief Enable or disable non-linear volume response curve
 * @param[in] enable 1 to enable logarithmic response (recommended), 0 for linear
 * @note Non-linear response matches human loudness perception for intuitive control
 */
void                SetVolumeResponseNonlinear        ( uint8_t enable );

/**
 * @brief Get current volume response mode
 * @return 1 if non-linear mode enabled, 0 if linear
 */
uint8_t             GetVolumeResponseNonlinear        ( void );

/**
 * @brief Set volume response gamma exponent
 * @param[in] gamma Gamma value (1.0-4.0), typical: 2.0 for quadratic response
 * @note Higher gamma = more aggressive curve. 1.0 = linear, 2.0 = recommended
 */
void                SetVolumeResponseGamma            ( float gamma );

/**
 * @brief Get current volume response gamma exponent
 * @return Current gamma value
 */
float               GetVolumeResponseGamma            ( void );

/* Chunk processing callbacks (call from DMA callbacks) */
/**
 * @brief Process next 16-bit PCM chunk from DMA half-complete callback
 * @param[in,out] chunk_p Pointer to next chunk of 16-bit samples to process
 * @return PB_Playing while playback continues, PB_Idle when finished
 * @note Called from DMA ISR. Applies volume, filters, fade, clipping
 */
PB_StatusTypeDef    ProcessNextWaveChunk              ( int16_t *chunk_p );

/**
 * @brief Process next 8-bit PCM chunk from DMA half-complete callback
 * @param[in,out] chunk_p Pointer to next chunk of 8-bit samples to process
 * @return PB_Playing while playback continues, PB_Idle when finished
 * @note Called from DMA ISR. Applies volume, filters, fade, clipping
 */
PB_StatusTypeDef    ProcessNextWaveChunk_8_bit        ( uint8_t *chunk_p );

/**
 * @brief Advance sample pointer for next DMA transfer
 * @note Call from DMA complete callback after ProcessNextWaveChunk()
 */
void                AdvanceSamplePointer              ( void );

/* Air Effect runtime control */
/**
 * @brief Set air effect gain using Q16 fixed-point format
 * @param[in] gain_q16 Gain in Q16 format (65536 = 1.0x, 131072 = 2.0x max)
 * @note Enables air effect automatically if gain > 0
 */
void                 SetAirEffectGainQ16              ( uint32_t gain_q16 );

/**
 * @brief Get current air effect gain as Q16 fixed-point value
 * @return Current gain in Q16 format
 */
uint32_t             GetAirEffectGainQ16              ( void );

/**
 * @brief Set air effect gain using decibels
 * @param[in] db Gain in dB (0.0 = no gain, 6.0 dB = ~2x, -6.0 dB = ~0.5x)
 * @note Enables air effect automatically if db > 0
 */
void                 SetAirEffectGainDb               ( float db );

/**
 * @brief Get current air effect gain in decibels
 * @return Current gain in dB
 */
float                GetAirEffectGainDb               ( void );

/**
 * @brief Set air effect using predefined preset (automatically enables/disables)
 * @param[in] preset_index 0 = off, 1 = +1 dB, 2 = +2 dB, 3 = +3 dB (user can define more)
 * @return 1 if enabled (preset > 0), 0 if disabled (preset == 0)
 * @note Automatically calls SetAirEffectEnable(preset_index > 0 ? 1 : 0)
 */
void                 SetAirEffectPresetDb             ( uint8_t preset_index );

/**
 * @brief Cycle through available air effect presets
 * @return New preset index after cycling
 * @note Wraps from highest preset back to 0 (off). Useful for UI control.
 */
uint8_t              CycleAirEffectPresetDb           ( void );

/**
 * @brief Get the current air effect preset index
 * @return Current preset index (0 = off, 1-N = various dB levels)
 */
uint8_t              GetAirEffectPresetIndex          ( void );

/**
 * @brief Get total number of available air effect presets (including off)
 * @return Number of presets (typically 4 for off, +1dB, +2dB, +3dB)
 */
uint8_t              GetAirEffectPresetCount          ( void );

/**
 * @brief Get dB value for a specific air effect preset
 * @param[in] preset_index Preset index to query
 * @return dB gain for the preset (0.0 if preset_index is off or invalid)
 */
float                GetAirEffectPresetDb             ( uint8_t preset_index );


/* Hardware callbacks (to be called from I2S DMA callbacks) */
/**
 * @brief DMA half-complete callback for I2S
 * @note Called from DMA ISR when first half of buffer is complete
 * @note Application must call this from the GD32 DMA half-transfer ISR path
 */
void                 I2S_TxHalfCpltCallback          ( void );

/**
 * @brief DMA complete callback for I2S
 * @note Called from DMA ISR when entire buffer transfer is complete
 * @note Application must call this from the GD32 DMA full-transfer ISR path
 */
void                 I2S_TxCpltCallback             ( void );

/* Playback state accessors (for internal use or advanced applications) */
/**
 * @brief Get current playback state
 * @return PB_Idle, PB_Error, PB_Playing, PB_Paused, or PB_PlayingFailed
 */
PB_StatusTypeDef    GetPlaybackState                ( void );

/**
 * @brief Set playback state (internal use)
 * @param[in] state New playback state
 */
void                SetPlaybackState                ( PB_StatusTypeDef state );

/**
 * @brief Get which half of double-buffer is next to fill
 * @return FIRST (0) or SECOND (1)
 */
uint8_t             GetHalfToFill                   ( void );

/**
 * @brief Set which half of double-buffer to fill next (internal use)
 * @param[in] half FIRST or SECOND
 */
void                SetHalfToFill                   ( uint8_t half );

/**
 * @brief Get current playback sample rate
 * @return Sample rate in Hz (e.g., 22000, 44100)
 */
uint32_t            GetPlaybackSpeed                ( void );

/**
 * @brief Set playback sample rate (internal use)
 * @param[in] speed Sample rate in Hz
 */
void                SetPlaybackSpeed                ( uint32_t speed );

void                dma_config                      ( void );
void                spi_config                      ( uint32_t speed );

#ifdef __cplusplus
}
#endif

#endif /* __AUDIO_ENGINE_H */
