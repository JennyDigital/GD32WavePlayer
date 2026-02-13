//
// main.cpp : Defines entry point for an GD32F30x C/C++ application.
//
#include <gd32f30x.h>
#include "main.h"
#include "interrupt_utils.h"
#include "audio_engine.h"

/* Sounds for playback */

#include "newchallenger.h"
#include "newchallenger11k.h"
#include "guitar.h"
#include "mind_the_door.h"
#include "three_tone_arrival_c.h"
#include "tunnelbarra.h"
#include "tunnelbarra16.h"
#include "konnichiwa.h"
#include "dinding.h"
#include "elevator_ping.h"
#include "Danger.h"
#include "selfdestruct.h"
#include "new_rage32k.h"
#include "accoustic_rock22k.h"
#include "hey_yeah32k.h"
#include "darkblues32k.h"
#include "custom_tritone16k.h"
#include "ocarina_melody32k.h"
#include "theremin_quartet.h"
#include "steves_doorbell.h"
#include "harmony8b.h"
#include "andean_flute.h"
#include "quencho_flute.h"
#include "dreamy.h"
#include "guitar_small.h"
#include "guitar_riff.h"
#include "handpan.h"
#include "nylon_guitar.h"
#include "dalby_tritone16b16k.h"


static        void        SetupClocks               ( void );
              void        spi_config                ( uint32_t speed );
static        void        GPIO_InitPins             ( void );

              void        DAC_MasterSwitch          ( uint8_t setting );
              uint16_t    ReadVolume                ( void );
              void        WaitForTrigger            ( uint8_t trig_to_wait_for );
              uint8_t     GetTriggerOption          ( void );

              void        delay_ms                  ( uint32_t millis );
              void        ErrorHandler              ( void );


/* State variables */
volatile  uint16_t    trig_counter            = 0;
volatile  uint8_t     trig_status             = TRIGGER_CLR;

/* SysTick variables */
volatile uint32_t     systick_counter         = 0,
                      uwTick                  = 0;

// External variables from audio_engine
extern FilterConfig_TypeDef filter_cfg;

void main( void )
{
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  SystemInit();

  SetupClocks();

  nvic_irq_enable( DMA0_Channel4_IRQn, 0,0 );

  /* Initialize all configured peripherals */
  GPIO_InitPins();              

  /* Initialize audio engine with hardware interface functions */
  if( AudioEngine_Init( DAC_MasterSwitch, ReadVolume, spi_config ) != PB_Idle ) {
    ErrorHandler();
  }

  // Configure volume response curve (human perception matched)
  SetVolumeResponseNonlinear( 1 );    // Enable non-linear (logarithmic) response
  SetVolumeResponseGamma( 2.0f );     // Gamma = 2.0 (quadratic, typical for human perception)

  // Small delay to allow hardware to stabilize
  delay_ms( 150 );

  // Set DAC control to manual and start with it on.
  DAC_MasterSwitch( DAC_ON );         // Start with DAC off until ready to play
  SetDAC_Control( 0 );                // 0 = manual control, 1 = auto control by audio engine

  // FilterConfig_TypeDef filter_cfg;
  filter_cfg.enable_noise_gate            = 0;  // Noise gate disabled by default; enable as needed
  filter_cfg.enable_16bit_biquad_lpf      = 0;  // 16-bit biquad LPF disabled by default; enable as needed
  filter_cfg.enable_8bit_lpf              = 0;  // 8-bit LPF disabled by default; enable as needed
  filter_cfg.enable_soft_dc_filter_16bit  = 1;  // Soft DC blocking filter for 16-bit samples enabled by default
  filter_cfg.enable_soft_clipping         = 1;  // Soft clipping enabled by default
  filter_cfg.enable_air_effect            = 0;  // Air effect (high-shelf brightening) disabled by default; enable as needed
  filter_cfg.enable_filter_chain_16bit    = 1;  // Master enable for entire 16-bit filter chain
  filter_cfg.enable_filter_chain_8bit     = 0;  // Master enable for entire 8-bit filter chain

  // Apply initial filter configuration
  SetFilterConfig( &filter_cfg );

  // Set initial Air Effect boost in dB (runtime adjustable)
  SetAirEffectPresetDb( 0 );              // default +3 dB preset
  
  // Set fade times
  SetFadeInTime(1.8f );                   // 800 ms fade-in
  SetFadeOutTime( 2.15f );                // 150 ms fade-out
  SetPauseFadeTime( 0.15f );              // 150 ms pause fade-out
  SetResumeFadeTime( 1.25f );             // 1250 ms resume fade-in

  /* Superloop */
  while( true )
  {
    PlaySample( ocarina32k, OCARINA32K_SZ, I2S_AUDIOSAMPLE_32K, 16, Mode_mono );
    WaitForSampleEnd();
    delay_ms( 1000 );
  }
}


/** Changes NSD_MODE_Pin pin to control the DAC between on and Shutdown
  *
  * param: DAC_OFF (0) or DAC_ON (non zero)
  * retval: none
  */
void DAC_MasterSwitch( uint8_t setting )
{
  gpio_bit_write( nSD_MODE_BANK, nSD_MODE_Pin, setting );
  delay_ms( 10 );
}


/** Read the master volume level for playback.
  *
  * params: none
  * retval: uint16_t between 1 and 65535 for volume scaling.
  *
  * Note: Non-linear volume response is now handled internally by the audio engine.
  *       Use SetVolumeResponseNonlinear() and SetVolumeResponseGamma() to configure.
  */
uint16_t ReadVolume( void )
{
  uint16_t volume = 0;

    // Use digital GPIOs for volume (3 bits, scaled to 1-65535)
    uint8_t v = (
                  ( gpio_input_bit_get( OPT3_Bank, OPT3_Pin ) << 2 )  |
                  ( gpio_input_bit_get( OPT2_Bank, OPT2_Pin ) << 1 )  |
                  ( gpio_input_bit_get( OPT1_Bank, OPT1_Pin )      )
                );

    v = 7 - v;        // Invert so 0b000 = max volume, 0b111 = min volume
    uint32_t scaled = ( (uint32_t)v * 65535U ) / 7U;  // Map 0-7 to 0-65535
    volume = (uint16_t)scaled;
 
  /* Analog signals have noise; clamp low values to avoid noise-induced ultra-quiet audio */
  if( volume < 32U ) volume = 32U;

  /* Return raw volume - audio engine applies non-linear response curve internally */
  return volume;
} 


/** Wait for the trigger signal
 *
 * Waits until the trigger signal is received.
 *
 * params: none
 * reval: none
 *
 */
inline void WaitForTrigger( uint8_t trig_to_wait_for )
{
  while ( trig_status != trig_to_wait_for );
}


/** Returns the state of the trigger option pad.
  *
  * params: none
  * retval: trigger option pad state, 1 Being enabled.
  *
  */
uint8_t GetTriggerOption( void )
{
#ifdef TEST_CYCLING
  return 1;
#endif
#ifdef FORCE_TRIGGER_OPT
  return 1;
#endif
  return gpio_input_bit_get( OPT4_Bank, OPT4_Pin );
}


void SysTick_Handler( void ) {
  uwTick++;
  if( systick_counter )
  {
    systick_counter--;
  }

  if( gpio_input_bit_get( TRIGGER_Bank, TRIGGER_Pin ) != 0 )
  {
    if( trig_counter < TC_MAX ) trig_counter++;
  }
  else
  {
    if (trig_counter > 0 ) trig_counter--;
  }

  if( trig_counter < TC_LOW_THRESHOLD )   trig_status = TRIGGER_CLR;
  if( trig_counter > TC_HIGH_THRESHOLD )  trig_status = TRIGGER_SET;
}


void delay_ms( uint32_t millis )
{
  systick_counter = millis;
  while( systick_counter );
}


/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler( void )
{
  __disable_irq();
  DAC_MasterSwitch( DAC_OFF );
  while( 1 )
  {
  }
}


static void GPIO_InitPins( void )
{

  // Clocking for I2S Peripheral pins
  //
  gpio_init( I2S_WS_BANK, GPIO_MODE_AF_PP, GPIO_OSPEED_10MHZ, I2S_WS_Pin );
  gpio_init( I2S_CK_BANK, GPIO_MODE_AF_PP, GPIO_OSPEED_10MHZ, I2S_CK_Pin );
  gpio_init( I2S_SD_BANK, GPIO_MODE_AF_PP, GPIO_OSPEED_10MHZ, I2S_SD_Pin );

  
  // GPIO for turning the MAX98357A chip on and off.
  //
  gpio_init( nSD_MODE_BANK, GPIO_MODE_OUT_OD, GPIO_OSPEED_2MHZ, nSD_MODE_Pin );
  gpio_pin_lock( nSD_MODE_BANK, nSD_MODE_Pin );

  // Option pins
  //
  gpio_init( OPT1_Bank, GPIO_MODE_IPD, GPIO_OSPEED_2MHZ, OPT1_Pin );
  gpio_init( OPT2_Bank, GPIO_MODE_IPD, GPIO_OSPEED_2MHZ, OPT2_Pin );
  gpio_init( OPT3_Bank, GPIO_MODE_IPD, GPIO_OSPEED_2MHZ, OPT3_Pin );
  gpio_init( OPT4_Bank, GPIO_MODE_IPD, GPIO_OSPEED_2MHZ, OPT4_Pin );
  gpio_pin_lock( OPT1_Bank, OPT1_Pin );
  gpio_pin_lock( OPT2_Bank, OPT2_Pin );
  gpio_pin_lock( OPT3_Bank, OPT3_Pin );
  gpio_pin_lock( OPT4_Bank, OPT4_Pin );

  // Trigger pin.
  //
  gpio_init( TRIGGER_Bank, GPIO_MODE_IPD, GPIO_OSPEED_2MHZ, TRIGGER_Pin );
}


void spi_config( uint32_t speed )
{
  spi_parameter_struct spi_str;

    spi_i2s_deinit( PROJECT_SPI );
    i2s_init( PROJECT_SPI, I2S_MODE_MASTERTX, I2S_STD_PHILLIPS, I2S_CKPL_LOW );
    spi_i2s_data_frame_format_config( PROJECT_SPI, SPI_FRAMESIZE_16BIT );
    i2s_psc_config(PROJECT_SPI, speed, I2S_FRAMEFORMAT_DT16B_CH16B, I2S_MCKOUT_DISABLE);
    i2s_enable( PROJECT_SPI );
    spi_enable( PROJECT_SPI );
    spi_dma_enable(PROJECT_SPI, SPI_DMA_TRANSMIT );
}

static void SetupClocks( void )
{
  rcu_system_clock_source_config( RCU_SCSS_IRC8M );
  rcu_osci_off( RCU_PLL_CK );
  rcu_osci_on ( RCU_HXTAL );
  
  rcu_pll_config(RCU_PLLSRC_IRC8M_DIV2, RCU_PLL_MUL27 );
  rcu_osci_on( RCU_PLL_CK );
  if( SUCCESS != rcu_osci_stab_wait( RCU_PLL_CK ) )
  { 
    ErrorHandler();
  }
  
  rcu_system_clock_source_config( RCU_CKSYSSRC_PLL );
  SystemCoreClockUpdate();
  SysTick_Config( SystemCoreClock / 1000 );


  // Bank A usage:
  //              TRIGGER_Pin
  //
  rcu_periph_clock_enable( RCU_GPIOA );

  // Bank B usage:
  //              I2S (All pins)
  //              OPTx pins
  rcu_periph_clock_enable( RCU_GPIOB );

  // Clocking for I2S/SPU
  //
  rcu_periph_clock_enable( RCU_AF );
  rcu_periph_clock_enable( PROJECT_SPI_CLOCK );
  rcu_periph_clock_enable( RCU_DMA0 );
}


void HardFault_Handler( void )
{
  __disable_irq();
  while(1);
}

void ErrorHandler( void )
{
  while(1);
}