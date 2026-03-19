/**
  ******************************************************************************
  * @file           : audio_engine.c
  * @brief          : Audio playback engine and DSP filter chain implementation
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

#include "../main.h"
#include "audio_engine.h"
#include <gd32f30x.h>
#include <math.h>           // Some of our filter calculations need math functions
#include <stddef.h>         // For NULL definition
#include <stdint.h>         // We like things predictable in these here ports.
#include <string.h>         // Needed for memset
#include <stdbool.h>        // For true/false values in filter config, we like our C modern, clean and readable.

#define AUDIO_INT16_MAX             32767
#define AUDIO_INT16_MIN             (-32768)
#define Q16_SCALE                   65536U
#define Q16_SCALE_F                 65536.0f
#define SOFT_CLIP_THRESHOLD         28000
#define DITHER_SEED_DEFAULT         12345U
#define DEFAULT_VOLUME_INPUT        32U
#define NOISE_GATE_ATTENUATION_Q15  3277

#if AUDIO_ENGINE_INLINE_DMA_CALLBACK
#define DMA_CALLBACK_INLINE inline __attribute__((always_inline))
#else
#define DMA_CALLBACK_INLINE __attribute__((noinline))
#endif


/* Forward declarations for internal helper functions */

// Fade and volume helpers
static inline   void      UpdateFadeCounters          ( uint32_t samples_processed );
static          int16_t   ApplyFadeIn                 ( int16_t sample );
static          int16_t   ApplyFadeOut                ( int16_t sample );
static inline   int16_t   ApplyVolumeSetting          ( int16_t sample, uint16_t volume_setting );

// Noise reduction and gating
static          int16_t   ApplyNoiseGate              ( int16_t sample );

// Clipping
static          int16_t   ApplySoftClipping           ( int16_t sample );
static inline   int32_t   ComputeSoftClipCurve        ( int32_t excess, int32_t range );

// DC blocking filters
static inline   int16_t   ApplyDCFilterWithAlpha     (
                                                        volatile int16_t input,
                                                        volatile int32_t *prev_input,
                                                        volatile int32_t *prev_output,
                                                        uint32_t alpha_q16
                                                      );
static          int16_t   ApplyDCBlockingFilter       (
                                                        volatile int16_t input,
                                                        volatile int32_t *prev_input,
                                                        volatile int32_t *prev_output
                                                      );
static          int16_t   ApplySoftDCFilter16Bit      (
                                                        volatile int16_t input,
                                                        volatile int32_t *prev_input,
                                                        volatile int32_t *prev_output
                                                      );

// Filtering
static          int16_t   Apply8BitDithering          ( uint8_t sample8 );
static inline   uint16_t  GetLpf8BitAlpha             ( LPF_Level lpf_level );
static          int16_t   ApplyLowPassFilter16Bit     ( 
                                                        int16_t input,
                                                        volatile int32_t *x1,
                                                        volatile int32_t *x2,
                                                        volatile int32_t *y1,
                                                        volatile int32_t *y2
                                                      );
static          int16_t   ApplyLowPassFilter8Bit      (
                                                        int16_t sample,
                                                        volatile int32_t *y1
                                                      );
static inline   int16_t   ApplyPostFilters            ( int16_t sample, AudioChannelId channel_id );
static          int16_t   ApplyFilterChain16Bit       ( int16_t sample, AudioChannelId channel_id );
static          int16_t   ApplyFilterChain8Bit        ( int16_t sample, AudioChannelId channel_id );

// DMA stop helper
static inline   void      StopDmaAndResetPlaybackState( uint8_t reset_state );
static inline   void      PrepareForNewPlayback       ( void );
static inline   void      RetireCompletedDmaHalf      ( uint8_t completed_half );

// Default fader state
volatile uint8_t faders_enabled = 1;

/* Hardware interface function pointers (set by application) */
DAC_SwitchFunc  AudioEngine_DACSwitch   = NULL;
ReadVolumeFunc  AudioEngine_ReadVolume  = NULL;
I2S_InitFunc    AudioEngine_I2SInit     = NULL;

/* Weak callback function for playback end notification */
__attribute__((weak)) void AudioEngine_OnPlaybackEnd( void )
{
  /* Default implementation does nothing - override in application if needed */
}

/* Volume divisor (populated by hardware GPIO reading) */
volatile uint16_t vol_input;

/* Volume response configuration */
volatile uint8_t  volume_response_nonlinear = 1;                    // Default: enabled
volatile float    volume_response_gamma     = 2.0f;                 // Default: quadratic (human perception)

/* Playback buffer */
int16_t pb_buffer[ PB_BUFF_SZ ] = {SAMPLE16_MIDPOINT};              // Initialize to silence (midpoint for unsigned samples)

/* Filter configuration (runtime-tunable) */
volatile FilterConfig_TypeDef filter_cfg = {
  .enable_16bit_biquad_lpf      = 1,
  .enable_soft_dc_filter_16bit  = 1,
  .enable_8bit_lpf              = 1,
  .enable_noise_gate            = 0,
  .enable_soft_clipping         = 1,
  .enable_air_effect            = 0,
  .lpf_makeup_gain_q16          = LPF_MAKEUP_GAIN_Q16,
  .lpf_makeup_gain_16bit_q16    = LPF_16BIT_MAKEUP_GAIN_Q16,
  .lpf_16bit_level              = LPF_Custom,
  .lpf_16bit_custom_alpha       = LPF_16BIT_SOFT,
  .lpf_8bit_level               = LPF_Medium,
  .lpf_8bit_custom_alpha        = LPF_MEDIUM,
  .enable_filter_chain_16bit    = 1,
  .enable_filter_chain_8bit     = 1
};

/* Playback state variables */
  volatile  uint8_t         *pb_p8_ptr;                               // Pointer for 8-bit sample processing
volatile  uint8_t           *pb_end8_ptr;                             // End pointer for 8-bit sample processing
volatile  uint16_t          *pb_p16_ptr;                              // Pointer for 16-bit sample processing
volatile  uint16_t          *pb_end16_ptr;                            // End pointer for 16-bit sample processing

volatile  PB_StatusTypeDef  pb_state                    = PB_Idle;    // Playback state machine variable
volatile  uint8_t           half_to_fill;                             // Flag to indicate which half of the buffer to fill in the DMA callback
          uint8_t           pb_mode;                                  // Mono or stereo mode (set by application before playback)
          uint32_t          I2S_PlaybackSpeed           = 22025;      // Default playback speed in Hz
volatile  uint32_t          playback_total_samples      = 0;          // Total source samples scheduled for current playback session
volatile  uint32_t          playback_samples_played     = 0;          // Source samples already played by DMA
volatile  uint32_t          dma_half_sample_counts[ 2 ] = { 0U, 0U }; // Valid source samples currently queued in each DMA half

/* Playback engine control variables */
          uint32_t          p_advance;                                // Number of samples to advance in current buffer.
          PB_ModeTypeDef    channels                    = Mode_mono;  // Default to mono; set to Mode_stereo for stereo playback.
volatile  uint32_t          samples_remaining           = 0;          // Total samples remaining in current playback (used for tracking when to stop)
volatile  uint32_t          fadein_samples_remaining    = 0;          // Fade-in samples remaining, used for applying fade-in effect over specified duration
volatile  uint32_t          fadeout_samples_remaining   = 0;          // Fade-out samples remaining, used for applying fade-out effect over specified duration
volatile  uint32_t          paused_samples_remaining    = 0;          // Saved remaining samples at pause point (used to resume correctly)

/* Fade time configuration (stored in seconds, converted to samples based on playback speed) */
          float           fadein_time_seconds           = 0.150f;     // 150ms default
          float           fadeout_time_seconds          = 0.150f;     // 150ms default
          float           pause_fadeout_time_seconds    = 0.100f;     // 100ms default
          float           pause_fadein_time_seconds     = 0.100f;     // 100ms default
          uint32_t        fadein_samples                = 3300;       // Calculated fade in time from time and speed
          uint32_t        fadeout_samples               = 3300;       // Calculated fade out time from time and speed
          uint32_t        pause_fadeout_samples         = 2200;       // Calculated pause fade out time from time and speed
          uint32_t        pause_fadein_samples          = 2200;       // Calculated pause fade in time from time and speed

/* Per-channel state for filters that require memory of previous samples */
typedef struct AudioFilterChannelState {
  volatile int32_t dc_prev_input;
  volatile int32_t dc_prev_output;
  volatile int32_t lpf8_x1;
  volatile int32_t lpf8_x2;
  volatile int32_t lpf8_y1;
  volatile int32_t lpf8_y2;
  volatile int32_t lpf16_x1;
  volatile int32_t lpf16_x2;
  volatile int32_t lpf16_y1;
  volatile int32_t lpf16_y2;
  volatile int32_t air_x1;
  volatile int32_t air_y1;
} AudioFilterChannelState;

static AudioFilterChannelState filter_state[ CHANNEL_COUNT ] = {0}; // Initialize all filter state to zero

/* Dither state */
volatile  uint32_t        dither_state                = DITHER_SEED_DEFAULT;
          uint16_t        lpf_8bit_alpha              = LPF_MEDIUM;

/* Biquad filter state for 16-bit samples */
volatile  uint16_t        lpf_16bit_alpha             = LPF_16BIT_SOFT;

#if AUDIO_ENGINE_ENABLE_AIR_EFFECT
/* Air Effect runtime shelf gain (Q16). Defaults to AIR_EFFECT_SHELF_GAIN */
volatile  int32_t         air_effect_shelf_gain_q16   = AIR_EFFECT_SHELF_GAIN;

/* Air Effect preset table (dB) */
static const float        air_effect_presets_db[]     = { 1.0f, 2.0f, 3.0f };
#define AIR_EFFECT_PRESET_COUNT ( (uint8_t)( sizeof(air_effect_presets_db) / sizeof(air_effect_presets_db[0]) ) )
static volatile uint8_t   air_effect_preset_idx       = 1;          // default +2 dB
#endif

/* Pause/resume state tracking */
volatile  const void      *paused_sample_ptr          = NULL;       // Pointer to sample position where pause was initiated, used for resuming from same position

/* Stop request flag for asynchronous stop with fade-out */
volatile  uint8_t         stop_requested              = 0;          // Set to 1 to request a stop with fade-out, used for asynchronous stop handling in main loop

/* Playback end callback invocation guard - ensures callback is called only once */
volatile  uint8_t         playback_end_callback_called = 0;         // Flag to ensure playback end callback is only called once per playback session, prevents multiple invocations in edge cases

/* DAC power control flag */
volatile  uint8_t         dac_power_control           = true;       // Default to enabled


/* ===== Filter State Reset Helpers ===== */

/** Reset per-channel filter state to zero
  *
  * @param: state - Channel state to clear
  */
static inline void ResetFilterChannelState( AudioFilterChannelState *state )
{
  state->dc_prev_input = 0;
  state->dc_prev_output = 0;
  state->lpf8_x1 = 0;
  state->lpf8_x2 = 0;
  state->lpf8_y1 = 0;
  state->lpf8_y2 = 0;
  state->lpf16_x1 = 0;
  state->lpf16_x2 = 0;
  state->lpf16_y1 = 0;
  state->lpf16_y2 = 0;
  state->air_x1 = 0;
  state->air_y1 = 0;
}


/** Reset all per-channel filter state to zero
  *
  * @param: none
  */
static inline void ResetAllFilterState( void )
{
  ResetFilterChannelState( &filter_state[ CHANNEL_LEFT ] );
  ResetFilterChannelState( &filter_state[ CHANNEL_RIGHT ] );
}


/** Get pointer to channel filter state
  *
  * @param: channel_id - CHANNEL_LEFT or CHANNEL_RIGHT
  * @retval: AudioFilterChannelState* - Channel state pointer
  */
static inline AudioFilterChannelState *GetChannelState( AudioChannelId channel_id )
{
  return &filter_state[ channel_id ];
}


/* ===== Audio Engine Initialization ===== */

/** Initialize the audio engine with hardware interface functions
  * 
  * @brief Initializes the audio engine and validates that all required hardware
  *        interface functions have been provided. Resets all filter state and
  *        playback variables to safe defaults.
  * 
  * @param: dac_switch - Function pointer for DAC on/off control
  * @param: read_volume - Function pointer for reading volume setting
  * @param: i2s_init - Function pointer for I2S initialization
  * @retval: PB_Playing if initialization successful, PB_Error if any pointer is NULL
  */
PB_StatusTypeDef AudioEngine_Init( DAC_SwitchFunc dac_switch,
                                   ReadVolumeFunc read_volume,
                                   I2S_InitFunc i2s_init )
{
  /* Validate that all required function pointers are provided */
  if( dac_switch == NULL || read_volume == NULL || i2s_init == NULL ) {
    return PB_Error;
  }

  /* Assign hardware interface functions */
  AudioEngine_DACSwitch   = dac_switch;
  AudioEngine_ReadVolume  = read_volume;
  AudioEngine_I2SInit     = i2s_init;
  
  /* Reset all filter state variables to clean state */
  ResetAllFilterState();
  
  /* Reset playback state variables */
  pb_state                                  = PB_Idle;
  pb_mode                                   = 0;
  fadeout_samples_remaining                 = 0;
  fadein_samples_remaining                  = 0;
  paused_sample_ptr                         = NULL;
  vol_input                                 = DEFAULT_VOLUME_INPUT;  // Safe default above noise floor
  
  /* Reset dither state to non-zero seed */
  dither_state = DITHER_SEED_DEFAULT;
  
  /* Initialize default filter configuration */
  filter_cfg.enable_16bit_biquad_lpf        = 1;
  filter_cfg.enable_soft_dc_filter_16bit    = 1;
  filter_cfg.enable_8bit_lpf                = 1;
  filter_cfg.enable_noise_gate              = 0;
  filter_cfg.enable_soft_clipping           = 1;
  filter_cfg.enable_air_effect              = 0;
  filter_cfg.lpf_makeup_gain_q16            = LPF_MAKEUP_GAIN_Q16;
  filter_cfg.lpf_makeup_gain_16bit_q16      = LPF_16BIT_MAKEUP_GAIN_Q16;
  filter_cfg.lpf_16bit_level                = LPF_Soft;
  filter_cfg.lpf_8bit_level                 = LPF_Medium;
  filter_cfg.lpf_8bit_custom_alpha          = LPF_MEDIUM;
  filter_cfg.enable_filter_chain_16bit      = 1;
  filter_cfg.enable_filter_chain_8bit       = 1;
  
  return PB_Idle;  // Success - ready to play but not currently playing
}


/* ===== Filter Configuration Functions ===== */

/** Setup filter configuration 
  * 
  * @brief Sets the filter configuration parameters.
  * @param: cfg - Pointer to FilterConfig_TypeDef structure with desired settings.
  * @retval: none
  */
void SetFilterConfig( const FilterConfig_TypeDef *cfg )
{
  if( cfg != NULL ) {
    filter_cfg = *cfg;
    // Ensure a sane makeup gain (default if zero, cap if too high)
    if( filter_cfg.lpf_makeup_gain_q16 == 0 ) {
      filter_cfg.lpf_makeup_gain_q16 = LPF_MAKEUP_GAIN_Q16;
    }
    else if( filter_cfg.lpf_makeup_gain_q16 > AIR_EFFECT_SHELF_GAIN_MAX ) {
      filter_cfg.lpf_makeup_gain_q16 = AIR_EFFECT_SHELF_GAIN_MAX;
    }
    if( filter_cfg.lpf_makeup_gain_16bit_q16 == 0 ) {
      filter_cfg.lpf_makeup_gain_16bit_q16 = LPF_16BIT_MAKEUP_GAIN_Q16;
    }
    else if( filter_cfg.lpf_makeup_gain_16bit_q16 > AIR_EFFECT_SHELF_GAIN_MAX ) {
      filter_cfg.lpf_makeup_gain_16bit_q16 = AIR_EFFECT_SHELF_GAIN_MAX;
    }
  }
}


/** Get current filter configuration 
  * 
  * @brief Retrieves the current filter configuration parameters.
  * @param: cfg - Pointer to FilterConfig_TypeDef structure to populate.
  * @retval: none
  */
void GetFilterConfig( FilterConfig_TypeDef *cfg )
{
  if( cfg != NULL ) {
    *cfg = filter_cfg;
  }
}


/** Set whether or not to use soft clipping
  *
  * @brief Enables or disables the soft clipping filter.
  * @param: enabled - Non-zero to enable, zero to disable.
  * @retval: none
  */
void SetSoftClippingEnable( uint8_t enabled )
{
  filter_cfg.enable_soft_clipping = enabled ? 1 : 0;
}


/** Get whether or not soft clipping is enabled
  *
  * @brief Retrieves the current state of the soft clipping filter.
  * @param: none
  * @retval: non-zero if enabled, zero if disabled.
  */
uint8_t GetSoftClippingEnable( void )
{
  return filter_cfg.enable_soft_clipping;
}


/** Set the aggressiveness level for the 8-bit low-pass filter.
  * 
  * @brief Sets the filter level for the 8-bit biquad low-pass filter.
  * @param: level - Filter level (LPF_VerySoft, LPF_Soft, LPF_Medium, LPF_Firm, LPF_Aggressive, LPF_Custom).
  * @retval: none
  */
void SetLpf8BitLevel( LPF_Level level )
{
  if( level == LPF_Off ) {
    filter_cfg.enable_8bit_lpf = 0;
  } else {
    filter_cfg.enable_8bit_lpf = 1;
    if( level > LPF_Custom ) { // Prevent invalid off state
      level = LPF_VerySoft;
    }
  }
  filter_cfg.lpf_8bit_level = level;
}


/* Get the 8-bit low-pass filter aggressiveness level 
 * @param: none
 * @retval: current filter level
 */
LPF_Level GetLpf8BitLevel(void)
{
  return filter_cfg.lpf_8bit_level;
}


/** Set a custom alpha for the 8-bit low-pass filter
  * @param: alpha - Custom alpha value in Q16 format (0-65535)
  * @retval: none
  */
void SetLpf8BitCustomAlpha( uint16_t alpha )
{
  filter_cfg.lpf_8bit_custom_alpha = alpha;
  filter_cfg.lpf_8bit_level = LPF_Custom;
  filter_cfg.enable_8bit_lpf = 1;
  lpf_8bit_alpha = alpha;
}


/** Get the current custom alpha for the 8-bit low-pass filter
  * @param: none
  * @retval: current custom alpha in Q16 format
  */
uint16_t GetLpf8BitCustomAlpha( void )
{
  return filter_cfg.lpf_8bit_custom_alpha;
}


/** Set the aggressiveness level for the 16-bit biquad low-pass filter.
  * 
  * @brief Sets the filter level for the 16-bit biquad low-pass filter.
  * @param: level - Filter level (LPF_VerySoft, LPF_Soft, LPF_Medium, LPF_Firm, LPF_Aggressive, LPF_Custom).
  * @retval: none
  */
void SetFilterChain8BitEnable( uint8_t enabled )
{
  filter_cfg.enable_filter_chain_8bit = enabled ? 1 : 0;
}


/* Get whether the 8-bit filter chain is enabled 
 * @param: none
 * @retval: non-zero if enabled, zero if disabled
 */
uint8_t GetFilterChain8BitEnable( void )
{
  return filter_cfg.enable_filter_chain_8bit;
}


/* Set whether the 16-bit filter chain is enabled
 * @param: enabled - Non-zero to enable, zero to disable.
 * @retval: none
 */
void SetFilterChain16BitEnable( uint8_t enabled )
{
  filter_cfg.enable_filter_chain_16bit = enabled ? 1 : 0;
}


/* Get whether the 16-bit filter chain is enabled 
 * @param: none
 * @retval: non-zero if enabled, zero if disabled
 */
uint8_t GetFilterChain16BitEnable( void )
{
  return filter_cfg.enable_filter_chain_16bit;
}


/* Air Effect runtime control */

#if AUDIO_ENGINE_ENABLE_AIR_EFFECT
/** Sets whether to use the air effect or not
  * @param: enabled - Non-zero to enable, zero to disable.
  * @retval: none
  */
void SetAirEffectEnable( uint8_t enabled )
{
  filter_cfg.enable_air_effect = enabled ? 1 : 0;
}


/** Gets the state of the air effect enable flag
  * @param: none
  * @retval: non-zero if enabled, zero if disabled.
  */
uint8_t GetAirEffectEnable( void )
{
  return filter_cfg.enable_air_effect;
}


/** Air Effect runtime control: set shelf boost gain (Q16)
  * Clamps to AIR_EFFECT_SHELF_GAIN_MAX to avoid extreme boosts.
  * @param: gain_q16 - Desired shelf gain in Q16 format
  * @retval: none
  */
void SetAirEffectGainQ16( uint32_t gain_q16 )
{
  if( gain_q16 > AIR_EFFECT_SHELF_GAIN_MAX ) {
    gain_q16 = AIR_EFFECT_SHELF_GAIN_MAX;
  }
  air_effect_shelf_gain_q16 = (int32_t)gain_q16;
}


/** Air Effect runtime control: get current shelf boost gain (Q16)
  * @retval: current shelf gain in Q16
  */
uint32_t GetAirEffectGainQ16( void )
{
  return (uint32_t) air_effect_shelf_gain_q16;
}


/** Air Effect runtime control: set shelf boost using dB
  * Converts desired high-frequency boost (at ω=π) to internal G (Q16).
  * Formula: Hπ = 10^(db/20), G = (Hπ*(2-α) - α) / (2*(1-α))
  * @param: db - Desired boost in dB
  * @retval: none
  */
void SetAirEffectGainDb( float db )
{
  const float alpha               = (float)AIR_EFFECT_CUTOFF / Q16_SCALE_F;
  const float one_minus_alpha     = 1.0f - alpha;
  const float Hpi                 = powf( 10.0f, db / 20.0f );
  float G                         = (Hpi * ( 2.0f - alpha ) - alpha ) / ( 2.0f * one_minus_alpha );
  if( G < 0.0f ) G = 0.0f;
  uint32_t gain_q16               = (uint32_t)(G * Q16_SCALE_F + 0.5f);
  if( gain_q16 > AIR_EFFECT_SHELF_GAIN_MAX ) {
    gain_q16                      = AIR_EFFECT_SHELF_GAIN_MAX;
  }
  SetAirEffectGainQ16( gain_q16 );
}


/** Air Effect runtime control: get current shelf boost in dB (at ω=π)
  * Formula: Hπ = (α + 2(1-α)G) / (2-α), db = 20*log10(Hπ)
  * @retval: current boost in dB
  */
float GetAirEffectGainDb( void )
{
  const float alpha               = (float) AIR_EFFECT_CUTOFF / Q16_SCALE_F;
  const float one_minus_alpha     = 1.0f - alpha;
  const float G                   = (float) air_effect_shelf_gain_q16 / Q16_SCALE_F;
  const float Hpi                 = ( alpha + 2.0f * one_minus_alpha * G ) / ( 2.0f - alpha );
  return 20.0f * log10f( Hpi );
}


/** Air Effect preset selection by index
  * @param: preset_index - Index of desired preset (0 disables, >0 enables with that preset)
  * @retval: none
  */
void SetAirEffectPresetDb( uint8_t preset_index )
{
  if( preset_index >= AIR_EFFECT_PRESET_COUNT ) {
    preset_index = 0;
  }
  air_effect_preset_idx = preset_index;
  
  /* Auto-enable/disable air effect based on preset: 0 = off, >0 = on */
  SetAirEffectEnable( preset_index > 0 ? 1 : 0 );
  
  SetAirEffectGainDb( air_effect_presets_db[preset_index] );
}


/** Cycle to the next Air Effect preset. Returns the new preset index.
  * @retval: new preset index
  */
uint8_t CycleAirEffectPresetDb( void )
{
  uint8_t next = (uint8_t)( air_effect_preset_idx + 1 );
  if( next >= AIR_EFFECT_PRESET_COUNT ) {
    next = 0;
  }
  SetAirEffectPresetDb( next );
  return air_effect_preset_idx;
}


/** Get the current Air Effect preset index
  * @retval: current preset index
  */
uint8_t GetAirEffectPresetIndex( void )
{
  return air_effect_preset_idx;
}


/** Get the number of available Air Effect presets
 * @retval: preset count
  */
uint8_t GetAirEffectPresetCount( void )
{
  return AIR_EFFECT_PRESET_COUNT;
}


/** Get the dB value of a preset (clamps to current if OOB)
  * @param: preset_index - Index of desired preset
  * @retval: dB value of the preset
  */
float GetAirEffectPresetDb( uint8_t preset_index )
{
  if( preset_index  >=  AIR_EFFECT_PRESET_COUNT ) {
    preset_index    =   air_effect_preset_idx;
  }
  return air_effect_presets_db[preset_index];
}
#else   // If Air Effect is disabled at compile time, provide stubs that do nothing or return defaults.
void SetAirEffectEnable( uint8_t enabled )
{
  (void)enabled;
  filter_cfg.enable_air_effect = 0;
}

uint8_t GetAirEffectEnable( void )
{
  return 0;
}

void SetAirEffectGainQ16( uint32_t gain_q16 )
{
  (void)gain_q16;
}

uint32_t GetAirEffectGainQ16( void )
{
  return 0;
}

void SetAirEffectGainDb( float db )
{
  (void)db;
}

float GetAirEffectGainDb( void )
{
  return 0.0f;
}

void SetAirEffectPresetDb( uint8_t preset_index )
{
  (void)preset_index;
  filter_cfg.enable_air_effect = 0;
}

uint8_t CycleAirEffectPresetDb( void )
{
  return 0;
}

uint8_t GetAirEffectPresetIndex( void )
{
  return 0;
}

uint8_t GetAirEffectPresetCount( void )
{
  return 0;
}

float GetAirEffectPresetDb( uint8_t preset_index )
{
  (void)preset_index;
  return 0.0f;
}
#endif


/** Set LPF makeup gain 
  * 
  * @brief Sets the makeup gain applied after the low-pass filter.
  * @param: gain - Floating-point gain value (0.1 to 2.0).
  * @retval: none
  */
void SetLpfMakeupGain8Bit( float gain )
{
  if( gain < 0.1f ) {
    gain = 0.1f;
  } else if( gain > 2.0f ) {
    gain = 2.0f;
  }
  uint32_t q16 = (uint32_t)( gain * 65536.0f + 0.5f );
  filter_cfg.lpf_makeup_gain_q16 = q16;
}


/** Set 16-bit LPF makeup gain
  *
  * @brief Sets the makeup gain applied after the 16-bit low-pass filter. Default is 1.0 (No gain), lower limit: 0.1, upper limit: 2.0.
  * @param: gain - Floating-point gain value (0.1 to 2.0).
  * @retval: none
  */
void SetLpfMakeupGain16Bit( float gain )
{
  if( gain < 0.1f ) {
    gain = 0.1f;
  } else if( gain > 2.0f ) {
    gain = 2.0f;
  }
  uint32_t q16 = (uint32_t)( gain * 65536.0f + 0.5f );
  filter_cfg.lpf_makeup_gain_16bit_q16 = q16;
}


/** Get 16-bit LPF makeup gain
  *
  * @brief Returns the makeup gain applied after the 16-bit low-pass filter.
  * @retval: float - Linear gain value
  */
float GetLpfMakeupGain16Bit( void )
{
  return (float)filter_cfg.lpf_makeup_gain_16bit_q16 / 65536.0f;
}


/**
 * @brief Calculate Q16 alpha for 16-bit LPF from -3dB cutoff and sample rate
 *
 * 1-pole IIR: alpha = exp(-2*pi*fc/fs)
 * For biquad, this is a good starting point for the smoothing factor.
 *
 * @param cutoff_hz - Desired -3dB cutoff frequency in Hz
 * @param sample_rate_hz - Sample rate in Hz
 * @return Q16 alpha coefficient for SetLpf16BitCustomAlpha
 */
uint16_t CalcLpf16BitAlphaFromCutoff( float cutoff_hz, float sample_rate_hz )
{
  /* Validate input parameters */
  if( cutoff_hz <= 0.0f || sample_rate_hz <= 0.0f ) { return 0; }

  float alpha_f = expf( -2.0f * 3.14159265359f * cutoff_hz / sample_rate_hz );
  if( alpha_f < 0.0f ) alpha_f = 0.0f;
  if( alpha_f > 0.99998f ) alpha_f = 0.99998f; // avoid overflow
  return (uint16_t)( alpha_f * 65536.0f + 0.5f );
}


/**
 * @brief Calculate Q16 alpha for 8-bit LPF from -3dB cutoff and sample rate
 *
 * 1-pole IIR: alpha = 1 - exp(-2*pi*fc/fs)
 *
 * @param cutoff_hz - Desired -3dB cutoff frequency in Hz
 * @param sample_rate_hz - Sample rate in Hz
 * @return Q16 alpha coefficient for SetLpf8BitCustomAlpha
 */
uint16_t CalcLpf8BitAlphaFromCutoff( float cutoff_hz, float sample_rate_hz )
{
  /* Validate input parameters */
  if( cutoff_hz <= 0.0f || sample_rate_hz <= 0.0f ) { return 0; }

  float alpha_f = 1.0f - expf( -2.0f * 3.14159265359f * cutoff_hz / sample_rate_hz );
  if( alpha_f < 0.0f ) alpha_f = 0.0f;
  if( alpha_f > 0.99998f ) alpha_f = 0.99998f; // avoid overflow
  return (uint16_t)( alpha_f * 65536.0f + 0.5f );
}


/**
 * @brief Get Q16 alpha for 16-bit LPF custom mode from -3dB cutoff and current sample rate
 * @param cutoff_hz - Desired -3dB cutoff frequency in Hz
 * @return Q16 alpha coefficient
 */
uint16_t GetLpf16BitCustomAlphaFromCutoff( float cutoff_hz )
{
  return CalcLpf16BitAlphaFromCutoff( cutoff_hz, (float)I2S_PlaybackSpeed );
}


/**
 * @brief Calculate a sample offset from time, sample rate, and mode
 * @param seconds - Desired offset in seconds (>= 0)
 * @param sample_rate_hz - Sample rate in Hz
 * @param mode - Playback mode: Mode_mono or Mode_stereo
 * @return Sample offset in interleaved samples
 */
uint32_t CalcSampleOffsetSamples( float seconds, uint32_t sample_rate_hz, PB_ModeTypeDef mode )
{
  if( seconds <= 0.0f || sample_rate_hz == 0U ) {
    return 0U;
  }

  float samples_f = seconds * (float)sample_rate_hz;
  if( mode == Mode_stereo ) {
    samples_f *= 2.0f;
  }

  if( samples_f > (float)UINT32_MAX ) {
    return UINT32_MAX;
  }

  return (uint32_t)( samples_f + 0.5f );
}


/** Set 16-bit LPF filter level
  * 
  * @brief Sets the aggressiveness level for the 16-bit biquad low-pass filter.
  * @param: level - Filter level (LPF_VerySoft, LPF_Soft, LPF_Medium, LPF_Firm, LPF_Aggressive).
  * @retval: none
  */
void SetLpf16BitLevel( LPF_Level level )
{
  filter_cfg.lpf_16bit_level = level;
  switch( level ) {
    case LPF_Off:
      filter_cfg.enable_16bit_biquad_lpf = 0;
      break;
    case LPF_VerySoft:
      lpf_16bit_alpha = LPF_16BIT_VERY_SOFT;
      break;
    case LPF_Soft:
      lpf_16bit_alpha = LPF_16BIT_SOFT;
      break;
    case LPF_Medium:
      lpf_16bit_alpha = LPF_16BIT_MEDIUM;
      break;
    case LPF_Firm:
      lpf_16bit_alpha = LPF_16BIT_FIRM;
      break;
    case LPF_Aggressive:
      lpf_16bit_alpha = LPF_16BIT_AGGRESSIVE;
      break;
    case LPF_Custom:
      lpf_16bit_alpha = filter_cfg.lpf_16bit_custom_alpha;
      break;
    default:
      lpf_16bit_alpha = LPF_16BIT_SOFT;
      filter_cfg.lpf_16bit_level = LPF_Soft;
      break;
  }

  if( level != LPF_Off ) {
      filter_cfg.enable_16bit_biquad_lpf = 1;
  }
}


/** Set a custom alpha for the 16-bit low-pass filter
  * @param: alpha - Custom alpha value in Q16 format (0-65535)
  * @retval: none
  */
void SetLpf16BitCustomAlpha( uint16_t alpha )
{
    filter_cfg.lpf_16bit_custom_alpha = alpha;
    if( filter_cfg.lpf_16bit_level == LPF_Custom ) {
      lpf_16bit_alpha = filter_cfg.lpf_16bit_custom_alpha;
    }
}


/** Convert fade time in seconds to sample count
  * 
  * @brief Helper function to convert fade time to samples with bounds checking.
  * @param: seconds - Fade time in seconds (will be clamped to 0.001-5.0 range).
  * @retval: uint32_t - Number of samples (minimum 1).
  */
static uint32_t FadeTimeToSamples( float seconds )
{
  if( seconds < 0.001f ) seconds = 0.001f;
  if( seconds > 5.0f ) seconds = 5.0f;
  uint32_t samples = (uint32_t) ( seconds * (float) I2S_PlaybackSpeed + 0.5f );
  return ( samples == 0 ) ? 1 : samples;
}


/** Fader state Setter
  *
  * @brief Sets whether to apply fades or not.
  * @param: fader_setting - 1 is faders enabled, 0 is disabled.
  * @retval: none
  */
void SetFadersEnabled( uint8_t fader_setting )
{
  faders_enabled = fader_setting ? 1 : 0;
}


/** Fader state getter
  *
  * @brief Gets the the fader enable state
  * @param: none
  * @retval: fader_enabled state.
  */
uint8_t GetFadersEnabled( void )
{
  return faders_enabled;
}


/** Set fade-in time in seconds
  * 
  * @brief Sets the fade-in duration based on the current playback speed.
  * @param: seconds - Fade-in time in seconds (0.001 to 5.0).
  * @retval: none
  */
void SetFadeInTime( float seconds )
{
  fadein_time_seconds = seconds;
  fadein_samples      = FadeTimeToSamples( seconds );
}


/** Get fade-in time in seconds
  * 
  * @brief Returns the current fade-in duration in seconds.
  * @retval: float - Fade-in time in seconds.
  */
float GetFadeInTime( void )
{
  return fadein_time_seconds;
}


/** Set fade-out time in seconds
  * 
  * @brief Sets the fade-out duration based on the current playback speed.
  * @param: seconds - Fade-out time in seconds (0.001 to 5.0).
  * @retval: none
  */
void SetFadeOutTime( float seconds )
{
  fadeout_time_seconds = seconds;
  fadeout_samples      = FadeTimeToSamples( seconds );
}


/** Get fade-out time in seconds
  * 
  * @brief Returns the current fade-out duration in seconds.
  * @retval: float - Fade-out time in seconds.
  */
float GetFadeOutTime( void )
{
  return fadeout_time_seconds;
}


/** Set pause fade-out time in seconds
  * 
  * @brief Sets the pause fade-out duration based on the current playback speed.
  * @param: seconds - Pause fade-out time in seconds (0.001 to 5.0).
  * @retval: none
  */
void SetPauseFadeTime( float seconds )
{
  pause_fadeout_time_seconds = seconds;
  pause_fadeout_samples      = FadeTimeToSamples( seconds );
}


/** Get pause fade-out time in seconds
  * 
  * @brief Returns the current pause fade-out duration in seconds.
  * @retval: float - Pause fade-out time in seconds.
  */
float GetPauseFadeTime( void )
{
  return pause_fadeout_time_seconds;
}


/** Set resume fade-in time in seconds
  * 
  * @brief Sets the resume fade-in duration based on the current playback speed.
  * @param: seconds - Resume fade-in time in seconds (0.001 to 5.0).
  * @retval: none
  */
void SetResumeFadeTime( float seconds )
{
  pause_fadein_time_seconds = seconds;
  pause_fadein_samples      = FadeTimeToSamples( seconds );
}


/** Get resume fade-in time in seconds
  * 
  * @brief Returns the current resume fade-in duration in seconds.
  * @retval: float - Resume fade-in time in seconds.
  */
float GetResumeFadeTime( void )
{
  return pause_fadein_time_seconds;
}


/** Get the current playback speed
  * 
  * @param: none
  * @retval: uint32_t - Current playback speed in Hz
  */
uint32_t GetPlaybackSpeed( void )
{
    return I2S_PlaybackSpeed;
}


/** Get the number of source samples already played
  *
  * @retval: uint32_t - Number of interleaved source samples already consumed by DMA
  */
uint32_t GetPlaybackProgressSamples( void )
{
  return playback_samples_played;
}


/** Get playback progress as a percentage
  *
  * @retval: float - Playback progress in the range 0.0f to 100.0f
  */
float GetPlaybackProgressPercent( void )
{
  uint32_t total_samples  = playback_total_samples;
  uint32_t played_samples = playback_samples_played;

  if( total_samples == 0U ) {
    return 0.0f;
  }

  if( played_samples > total_samples ) {
    played_samples = total_samples;
  }

  return ( (float)played_samples * 100.0f ) / (float)total_samples;
}


/* ===== DSP Filter Functions ===== */

/* TPDF Dithering - Linear Congruential Generator (LCG) constants */
#define DITHER_LCG_MULTIPLIER       1103515245U   /* Standard POSIX LCG multiplier */
#define DITHER_LCG_INCREMENT        12345U        /* Standard POSIX LCG increment */
#define DITHER_RANDOM_BITS_SHIFT    16            /* Extract upper 16 bits of LCG state */
#define DITHER_RANDOM_MASK          0xFF          /* Mask to get 8-bit random value */
#define DITHER_SCALE_SHIFT          6             /* Scale dither from [-256, 256] to [-4, 4] */


/** Apply TPDF dithering during 8-bit to 16-bit conversion with cubic interpolation
  * 
  * Uses a fast Linear Congruential Generator for random noise generation.
  * 
  * @param: sample8 - Unsigned 8-bit audio sample
  * @retval: int16_t - Signed 16-bit dithered audio sample
  */
static int16_t Apply8BitDithering( uint8_t sample8 )
{
  // Convert unsigned 8-bit (0..255) to signed 16-bit
  int16_t sample16  = (int16_t)( sample8 - SAMPLE8_MIDPOINT ) << 8;
  
  // Generate TPDF (Triangular Probability Density Function) dither
  dither_state      = dither_state * DITHER_LCG_MULTIPLIER + DITHER_LCG_INCREMENT;
  int32_t rand1     = ( dither_state >> DITHER_RANDOM_BITS_SHIFT ) & DITHER_RANDOM_MASK;
  
  dither_state      = dither_state * DITHER_LCG_MULTIPLIER + DITHER_LCG_INCREMENT;
  int32_t rand2     = ( dither_state >> DITHER_RANDOM_BITS_SHIFT ) & DITHER_RANDOM_MASK;
  
  int16_t dither    = (int16_t)( ( rand1 - rand2 ) >> DITHER_SCALE_SHIFT );
  
  return sample16 + dither;
}


/** Apply low-pass biquad filter to 8-bit sample
  * 
  * @param: sample - Signed 16-bit audio sample (from 8-bit input)
  * @param: y2 - Pointer to previous output samples
  * @retval: int16_t - Filtered signed 16-bit audio sample
  */
static int16_t PUT_IN_FASTMEM ApplyLowPassFilter8Bit( int16_t sample, volatile int32_t *y1 )
{
  int32_t alpha = lpf_8bit_alpha;
  int32_t one_minus_alpha = (int32_t)( Q16_SCALE - alpha );
  int32_t output = ( ( alpha * sample) >> 16 ) + 
                   ( ( one_minus_alpha * ( *y1 ) ) >> 16 );
  // Apply makeup gain
  int64_t output64 = ( (int64_t)output * (int64_t)filter_cfg.lpf_makeup_gain_q16 ) >> 16;
  // Clamp to valid 16-bit range
  if( output64 > AUDIO_INT16_MAX ) output64 = AUDIO_INT16_MAX;
  if( output64 < AUDIO_INT16_MIN ) output64 = AUDIO_INT16_MIN;
  *y1 = (int32_t)output64;
  return (int16_t)output64;
}


/** Apply fade-in effect to audio sample
  * 
  * @param: sample - Signed 16-bit audio sample
  * @retval: int16_t - Faded-in signed 16-bit audio sample
  */
static int16_t PUT_IN_FASTMEM ApplyFadeIn( int16_t sample ) 
{
  if( fadein_samples_remaining > 0 ) {
    /* Calculate progress as a ratio of how much fade time has elapsed.
       The initial value of fadein_samples_remaining is our duration.
       We calculate fade_mult based on the ratio of remaining to initial. */
    
    /* Determine which fade duration to use as reference. */
    uint32_t fade_total = ( pb_state == PB_Playing && fadein_samples_remaining <= fadein_samples ) ? 
                          fadein_samples : fadein_samples_remaining;
    
    int32_t progress    = fade_total - fadein_samples_remaining;

    // Use 64-bit intermediate to prevent overflow when squaring progress
    int64_t fade_mult   = ( (int64_t)progress * (int64_t)progress ) / fade_total;
    int64_t result      = ( (int64_t)sample * fade_mult ) / fade_total;

    // Clamp to valid 16-bit range
    if( result > AUDIO_INT16_MAX ) result = AUDIO_INT16_MAX;
    if( result < AUDIO_INT16_MIN ) result = AUDIO_INT16_MIN;

    return (int16_t)result;
  }
  return sample;
}


/** Apply fade-out effect to audio sample
  * 
  * @param: sample - Signed 16-bit audio sample
  * @retval: int16_t - Faded-out signed 16-bit audio sample
  */
static int16_t ApplyFadeOut( int16_t sample )
{
  uint8_t should_apply_fade = 0;
  uint32_t fade_total       = 0;
  uint32_t remaining_to_use = 0;
  
  if( pb_state == PB_Pausing && fadeout_samples_remaining > 0 ) {
    /* Apply fadeout during explicit pause */
    should_apply_fade = 1;
    fade_total        = pause_fadeout_samples;
    remaining_to_use  = fadeout_samples_remaining;
  } else if( pb_state == PB_Playing ) {
    if( samples_remaining > 0 && samples_remaining <= fadeout_samples ) {
      should_apply_fade = 1;
      fade_total = fadeout_samples;
      remaining_to_use = samples_remaining;
    }
  }
  
  if( should_apply_fade ) {
    // Use 64-bit intermediate to prevent overflow when squaring remaining
    int64_t fade_mult   = ( (int64_t)remaining_to_use * (int64_t)remaining_to_use ) / fade_total;
    int64_t result      = ( (int64_t)sample * fade_mult ) / fade_total;

    // Clamp to valid 16-bit range
    if( result > AUDIO_INT16_MAX ) result = AUDIO_INT16_MAX;
    if( result < AUDIO_INT16_MIN ) result = AUDIO_INT16_MIN;

    return (int16_t)result;
  }
  return sample;
}


/** Apply noise gate to audio sample
  * 
  * @param: sample - Signed 16-bit audio sample
  * @retval: int16_t - Noise-gated signed 16-bit audio sample
  */
static int16_t ApplyNoiseGate( int16_t sample )
{
  int32_t sample32 = sample;
  int32_t abs_sample = ( sample32 < 0 ) ? -sample32 : sample32;
  if( abs_sample < NOISE_GATE_THRESHOLD ) {
    // Soft gate: attenuate signal using fixed-point integer math (Q15)
    // Example: 0.1 attenuation = 3277 (0.1 * 32768)
    const int16_t attenuation_q15 = NOISE_GATE_ATTENUATION_Q15; // ~0.1 in Q15
    return (int16_t)( ( sample * attenuation_q15 ) >> 15 );
  }
  return sample;
}


/** Update fade in/out counters
  * 
  * @param: samples_processed - Number of samples processed (1 for mono, 2 for stereo)
  */
static inline void PUT_IN_FASTMEM UpdateFadeCounters( uint32_t samples_processed )
{
  /* Track how many samples remain in the file */
  if( samples_remaining > 0 ) {
    samples_remaining = ( samples_remaining > samples_processed ) ? 
                        samples_remaining - samples_processed : 0;
  }
  
  /* Fade in: ramp up from silence */
  if( fadein_samples_remaining > 0 ) {
    fadein_samples_remaining = ( fadein_samples_remaining > samples_processed ) ? 
                               fadein_samples_remaining - samples_processed : 0;
  }
  
  /* Fade out: applied during pause or at end of normal playback */
  if( fadeout_samples_remaining > 0 ) {
    fadeout_samples_remaining = ( fadeout_samples_remaining > samples_processed ) ? 
                                fadeout_samples_remaining - samples_processed : 0;
  }
}


/** Warm up 16-bit biquad filter state to avoid startup transient
  * 
  * @param: sample - Initial sample value to use for warmup iterations
  */
static inline void WarmupBiquadFilter16Bit( int16_t sample )
{
  AudioFilterChannelState *left  = &filter_state[ CHANNEL_LEFT ];
  AudioFilterChannelState *right = &filter_state[ CHANNEL_RIGHT ];
  // Run multiple passes to let aggressive filters settle smoothly
  for( uint8_t i = 0; i < BIQUAD_WARMUP_CYCLES; i++ ) {
    ApplyLowPassFilter16Bit( sample, &left->lpf16_x1, &left->lpf16_x2,
                             &left->lpf16_y1, &left->lpf16_y2 );
    ApplyLowPassFilter16Bit( sample, &right->lpf16_x1, &right->lpf16_x2,
                             &right->lpf16_y1, &right->lpf16_y2 );
  }
}


/** Compute soft clipping curve response
  * 
  * Helper function that calculates the smooth clipping curve.
  * Uses a cubic smoothstep function to gradually limit excess signal.
  * 
  * @param: excess - Amount by which signal exceeds threshold
  * @param: range - Dynamic range available beyond threshold (max - threshold)
  * @retval: int32_t - Curve scaling factor (typically 0 to 65536)
  */
static inline int32_t ComputeSoftClipCurve( int32_t excess, int32_t range )
{
  int64_t x = ( (int64_t)excess * (int64_t)Q16_SCALE ) / range;
  if( x > (int64_t)Q16_SCALE ) x = (int64_t)Q16_SCALE;
  int64_t x2 = ( x * x )  >> 16;
  int64_t x3 = ( x2 * x ) >> 16;

  return (int32_t)( ( ( 3 * x2 ) >> 1 ) - ( ( 2 * x3 ) >> 1 ) );
}


/** Apply soft clipping to audio sample
  * 
  * @param: sample - Signed 16-bit audio sample
  * @retval: int16_t - Soft-clipped signed 16-bit audio sample
  */
static int16_t PUT_IN_FASTMEM ApplySoftClipping( int16_t sample )
{
  const int32_t threshold = SOFT_CLIP_THRESHOLD;
  const int32_t max_val   = AUDIO_INT16_MAX;
  
  int32_t s = sample;
  
  if( s > threshold ) {
    int32_t excess   = s - threshold;
    int32_t range    = max_val - threshold;
    int32_t curve    = ComputeSoftClipCurve( excess, range );
    s                = threshold + ( ( range * curve ) >> 16 );
  }
  else if( s < -threshold ) {
    int32_t excess   = -threshold - s;
    int32_t range    = max_val - threshold;
    int32_t curve    = ComputeSoftClipCurve( excess, range );
    s                = -threshold - ( ( range * curve ) >> 16 );
  }
  
  if( s > max_val )  s = max_val;
  if( s < -max_val ) s = -max_val;
  
  return (int16_t) s;
}


/** Apply a DC filter with a configurable alpha coefficient
  *
  * @param: input - Signed 16-bit audio sample
  * @param: prev_input - Pointer to previous input sample
  * @param: prev_output - Pointer to previous output sample
  * @param: alpha_q16 - Q16 alpha coefficient
  * @retval: int16_t - Filtered signed 16-bit audio sample
  */
static inline int16_t ApplyDCFilterWithAlpha(
                                              volatile int16_t input,
                                              volatile int32_t *prev_input,
                                              volatile int32_t *prev_output,
                                              uint32_t alpha_q16
                                            )
{
  int64_t filtered = ( (int64_t)( *prev_output ) * (int64_t)(int32_t)alpha_q16 ) >> DC_FILTER_SHIFT;
  int64_t output64 = (int64_t)input - (int64_t)( *prev_input ) + filtered;

  if( output64 > AUDIO_INT16_MAX ) output64 = AUDIO_INT16_MAX;
  if( output64 < AUDIO_INT16_MIN ) output64 = AUDIO_INT16_MIN;

  *prev_input  = input;
  *prev_output = (int32_t)output64;

  return (int16_t)output64;
}


/** Apply DC blocking filter to audio sample
  * 
  * @param: input - Signed 16-bit audio sample
  * @param: prev_input - Pointer to previous input sample
  * @param: prev_output - Pointer to previous output sample
  * @retval: int16_t - DC-blocked signed 16-bit audio sample
  */
static int16_t ApplyDCBlockingFilter(
                                      volatile int16_t input,
                                      volatile int32_t *prev_input,
                                      volatile int32_t *prev_output
                                    )
{
  return ApplyDCFilterWithAlpha( input, prev_input, prev_output, DC_FILTER_ALPHA );
}


/** Apply soft DC filter to audio sample
  * 
  * @param: input - Signed 16-bit audio sample
  * @param: prev_input - Pointer to previous input sample
  * @param: prev_output - Pointer to previous output sample
  * @retval: int16_t - Soft DC-filtered signed 16-bit audio sample
  */
static int16_t ApplySoftDCFilter16Bit (
                                        volatile int16_t input,
                                        volatile int32_t *prev_input,
                                        volatile int32_t *prev_output
                                      )
{
  return ApplyDCFilterWithAlpha( input, prev_input, prev_output, SOFT_DC_FILTER_ALPHA );
}


/** Apply low-pass biquad filter to 16-bit sample
  * 
  * @param: input - Signed 16-bit audio sample
  * @param: x1, x2 - Pointers to previous input samples
  * @param: y1, y2 - Pointers to previous output samples
  * @retval: int16_t - Filtered signed 16-bit audio sample
  */
static int16_t PUT_IN_FASTMEM 
                ApplyLowPassFilter16Bit(
                                        int16_t input,
                                        volatile int32_t *x1,
                                        volatile int32_t *x2,
                                        volatile int32_t *y1,
                                        volatile int32_t *y2
                                      )
{
  uint32_t alpha = (uint32_t)lpf_16bit_alpha;
  if( alpha >= Q16_SCALE ) {
    alpha = Q16_SCALE - 1U;
  }

  uint32_t one_minus_alpha = Q16_SCALE - alpha;
  int32_t b0 = (int32_t)( ( (int64_t)one_minus_alpha * (int64_t)one_minus_alpha ) >> 17 );
  int32_t b1 = b0 << 1;
  int32_t b2 = b0;
  int32_t a1 = -( (int32_t)alpha * 2 );
  int32_t a2 = (int32_t)( ( (int64_t)alpha * (int64_t)alpha ) >> 16 );
  /* Use 64-bit accumulation to avoid overflow on aggressive coefficients */
  int64_t acc = ( (int64_t)b0 * input ) +
                ( (int64_t)b1 * (*x1) ) +
                ( (int64_t)b2 * (*x2) ) -
                ( (int64_t)a1 * (*y1) ) -
                ( (int64_t)a2 * (*y2) );
  int64_t output_pre = acc >> 16;
  if( output_pre > INT32_MAX ) output_pre = INT32_MAX;
  if( output_pre < INT32_MIN ) output_pre = INT32_MIN;
  int32_t output = (int32_t)output_pre;
  *x2 = *x1;
  *x1 = input;
  *y2 = *y1;
  *y1 = output;
  // Apply 16-bit LPF makeup gain
  int64_t output64 = ( (int64_t)output * (int64_t)filter_cfg.lpf_makeup_gain_16bit_q16 ) >> 16;
  // Clamp to valid 16-bit range
  if ( output64 > AUDIO_INT16_MAX ) output64 = AUDIO_INT16_MAX;
  if ( output64 < AUDIO_INT16_MIN ) output64 = AUDIO_INT16_MIN;
  output = (int32_t)output64;
  return (int16_t)output;
}


/** Apply Air Effect (High-Shelf Brightening) to 16-bit sample
  * 
  * One-pole high-shelf filter to add brightness/presence to audio.
  * Boosts frequencies above cutoff (~5-6 kHz @ 22 kHz sample rate).
  * 
  * @param: input - Signed 16-bit audio sample
  * @param: x1 - Pointer to previous input sample
  * @param: y1 - Pointer to previous output sample
  * @retval: int16_t - Filtered signed 16-bit audio sample
  */
#if AUDIO_ENGINE_ENABLE_AIR_EFFECT
static int16_t PUT_IN_FASTMEM ApplyAirEffect( int16_t input, volatile int32_t *x1, volatile int32_t *y1 )
{
  // Air effect uses high-shelf filter to brighten treble
  int32_t alpha               = AIR_EFFECT_CUTOFF;              // ~0.75
  int32_t one_minus_alpha     = (int32_t)( Q16_SCALE - alpha ); // ~0.25
  int32_t shelf_gain          = air_effect_shelf_gain_q16;      // runtime-adjustable boost
  
  // High-pass portion: amplify high frequencies
  // Use 64-bit intermediates to prevent overflow when multiplying Q16 terms
  int32_t high_freq           = (int32_t)input - *x1;
  int64_t air_boost64         = ( ( (int64_t)high_freq * (int64_t)one_minus_alpha ) >> 16 );
  air_boost64                 = ( ( air_boost64 * (int64_t)shelf_gain ) >> 16 );
  int32_t air_boost           = (int32_t)air_boost64;
  
  // Output = low-pass + high-pass boost
  int64_t out64   = ( ( (int64_t)alpha * (int64_t)input ) >> 16 ) +
                  ( ( ( (int64_t)(65536 - alpha) * (int64_t)( *y1 ) ) >> 16 ) ) +
                  (int64_t)air_boost;
  int32_t output  = (int32_t)out64;
  
  *x1 = input;
  *y1 = output;
  
  if ( output > AUDIO_INT16_MAX ) output = AUDIO_INT16_MAX;
  if ( output < AUDIO_INT16_MIN ) output = AUDIO_INT16_MIN;
  
  return (int16_t)output;
}
#endif


/** Apply full filter chain to 16-bit sample
  * 
  * @param: sample - Signed 16-bit audio sample
  * @param: channel_id - CHANNEL_LEFT or CHANNEL_RIGHT
  * @retval: int16_t - Processed signed 16-bit audio sample
  */
static int16_t PUT_IN_FASTMEM ApplyFilterChain16Bit( int16_t sample, AudioChannelId channel_id )
{
  AudioFilterChannelState *channel = GetChannelState( channel_id );
  if( filter_cfg.enable_16bit_biquad_lpf ) {
    sample = ApplyLowPassFilter16Bit( sample, &channel->lpf16_x1, &channel->lpf16_x2,
                                      &channel->lpf16_y1, &channel->lpf16_y2 );
  }
  
  return ApplyPostFilters( sample, channel_id );
}


/** Apply full filter chain to 8-bit sample
  * 
  * @param: sample - Signed 16-bit audio sample
  * @param: channel_id - CHANNEL_LEFT or CHANNEL_RIGHT
  * @retval: int16_t - Processed signed 16-bit audio sample
  */
static int16_t PUT_IN_FASTMEM ApplyFilterChain8Bit( int16_t sample, AudioChannelId channel_id )
{
  AudioFilterChannelState *channel = GetChannelState( channel_id );
  if( filter_cfg.enable_8bit_lpf ) {
    sample = ApplyLowPassFilter8Bit( sample, &channel->lpf8_y1 );
  }
  
  return ApplyPostFilters( sample, channel_id );
}


/** Apply post-LPF filters common to 8-bit and 16-bit chains
  *
  * @param: sample - Signed 16-bit audio sample
  * @param: channel_id - CHANNEL_LEFT or CHANNEL_RIGHT
  * @retval: int16_t - Processed signed 16-bit audio sample
  */
static inline int16_t PUT_IN_FASTMEM ApplyPostFilters( int16_t sample, AudioChannelId channel_id )
{
  AudioFilterChannelState *channel = GetChannelState( channel_id );
  volatile int32_t *dc_prev_input  = &channel->dc_prev_input;
  volatile int32_t *dc_prev_output = &channel->dc_prev_output;
  if( filter_cfg.enable_soft_dc_filter_16bit ) {
    sample = ApplySoftDCFilter16Bit( sample, dc_prev_input, dc_prev_output );
  } else {
    sample = ApplyDCBlockingFilter( sample, dc_prev_input, dc_prev_output );
  }
  
#if AUDIO_ENGINE_ENABLE_AIR_EFFECT
  if( filter_cfg.enable_air_effect ) {
    sample = ApplyAirEffect( sample, &channel->air_x1, &channel->air_y1 );
  }
#endif
  
  if( filter_cfg.enable_noise_gate ) {
    sample = ApplyNoiseGate( sample );
  }
  
  if( filter_cfg.enable_soft_clipping ) {
    sample = ApplySoftClipping( sample );
  }
  
  return sample;
}


/** Reset playback state variables to idle condition.
  * 
  * Resets mode, pointers, and counters.
  */
static void ResetPlaybackState( void ) {
  pb_mode                       = 0;
  pb_p8_ptr                     = NULL;
  pb_end8_ptr                   = NULL;
  pb_p16_ptr                    = NULL;
  pb_end16_ptr                  = NULL;
  paused_sample_ptr             = NULL;
  samples_remaining             = 0;
  fadeout_samples_remaining     = 0;
  fadein_samples_remaining      = 0;
  paused_samples_remaining      = 0;
  half_to_fill                  = FIRST;
  playback_total_samples        = 0;
  playback_samples_played       = 0;
  dma_half_sample_counts[ FIRST ] = 0U;
  dma_half_sample_counts[ SECOND ] = 0U;
  stop_requested                = 0;
  playback_end_callback_called  = 0;
}


/** Prepare engine and transport state for a deterministic playback start.
  *
  * Ensures DMA is halted, playback state is reset, filter memory is cleared,
  * and the output buffer is filled with silence before prefill begins.
  */
static inline void PrepareForNewPlayback( void )
{
  dma_interrupt_disable( DMA0, DMA_CH4, DMA_INT_HTF | DMA_INT_FTF );
  spi_dma_disable( PROJECT_SPI, SPI_DMA_TRANSMIT );   // FINDME
  i2s_disable( PROJECT_SPI );
  ResetPlaybackState();
  ResetAllFilterState();
  MIDPOINT_FILL_BUFFER();
  pb_state = PB_Idle;
}


/* ===== State Accessors ===== */

/** Get current playback state
  * 
  * @param: none
  * @retval: PB_StatusTypeDef - Current playback state (PB_Idle, PB_Playing, PB_Paused)
  */
PB_StatusTypeDef GetPlaybackState( void )
{
  return pb_state;
}


/** Set current playback state
  * 
  * @param: state - Desired playback state (PB_Idle, PB_Playing, PB_Paused)
  * @retval: none
  */
void SetPlaybackState( PB_StatusTypeDef state )
{
  pb_state = state;
}


/** DAC Control Setter
 *
 * @param: enable - 1 to enable DAC output, 0 to disable
 * @retval: none
 */
 void SetDAC_Control( uint8_t state )
 {
  dac_power_control = state;
 }


/** DAC Control Getter *
 * @param: none
 * @retval: uint8_t - Current DAC control state (1 for enabled, 0 for disabled)
 */
 uint8_t GetDAC_Control( void )
 {
  return dac_power_control;
 }
 

/** Get which half of the buffer to fill
  * 
  * @param: none
  * @retval: uint8_t - FIRST or SECOND half indicator
  */
uint8_t GetHalfToFill( void )
{
  return half_to_fill;
}


/** Set which half of the buffer to fill
  * 
  * @param: half - FIRST or SECOND half indicator
  * @retval: none
  */
void SetHalfToFill( uint8_t half )
{
  half_to_fill = half;
}


/** Set current playback speed
  * 
  * @brief Sets the playback speed for sample processing.  Not to be use mid-playback.
  * @param: speed - Desired playback speed in Hz
  * @retval: none
  */
void SetPlaybackSpeed( uint32_t speed )
{
  I2S_PlaybackSpeed = speed;
}


/* ============================================================================
 * DMA Callbacks
 * ============================================================================
 */

/* Static inline helpers */

/** Helper to recalculate fade sample counts based on current fade times
  *
  * Should be called whenever fade times or playback speed are changed to ensure fade
  * durations remain consistent.
  */
static inline void RecalculateFadeSamples( void )
{
  fadein_samples        = FadeTimeToSamples( fadein_time_seconds );
  fadeout_samples       = FadeTimeToSamples( fadeout_time_seconds );
  pause_fadeout_samples = FadeTimeToSamples( pause_fadeout_time_seconds );
  pause_fadein_samples  = FadeTimeToSamples( pause_fadein_time_seconds );
}


/** Clean up after playback ends
  *
  * Common cleanup logic for when playback finishes naturally or when a stop request is processed.
  * Resets state, fills buffer with silence, halts DMA, and calls the playback
  * end callback if it hasn't been called already.
  */
static inline void EndPlaybackCleanup( void )
{
  pb_state = PB_Idle;
  MIDPOINT_FILL_BUFFER();
  StopDmaAndResetPlaybackState( 1U );
  if( !playback_end_callback_called ) {
    playback_end_callback_called = 1;
    AudioEngine_OnPlaybackEnd();
    if( dac_power_control == true ) {
      AudioEngine_DACSwitch( 0 );
    }
  }
}


/** Stop DMA transmission and optionally reset playback state
  *
  * @param: reset_state - Non-zero to reset playback state
  * @retval: none
  */
static inline void StopDmaAndResetPlaybackState( uint8_t reset_state )
{
  dma_interrupt_disable( DMA0, DMA_CH4, DMA_INT_HTF | DMA_INT_FTF );
  spi_dma_disable( PROJECT_SPI, SPI_DMA_TRANSMIT );   // FINDME
  i2s_disable( PROJECT_SPI );
  if( reset_state ) {
    ResetPlaybackState();
  }
}


/** Retire the samples that were just played from a completed DMA half
  *
  * @param: completed_half - FIRST or SECOND
  * @retval: none
  */
static inline void RetireCompletedDmaHalf( uint8_t completed_half )
{
  uint32_t completed_samples = dma_half_sample_counts[ completed_half ];
  uint32_t remaining_samples = ( playback_total_samples > playback_samples_played ) ?
                               ( playback_total_samples - playback_samples_played ) : 0U;

  if( completed_samples > remaining_samples ) {
    completed_samples = remaining_samples;
  }

  playback_samples_played += completed_samples;
  dma_half_sample_counts[ completed_half ] = 0U;
}


/** Immediate stop of playback, halting DMA and resetting state without waiting for buffer to drain 
  *
  * Use when stopping from paused state or when an immediate halt is required.  Will not apply fade-out.
  */
static inline void StopImmediate( void )
{
  pb_state = PB_Idle;
  StopDmaAndResetPlaybackState( 1U );
  MIDPOINT_FILL_BUFFER();
  if( !playback_end_callback_called ) {
    playback_end_callback_called = 1;
    AudioEngine_OnPlaybackEnd();
    if( dac_power_control == true ) {
      AudioEngine_DACSwitch( DAC_OFF );
    }
  }
}


/** Common DMA callback processing logic
  *
  * Helper function to handle buffer refilling for both half-complete and 
  * complete DMA callbacks. Eliminates duplication between the two callbacks.
  *
  * @param: which_half - Which half of buffer to fill (FIRST or SECOND)
  * @retval: none
  */
static DMA_CALLBACK_INLINE void ProcessDMACallback( uint8_t which_half )
{
  if( which_half == FIRST ) {
    dma_interrupt_flag_clear( DMA0, DMA_CH4, DMA_INT_FLAG_HTF );
  } else {
    dma_interrupt_flag_clear( DMA0, DMA_CH4, DMA_INT_FLAG_FTF );
  }
  
  RetireCompletedDmaHalf( which_half );
  /* Handle pending stop request at the beginning of DMA callback (safest place to modify state) */
  if( stop_requested && pb_state != PB_Idle ) {
    /* Only handle stop if we're in a playable state */
    if( pb_state == PB_Paused ) {
      /* If paused, stop immediately */
      StopImmediate();
      return;
    }
    
    /* For playing states, initiate fade-out by shortening the sample duration */
    if( pb_state != PB_Pausing ) {
      /* If not already pausing, set state to pausing and prepare for fade */
      pb_state = PB_Pausing;
      if( pb_mode == 16 ) {
        ptrdiff_t remaining = pb_end16_ptr - pb_p16_ptr;
        if( remaining <= 0 ) {
          EndPlaybackCleanup();
          return;
        }
        if( (uint32_t)remaining > fadeout_samples ) {
          pb_end16_ptr = pb_p16_ptr + fadeout_samples;
        }
      } else if( pb_mode == 8 ) {
        ptrdiff_t remaining = pb_end8_ptr - pb_p8_ptr;
        if( remaining <= 0 ) {
          EndPlaybackCleanup();
          return;
        }
        if( (uint32_t)remaining > fadeout_samples ) {
          pb_end8_ptr = pb_p8_ptr + fadeout_samples;
        }
      }
    }
  }
  
  /* If fully paused (fadeout already complete), fill buffer with silence */
  if( pb_state == PB_Paused ) {
    MIDPOINT_FILL_BUFFER();
    dma_half_sample_counts[ which_half ] = 0U;
    return;
  }
  
  /* Special case: if pausing and fadeout nearly complete, skip processing and fill with silence */
  if( pb_state == PB_Pausing && fadeout_samples_remaining <= HALFCHUNK_SZ ) {
    MIDPOINT_FILL_BUFFER();
    dma_half_sample_counts[ which_half ] = 0U;
    pb_state = PB_Paused;
    return;
  }

  half_to_fill = which_half;

  if( pb_mode == 16 || pb_mode == 8 ) {
    if( ( pb_mode == 16 && pb_p16_ptr >= pb_end16_ptr ) ||
        ( pb_mode == 8  && pb_p8_ptr  >= pb_end8_ptr )
      ) {
      EndPlaybackCleanup();   // Cleanup and stop playback if we've reached the end of the sample data.
      return;
    }
    /* Only one chunk process will be used because of short-circuit evaluation. */
    if( ( pb_mode == 16 && ProcessNextWaveChunk( (int16_t *) pb_p16_ptr ) != PB_Playing ) ||
        ( pb_mode == 8  && ProcessNextWaveChunk_8_bit( (uint8_t *) pb_p8_ptr ) != PB_Playing ) ) {
      return;
    }
  } else {
    MIDPOINT_FILL_BUFFER();
    return;
  }
  
  AdvanceSamplePointer();
}


/** Handle refilling the first half of the buffer whilst the second half is playing
  *
  * params: hi2s_p I2S port handle.
  * retval: none.
  *
  * NOTE: Also shuts down the playback when the recording is done.
  *
  */
void DMA0_Channel4_IRQHandler( void )
{
  FlagStatus dma_done, dma_half_done;

  dma_done        = dma_flag_get( DMA0, DMA_CH4, DMA_FLAG_FTF );
  dma_half_done   = dma_flag_get( DMA0, DMA_CH4, DMA_FLAG_HTF );


  if( dma_half_done == SET ) {
    ProcessDMACallback( FIRST );
  }
  else if( dma_done == SET ) {
    ProcessDMACallback( SECOND );
  }
}


/** Advance the sample pointer based on playback mode and advance value
  *
  * params: none.
  * retval: none.
  *
  */
void AdvanceSamplePointer( void )
{
  if( pb_mode == 16 ) {  // Advance the 16-bit sample pointer
    pb_p16_ptr += p_advance;
    if( pb_p16_ptr >= pb_end16_ptr ) {
      StopImmediate();
      pb_state = PB_Idle;
      return;
    }
  }
  else if( pb_mode == 8 ) {  // Or advance the 8-bit sample pointer
    pb_p8_ptr += p_advance;
    if( pb_p8_ptr >= pb_end8_ptr ) {
      StopImmediate();
      pb_state = PB_Idle;
      return;
    }
  } 
}


/* ============================================================================
 * Chunk Processing
 * ============================================================================
 */

/** Transfers a chunk of 16-bit samples to the DMA playback buffer and processes them
  *
  * It also processes for stereo mode
  *
  * @param: int16_t* chunk_p.  The start of the chunk to transfer.
  * @retval: none.
  *
  */
PB_StatusTypeDef ProcessNextWaveChunk( int16_t * chunk_p )
{
  int16_t *input, *output;
  int16_t leftsample, rightsample;
  uint16_t current_volume;
  uint32_t chunk_source_samples = 0U;

  if( chunk_p == NULL ) {   // Sanity check
    return PB_Error;
  }

  vol_input = AudioEngine_ReadVolume();

  input   = chunk_p;      // Source sample pointer
  output  = ( half_to_fill == SECOND ) ? ( pb_buffer + CHUNK_SZ ) : pb_buffer;

  // Transfer mono audio (scaled for volume) into the stereo buffer.
  // This is done both to eliminate the need for a second resistor
  // and also because the MAX983567A expects stereo audio data, but we only
  // have one speaker.
  //
  for( uint16_t i = 0; i < HALFCHUNK_SZ; i++ )
  {
    current_volume = AudioEngine_ReadVolume();
    vol_input = current_volume;

    if( (uint16_t *) input >=  pb_end16_ptr ) {                                   // Check for end of sample data
      leftsample = SAMPLE16_MIDPOINT;                                             // Pad with silence if at end 
    }
    else {
      chunk_source_samples++;
      leftsample = ApplyVolumeSetting( *input, current_volume );                  // Apply volume setting
      if( filter_cfg.enable_filter_chain_16bit == 1 ) {
        leftsample = ApplyFilterChain16Bit( leftsample, CHANNEL_LEFT );            // Apply complete filter chain
      }
      if( faders_enabled ) {
        leftsample = ApplyFadeIn( leftsample );
        leftsample = ApplyFadeOut( leftsample );
      }
    }
    input++;

    if( channels == Mode_mono ) { rightsample = leftsample; }                     // Right channel is the same as left.
    else {
      if( (uint16_t *) input >=  pb_end16_ptr ) {                                 // Check for end of sample data
        rightsample = SAMPLE16_MIDPOINT;                                          // Pad with silence if at end
      }
      else { 
        chunk_source_samples++;
        rightsample = ApplyVolumeSetting( *input, current_volume );               // Right channel
        if( filter_cfg.enable_filter_chain_16bit == 1 ) {
          rightsample = ApplyFilterChain16Bit( rightsample, CHANNEL_RIGHT );       // Apply complete filter chain
        }
        if( faders_enabled ) {
          rightsample = ApplyFadeIn( rightsample );
          rightsample = ApplyFadeOut( rightsample );
        }
      }   // End of right channel processing
      input++;
    }
    *output = leftsample;  output++;                                              // Write samples to output buffer
    *output = rightsample; output++;
    
    // Update fade counters based on samples processed
    uint32_t samples_processed = ( channels == Mode_stereo ) ? 2 : 1;

    UpdateFadeCounters( samples_processed );
  }
  dma_half_sample_counts[ half_to_fill ] = chunk_source_samples;
  return PB_Playing;;
}


/** Transfers a chunk of 8-bit samples to the DMA playback buffer
  *
  * It also processes for stereo mode
  *
  * @param: uint8_t* chunk_p.  The start of the chunk to transfer.
  * @retval: none.
  *
  */
PB_StatusTypeDef ProcessNextWaveChunk_8_bit( uint8_t * chunk_p )
{
  uint8_t *input;
  int16_t *output;
  int16_t leftsample, rightsample;
  uint16_t current_volume;
  uint32_t chunk_source_samples = 0U;

  if( chunk_p == NULL ) {   // Sanity check
    return PB_Error;
  }

  vol_input = AudioEngine_ReadVolume();

  input   = chunk_p;                                                        // Source sample pointer
  output  = ( half_to_fill == SECOND ) ? ( pb_buffer + CHUNK_SZ ) : pb_buffer;

  // Transfer mono audio (scaled for volume) into the stereo buffer.
  // This is done both to eliminate the need for a second resistor
  // and also because the MAX983567A expects stereo audio data, but we only
  // have one speaker.

  for( uint16_t i = 0; i < HALFCHUNK_SZ; i++ )
  {
    current_volume = AudioEngine_ReadVolume();
    vol_input = current_volume;

    if( (uint8_t *) input >=  pb_end8_ptr ) {                               // Check for end of sample data
      leftsample = SAMPLE16_MIDPOINT;                                       // Pad with silence if at end
    }
    else {
      chunk_source_samples++;
      /* Convert unsigned 8-bit (0..255) -> signed 16-bit with dithering */
      uint8_t sample8 = *input;
      leftsample = Apply8BitDithering( sample8 );                           // Left channel with dithering
      leftsample = ApplyVolumeSetting( leftsample, current_volume );
      if( filter_cfg.enable_filter_chain_8bit == 1 ) {
        leftsample = ApplyFilterChain8Bit( leftsample, CHANNEL_LEFT );       // Apply complete filter chain
      }
      if( faders_enabled ) {
        leftsample = ApplyFadeIn( leftsample );
        leftsample = ApplyFadeOut( leftsample );
      }
    }
    input++;

    if( channels == Mode_mono ) { rightsample = leftsample; }               // Right channel is the same as left.
    else {    
      if( (uint8_t *) input >=  pb_end8_ptr ) {                             // Check for end of sample data
        rightsample = SAMPLE16_MIDPOINT;                                    // Pad with silence if at end
      }
      else {               
        chunk_source_samples++;
        /* Convert unsigned 8-bit (0..255) -> signed 16-bit with dithering */
        uint8_t sample8 = *input;
        rightsample = Apply8BitDithering( sample8 );                        // Right channel with dithering
        rightsample = ApplyVolumeSetting( rightsample, current_volume );
        if( filter_cfg.enable_filter_chain_8bit == 1 ) {
          rightsample = ApplyFilterChain8Bit( rightsample, CHANNEL_RIGHT );   // Apply complete filter chain
        }
        if( faders_enabled ) {
          rightsample = ApplyFadeIn( rightsample );
          rightsample = ApplyFadeOut( rightsample );
        }
      }
      input++;
    }
    *output = leftsample;  output++;                                        // Transfer samples to output buffer
    *output = rightsample; output++;
    
    // Update fade counters based on samples processed
    uint32_t samples_processed = ( channels == Mode_stereo ) ? 2 : 1;

    UpdateFadeCounters( samples_processed );
  }
  dma_half_sample_counts[ half_to_fill ] = chunk_source_samples;
  return PB_Playing;
}


/* ============================================================================
 * Playback Control Functions
 * ============================================================================
 */

/** Initiates playback of your specified sample
  *
  * @param: const void* sample_to_play.  Pointer to audio sample data (8-bit or 16-bit).
  * @param: uint32_t sample_set_sz.  Total samples (all channels combined).
  * @param: uint32_t playback_speed.  This is the sample rate.
  * @param: uint8_t sample depth.  This should be 8 or 16 bits.
  * @param: PB_ModeTypeDef mode.  Mono or stereo playback.
  * @retval: PB_StatusTypeDef.  Indicates success or failure.
  *
  */
PB_StatusTypeDef PlaySample (  
                              const void *sample_to_play, 
                              uint32_t sample_set_sz, 
                              uint32_t playback_speed, 
                              uint8_t sample_depth, 
                              PB_ModeTypeDef mode 
                            ) 
{
  // Parameter sanity checks
  //
  if( ( sample_depth != 16 && sample_depth != 8 ) || 
      ( mode != Mode_mono && mode != Mode_stereo) ||
        sample_set_sz   == 0                      ||
        sample_to_play  == NULL
    ) { return PB_Error; }

  // Ensure volume callback is initialized before starting playback
  if( AudioEngine_ReadVolume == NULL ) {
    return PB_Error;
  }

  // Set low-pass filter alpha coefficient based on filter config
  lpf_8bit_alpha = GetLpf8BitAlpha( filter_cfg.lpf_8bit_level );
  
  if( mode == Mode_stereo ) {                             // Pointer advance amount for stereo/mono mode.
     p_advance = CHUNK_SZ;                                // Two channels worth of samples per chunk
     channels  = Mode_stereo;
  }
  else {                                                  // Or one channels worth of samples per chunk... One lump or two vicar?
    p_advance  = HALFCHUNK_SZ;
    channels   = Mode_mono;     
  }

  I2S_PlaybackSpeed = playback_speed;                     // Set our playback speed.
  
  // Recalculate fade sample counts based on new playback speed
  RecalculateFadeSamples();
  
  if( AudioEngine_I2SInit ) { AudioEngine_I2SInit( playback_speed ); }  // Initialize I2S peripheral with our chosen sample rate.

  dma_interrupt_disable( DMA0, DMA_CH4, DMA_INT_HTF | DMA_INT_FTF );    // Stop playback and IRQ generation.
  i2s_disable( PROJECT_SPI );

// Always start from a known-clean state before filling the next playback buffer.
  PrepareForNewPlayback();
  
  // Warm up 16-bit biquad filter state from first sample to avoid startup transient
  if( sample_depth == 16 && filter_cfg.enable_16bit_biquad_lpf ) {
    int16_t first_sample = *( (int16_t *)sample_to_play );
    WarmupBiquadFilter16Bit( first_sample );
  }
  
  if( sample_depth == 16 ) {                  // For 16-bit, initialize 16-bit sample playback pointers
    pb_p16_ptr    = (uint16_t *) sample_to_play;
    pb_end16_ptr  = pb_p16_ptr + sample_set_sz;
    pb_mode   = 16;
  }
  else if( sample_depth == 8 ) {              // For 8-bit, initialize 8-bit sample playback pointers
    pb_p8_ptr     = (uint8_t *) sample_to_play;
    pb_end8_ptr   = pb_p8_ptr + sample_set_sz;
    pb_mode   = 8;
  }
  // Initialize fade counters
  playback_total_samples      = sample_set_sz;
  playback_samples_played     = 0U;
  dma_half_sample_counts[ FIRST ] = 0U;
  dma_half_sample_counts[ SECOND ] = 0U;
  samples_remaining         = sample_set_sz;  // Track position in file
  fadeout_samples_remaining = 0;              // Pause fadeout duration (set when pause is called)
  fadein_samples_remaining  = fadein_samples;
  
  // Pre-fill the buffer with processed samples before starting DMA
  // This ensures the fade-in is applied from the very first sample that plays
  half_to_fill = FIRST;
  if( pb_mode == 16 ) {
    if( ProcessNextWaveChunk( (int16_t *) pb_p16_ptr ) != PB_Playing ) { return PB_Error; }
    pb_p16_ptr += p_advance;

    half_to_fill = SECOND;
    if( ProcessNextWaveChunk( (int16_t *) pb_p16_ptr ) != PB_Playing ) { return PB_Error; }
    pb_p16_ptr += p_advance;
    half_to_fill = FIRST;
  }
  else if( pb_mode == 8 ) {
    if( ProcessNextWaveChunk_8_bit( (uint8_t *) pb_p8_ptr ) != PB_Playing ) { return PB_Error; }
    half_to_fill = SECOND;
    if( ProcessNextWaveChunk_8_bit( (uint8_t *) pb_p8_ptr ) != PB_Playing ) { return PB_Error; }
    pb_p8_ptr += p_advance;
    half_to_fill = FIRST;
  }
  
  // Start playback of the recording
  //
  if( dac_power_control == true ) {
    AudioEngine_DACSwitch( DAC_ON );  // Ensure DAC is powered on before starting playback
  }

  pb_state = PB_Playing;
  dma_config();
  spi_config( playback_speed );
  return PB_Playing;
}


/** Simple function that waits for the end of playback.
  *
  * @param: none
  * @retval: PB_StatusTypeDef indicating success or failure.
  *
  */
PB_StatusTypeDef WaitForSampleEnd( void )
{
  while( pb_state == PB_Playing || pb_state == PB_Pausing || pb_state == PB_Paused ) {
    __NOP();  // Prevent optimizer from removing loop
  }
  
  // Cleanup: Stop DMA transmission now that we're out of the callback context
  // This prevents the I2S_WaitFlagStateUntilTimeout hang that occurs when
  // stopping from within the DMA callback
  if( pb_state != PB_Playing ) {
  dma_interrupt_disable( DMA0, DMA_CH4, DMA_INT_HTF | DMA_INT_FTF );
  i2s_disable( PROJECT_SPI );
  }
  
  return pb_state;
}


/**
  * @brief Pause playback with a smooth fade-out
  * 
  * Saves the current playback position and initiates a fade-out over PAUSE_FADEOUT_SAMPLES.
  * float alpha_f = 1.0f - expf( -2.0f * 3.14159265359f * cutoff_hz / sample_rate_hz );
  * Call ResumePlayback() to continue from where it paused.
  * 
  * @retval PB_StatusTypeDef - Current playback state (PB_Playing while fading, PB_Paused when done)
  */
PB_StatusTypeDef PausePlayback( void )
{
  if( pb_state != PB_Playing ) {
    return pb_state;  // Can only pause while actively playing
  }
  
  /* Save current playback position before initiating pause */
  if( pb_mode == 16 ) {
    paused_sample_ptr = (const void *)pb_p16_ptr;
  } else {
    paused_sample_ptr = (const void *)pb_p8_ptr;
  }

  /* Preserve remaining samples so resume doesn't skip the end-of-file logic */
  paused_samples_remaining = samples_remaining;
  
  /* Calculate current volume level if fading in */
  uint32_t fadeout_start_level = pause_fadeout_samples;
  
  if( fadein_samples_remaining > 0 ) {
    /* We're in the middle of fade-in. Calculate how far we've progressed. */
    uint32_t fadein_progress = fadein_samples - fadein_samples_remaining;
    /* Start the pause fadeout from the current volume level (linear progress maps to quadratic volume curve) */
    fadeout_start_level = ( fadein_progress * pause_fadeout_samples ) / fadein_samples;
  }
  else if( pb_mode == 16 || pb_mode == 8 ) {
    /* Check if we're in the middle of end-of-file fadeout */
    uint32_t remaining_in_file = 0;
    if( pb_mode == 16 ) {
      ptrdiff_t remaining = pb_end16_ptr - pb_p16_ptr;
      if( remaining > 0 ) {
        remaining_in_file = (uint32_t)remaining;
      }
    } else {
      ptrdiff_t remaining = pb_end8_ptr - pb_p8_ptr;
      if( remaining > 0 ) {
        remaining_in_file = (uint32_t)remaining;
      }
    }
    
    if( remaining_in_file > 0 && remaining_in_file <= fadeout_samples ) {
      /* We're in the end-of-file fadeout window. Scale pause fadeout to start from current level.
         End-of-file fadeout uses: fade_mult = remaining_in_file^2 / fadeout_samples^2
         For continuity: remaining_in_file / fadeout_samples = fadeout_start_level / pause_fadeout_samples */
      fadeout_start_level = ( remaining_in_file * pause_fadeout_samples ) / fadeout_samples;
    }
  }
  
  /* Cancel fade-in and initiate pause fadeout from current volume */
  fadein_samples_remaining  = 0;
  fadeout_samples_remaining = fadeout_start_level;
  pb_state                  = PB_Pausing;
  
  return PB_Pausing;
}


/**
  * @brief Resume playback from pause with a smooth fade-in
  * 
  * Resumes playback from where it was paused and fades in over FADEIN_SAMPLES.
  * The audio will smoothly ramp up from silence to full volume.
  * 
  * @retval PB_StatusTypeDef - PLAYING if successfully resumed, error state otherwise
  */
PB_StatusTypeDef ResumePlayback( void )
{
  if( pb_state != PB_Paused && pb_state != PB_Pausing ) {
    return pb_state;  // Can only resume from paused or pausing state
  }
  
  /* Restore playback position from where it was paused */
  if( paused_sample_ptr != NULL ) {
    if( pb_mode == 16 ) {
      pb_p16_ptr  = (uint16_t *)paused_sample_ptr;
    } else {
      pb_p8_ptr   = (uint8_t *)paused_sample_ptr;
    }
  }

  /* Restore remaining samples to align fade-out timing after resume */
  if( paused_samples_remaining > 0 ) {
    samples_remaining = paused_samples_remaining;
  }

  /* Resume playback from where it was paused */
  pb_state = PB_Playing;
  
  /* Reset fade-out counter and initiate smooth fade-in */
  fadeout_samples_remaining = 0;
  fadein_samples_remaining  = pause_fadein_samples;
  
  return PB_Playing;
}


/**
  * @brief Request asynchronous stop with normal end-of-play fade-out
  *
  * Sets a stop request flag that causes playback to fade out using the standard
  * end-of-play fade and then stop. Returns immediately without blocking.
  * The DMA callback handles the actual stop when fade completes.
  *
  * Use GetPlaybackState() to poll for completion: returns PB_Idle when done.
  *
  * @retval PB_StatusTypeDef - Current playback state, or PB_Idle if already idle
  */
PB_StatusTypeDef StopPlayback( void )
{
  if( pb_state == PB_Idle ) {
    return PB_Idle;
  }

  /* Request asynchronous stop (DMA callback will handle it on next interrupt) */
  stop_requested = 1;
  
  return pb_state;
}


/** Apply non-linear volume response curve for perceptually-uniform control
  * 
  * @param: linear_volume - Linear volume value (0-65535)
  * @retval: uint16_t - Non-linearly scaled volume (0-65535)
  */
static inline uint16_t ApplyVolumeResponseCurve( uint16_t linear_volume )
{
  if( volume_response_nonlinear ) {

    /* Normalize to 0.0-1.0 range */
    float normalized = (float)linear_volume / 65535.0f;

    /* Apply inverse power law (gamma > 1 creates logarithmic response) */
    float curved = powf( normalized, 1.0f / volume_response_gamma );
    
    /* Scale back to 0-65535 range */
    return (uint16_t)( curved * 65535.0f + 0.5f );
  } else {
    /* Linear response - no scaling */
    return linear_volume;
  }
}


/** Apply volume setting to sample
  * 
  * @param: sample - Signed 16-bit audio sample
  * @param: volume_setting - Volume division factor (1 to 65535) where 65535 is full volume and 1 is minimum audible volume
  * @retval: int16_t - Volume-adjusted signed 16-bit audio sample
  */
static inline int16_t ApplyVolumeSetting( int16_t sample, uint16_t volume_setting )
{
  static uint16_t   adjusted_volume,
                    last_volume_setting   = 0;

  /* Apply non-linear volume response curve if enabled, only recalculating when volume changes to lower CPU overhead */
  if( volume_setting != last_volume_setting ) {
    adjusted_volume = ApplyVolumeResponseCurve( volume_setting );
    last_volume_setting = volume_setting;
  }

  int32_t sample32 = (int32_t) sample;
  int32_t volume32 = (int32_t) adjusted_volume;

  /* Apply 16-bit volume (0-65535) with proper scaling to 0.0-1.0 range
     Preserve signed sample polarity by keeping all math in signed space. */
  return  (int16_t)( ( sample32 * volume32 ) / 65535 );
}


/** Hard shutdown
  *
  * Immediately halts playback and resets all state. Use in critical failure scenarios
  * where you need to stop sound output immediately without waiting for fade-out.
  * Use with caution as it will cut off sound abruptly.
  * @param: none
  * @retval: none
   */
void ShutDownAudio( void )
{
  // Hard stop: immediately halt DMA and reset playback state
  StopDmaAndResetPlaybackState( 1U );
  MIDPOINT_FILL_BUFFER();
  pb_state = PB_Idle;

  if( dac_power_control == true ) {
    AudioEngine_DACSwitch( DAC_OFF );
  }
}


/* Return the selected alpha value
*
* @param: LPF_Level lpf_level - Desired low-pass filter level
* @retval: uint16_t - Corresponding alpha coefficient for 8-bit LPF
*/
static inline uint16_t GetLpf8BitAlpha( LPF_Level lpf_level )
{
    switch (lpf_level) {
    case LPF_Off:
      return LPF_MEDIUM;

        case LPF_Custom:
      return filter_cfg.lpf_8bit_custom_alpha;
            
        case LPF_VerySoft:
            return LPF_VERY_SOFT;

        case LPF_Soft:
            return LPF_SOFT;

        case LPF_Firm:
            return LPF_FIRM;
            
        case LPF_Aggressive:
            return LPF_AGGRESSIVE;

        case LPF_Medium:
        default:
            return LPF_MEDIUM;
    }
}


/** Enable or disable non-linear volume response
  * 
  * @brief Human hearing perceives loudness logarithmically. Enabling non-linear
  *        response applies a power curve to create more intuitive volume control.
  * 
  * @param: enable - 1 to enable non-linear response, 0 for linear
  * @retval: none
  */
void SetVolumeResponseNonlinear( uint8_t enable )
{
  volume_response_nonlinear = enable ? 1 : 0;
}


/** Get current volume response mode
  * 
  * @retval: uint8_t - 1 if non-linear, 0 if linear
  */
uint8_t GetVolumeResponseNonlinear( void )
{
  return volume_response_nonlinear;
}


/** Set volume response gamma exponent
  * 
  * @brief Controls the aggressiveness of the non-linear volume curve.
  *        Typical values: 1.0 = linear, 2.0 = quadratic (recommended for human perception)
  * 
  * @param: gamma - Gamma exponent value (clamped to 1.0-4.0 range)
  * @retval: none
  */
void SetVolumeResponseGamma( float gamma )
{
  if( gamma < 1.0f ) gamma = 1.0f;
  if( gamma > 4.0f ) gamma = 4.0f;
  volume_response_gamma = gamma;
}


/** Get current volume response gamma exponent
  * 
  * @retval: float - Current gamma value
  */
float GetVolumeResponseGamma( void )
{
  return volume_response_gamma;
}


/*!
    \brief      configure the DMA peripheral
    \param[in]  none
    \param[out] none
    \retval     none
*/
void dma_config()
{
    dma_parameter_struct dma_init_struct;
    
    /* SPI transmit dma config: DMA0,DMA_CH4  */
    dma_deinit(DMA0, DMA_CH4);
    dma_init_struct.periph_addr  = (uint32_t) &SPI_DATA( PROJECT_SPI );
    dma_init_struct.memory_addr  = (uint32_t) pb_buffer;
    dma_init_struct.direction    = DMA_MEMORY_TO_PERIPHERAL;
    dma_init_struct.memory_width = DMA_MEMORY_WIDTH_16BIT;             // This needs looking at!
    dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_16BIT;
    dma_init_struct.priority     = DMA_PRIORITY_HIGH;
    dma_init_struct.number       = PB_BUFF_SZ;
    dma_init_struct.periph_inc   = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc   = DMA_MEMORY_INCREASE_ENABLE;

    dma_init(DMA0, DMA_CH4, &dma_init_struct);

    /* configure DMA mode */
    dma_circulation_enable(DMA0, DMA_CH4);
    dma_memory_to_memory_disable(DMA0, DMA_CH4);
   
    dma_interrupt_enable(DMA0, DMA_CH4, DMA_INT_FTF | DMA_INT_HTF ); 
    dma_channel_enable(DMA0, DMA_CH4);
}
