//
// main.cpp : Defines entry point for an GD32F30x C/C++ application.
//
#include <gd32f30x.h>
#include <stdbool.h>
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
#include "Lemon_Tree.h"

              void        SetupADC                  ( void );
              void        SetupTimer2               ( void );
static        void        SetupClocks               ( void );
              void        spi_config                ( uint32_t speed );
static        void        GPIO_InitPins             ( void );

              void        DAC_MasterSwitch          ( uint8_t setting );
              uint16_t    ReadVolume                ( void );
              void        WaitForTrigger            ( uint8_t trig_to_wait_for );
              uint8_t     GetTriggerOption          ( void );

              void        delay_ms                  ( uint32_t millis );
              void        Error_Handler             ( void );
              void        Enter_LP_SleepMode        ( void );

// Trigger control variables (hardware-specific)
volatile  uint16_t        trig_counter                  = 0;              // Counter for trigger input timing
volatile  uint8_t         trig_timeout_flag             = 0;              // Flag indicating trigger timeout has occurred
volatile  uint16_t        trig_timeout_counter          = 0;              // Counter for trigger timeout duration
volatile  uint8_t         trig_status                   = TRIGGER_CLR;    // Current trigger status  (SET or CLR)


/* SysTick variables */
volatile  uint32_t        systick_counter               = 0,
                          uwTick                        = 0;

/* The ADC Value */
volatile  uint16_t        adc_raw                       = 0;

// External variables from audio_engine
extern FilterConfig_TypeDef filter_cfg;

int main( void )
{
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  SystemInit();

  SetupClocks();

  nvic_irq_enable( DMA0_Channel4_IRQn, 0,0 );
  nvic_irq_enable( EXTI5_9_IRQn, 1,0 );

  exti_init( EXTI_8, EXTI_INTERRUPT, EXTI_TRIG_BOTH );
  exti_interrupt_enable( EXTI_8 );

  /* Initialize all configured peripherals */
  GPIO_InitPins();              

  /* Let's get our ADC Working, triggered by timer 2 (phew, that was just trouble! */
  SetupADC();
  SetupTimer2();

  /* Initialize audio engine with hardware interface functions */
  if( AudioEngine_Init( DAC_MasterSwitch, ReadVolume, spi_config ) != PB_Idle ) {
    Error_Handler();
  }

  // Configure volume response curve (human perception matched)
  SetVolumeResponseNonlinear( 1 );    // Enable non-linear (logarithmic) response
  SetVolumeResponseGamma( 2.0f );     // Gamma = 2.0 (quadratic, typical for human perception)

  // Small delay to allow hardware to stabilize
  delay_ms( 150 );

  // Set DAC control to auto..
  SetDAC_Control( 1 );                // 0 = manual control, 1 = auto control by audio engine

  // FilterConfig_TypeDef filter_cfg;
  filter_cfg.enable_noise_gate            = 0;  // Noise gate disabled by default; enable as needed
  filter_cfg.enable_16bit_biquad_lpf      = 0;  // 16-bit biquad LPF disabled by default; enable as needed
  filter_cfg.enable_8bit_lpf              = 1;  // 8-bit LPF disabled by default; enable as needed
  filter_cfg.enable_soft_dc_filter_16bit  = 1;  // Soft DC blocking filter for 16-bit samples enabled by default
  filter_cfg.enable_soft_clipping         = 1;  // Soft clipping enabled by default
  filter_cfg.enable_air_effect            = 0;  // Air effect (high-shelf brightening) disabled by default; enable as needed
  filter_cfg.enable_filter_chain_16bit    = 1;  // Master enable for entire 16-bit filter chain
  filter_cfg.enable_filter_chain_8bit     = 0;  // Master enable for entire 8-bit filter chain

  // Apply initial filter configuration
  SetFilterConfig( &filter_cfg );

  // Set initial Air Effect boost in dB (runtime adjustable)
  SetAirEffectPresetDb( 0 );              // default +3 dB preset
  SetLpf16BitLevel( LPF_Custom );
  SetLpf16BitCustomAlpha( CalcLpf16BitAlphaFromCutoff( 2500, I2S_AUDIOSAMPLE_16K ) );
  
  // Set fade times
  SetFadeInTime(0.8f );                   // 800 ms fade-in
  SetFadeOutTime( 0.8f );                 // 150 ms fade-out
  SetPauseFadeTime( 0.15f );              // 150 ms pause fade-out
  SetResumeFadeTime( 1.25f );             // 1250 ms resume fade-in

  /* Superloop */
  while( true )
  {
    WaitForTrigger( TRIGGER_SET );

    PlaySample( Lemon_Tree16b16km, LEMON_TREE16B16KM_SZ, I2S_AUDIOSAMPLE_16K, 16, LEMON_TREE16B16KM_PB_FMT );

    WaitForSampleEnd();
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


///** Read the master volume level for playback.
//  *
//  * params: none
//  * retval: uint16_t between 1 and 65535 for volume scaling.
//  *
//  * Note: Non-linear volume response is now handled internally by the audio engine.
//  *       Use SetVolumeResponseNonlinear() and SetVolumeResponseGamma() to configure.
//  */
//uint16_t ReadVolume( void )
//{
//  uint16_t volume = 0;

//    // Use digital GPIOs for volume (3 bits, scaled to 1-65535)
//    uint8_t v = (
//                  ( gpio_input_bit_get( OPT3_Bank, OPT3_Pin ) << 2 )  |
//                  ( gpio_input_bit_get( OPT2_Bank, OPT2_Pin ) << 1 )  |
//                  ( gpio_input_bit_get( OPT1_Bank, OPT1_Pin )      )
//                );

//    v = 7 - v;        // Invert so 0b000 = max volume, 0b111 = min volume
//    uint32_t scaled = ( (uint32_t)v * 65535U ) / 7U;  // Map 0-7 to 0-65535
//    volume = (uint16_t)scaled;
 
//  /* Analog signals have noise; clamp low values to avoid noise-induced ultra-quiet audio */
//  if( volume < 32U ) volume = 32U;

//  /* Return raw volume - audio engine applies non-linear response curve internally */
//  return volume;
//} 


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

  #ifdef VOLUME_INPUT_DIGITAL
    // Use digital GPIOs for volume (3 bits, scaled to 1-65535)
    uint8_t v =
      ( ( (OPT3_GPIO_Port->IDR & OPT3_Pin) != 0 ) << 2 ) |
      ( ( (OPT2_GPIO_Port->IDR & OPT2_Pin) != 0 ) << 1 ) |
      ( ( (OPT1_GPIO_Port->IDR & OPT1_Pin) != 0 ) << 0 );

    v = 7 - v;        // Invert so 0b000 = max volume, 0b111 = min volume
    uint32_t scaled = ( (uint32_t)v * 65535U ) / 7U;  // Map 0-7 to 0-65535
    volume = (uint16_t)scaled;
  #else
    // Use 12-bit ADC value (0-4095) for linear volume
    // Scale 12-bit ADC directly to match 16-bit volume range with 16x scaling factor
    // (4095 * 16 = 65520, close to full 65535 range)
    #ifndef VOLUME_ADC_INVERTED
    uint32_t lin = (uint32_t)adc_raw * MASTER_VOLUME_SCALE;               // Scale 12-bit to acceptable range
    #else
    uint32_t lin = ( (4095U - (uint32_t)adc_raw) * MASTER_VOLUME_SCALE ); // Invert ADC reading so 0 = max volume, 4095 = min volume
    #endif
    if( lin > VOLUME_ADC_MAX_SCALED ) lin = VOLUME_ADC_MAX_SCALED;        // Cap at maximum ADC * 16
    volume = (uint16_t)lin;
  #endif

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
  while( true ) {
    trig_timeout_flag = 0;
    while( trig_status != trig_to_wait_for ) {
      delay_ms( 1 );
      trig_timeout_counter++;
      if( trig_timeout_counter >= TRIG_TIMEOUT_MS ) {
        trig_timeout_flag = 1;
        trig_timeout_counter = 0;
        break;
      }
    }
    if( trig_status == trig_to_wait_for ) return;
#ifndef NO_SLEEP_MODE
    Enter_LP_SleepMode();
#endif
  }
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



/** Process the System Tick
  *
  */
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


/** Blocking delay function.
  *
  * @param: millis. The number of milliseconds to wait
  * @retval: none
  *
  */
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


/** Initialize the GPIO Pins for the application
  *
  * @param: none
  * @retval: none
  *
  */
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


void SetupADC( void )
{
  //adc_deinit( ADC1 );
  //adc_resolution_config( ADC1, ADC_RESOLUTION_12B );
  //adc_discontinuous_mode_config( ADC1, ADC_REGULAR_CHANNEL, 1 );
  //adc_data_alignment_config( ADC1, ADC_DATAALIGN_RIGHT );
  //adc_mode_config( ADC_MODE_FREE );

  //adc_special_function_config(ADC1, ADC_SCAN_MODE, DISABLE);        // Disable Scan
  //adc_special_function_config(ADC1, ADC_CONTINUOUS_MODE, DISABLE);  // !!! Disable Continuous Mode

  //adc_external_trigger_source_config( ADC1, ADC_REGULAR_CHANNEL, ADC0_1_EXTTRIG_INSERTED_T0_TRGO );
  //adc_external_trigger_config( ADC1, ADC_REGULAR_CHANNEL, ENABLE );

  //adc_interrupt_enable( ADC1, ADC_INT_EOC );
  //adc_enable( ADC1 );

  //nvic_irq_enable( ADC1, 3, 0 );

  adc_deinit(ADC1);
  // Configure ADC1: Single channel, no scan
  adc_special_function_config(ADC1, ADC_SCAN_MODE, DISABLE);
  adc_special_function_config(ADC1, ADC_CONTINUOUS_MODE, DISABLE); // Triggered, not continuous
  adc_data_alignment_config(ADC1, ADC_DATAALIGN_RIGHT);

  // Set Trigger Source to Timer1 TRGO
  adc_external_trigger_source_config( ADC1, ADC_REGULAR_CHANNEL, ADC0_1_EXTTRIG_REGULAR_T2_TRGO );
  adc_external_trigger_config( ADC1, ADC_REGULAR_CHANNEL, ENABLE );

  // Enable ADC and calibrate
  adc_enable(ADC1);
  adc_calibration_enable(ADC1);

  // Enable Interrupt for End of Conversion
  adc_interrupt_enable(ADC1, ADC_INT_EOC);
  nvic_irq_enable(ADC0_1_IRQn, 0, 0); // Need to handle ADC0,1,2 ISR

  // Start ADC
  adc_software_trigger_enable(ADC1, ADC_REGULAR_CHANNEL); // Initial trigger
}

/** Set up timer 2 with update event
  *
  * @param: none.
  * @retval: none
  *
  */
void SetupTimer2( void )
{
  timer_deinit( TIMER2 );

  timer_parameter_struct timer_initpara;


  /* 2. Configure TIMER0 for Hz update */
  timer_deinit( TIMER2 );
  timer_initpara.prescaler         = 6000-1;
  timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
  timer_initpara.counterdirection  = TIMER_COUNTER_UP;
  timer_initpara.period            = 100-1;
  timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
  timer_initpara.repetitioncounter = 0;
  timer_init( TIMER2, &timer_initpara );

  /* 3. Set Master Mode to send Update Event to TRGO */
  timer_master_slave_mode_config( TIMER2, TIMER_MASTER_SLAVE_MODE_ENABLE );
  timer_master_output_trigger_source_select( TIMER2, TIMER_TRI_OUT_SRC_UPDATE );
  timer_update_event_enable( TIMER2 );


  /* 5. Enable the timer */
  timer_enable( TIMER2 );
}


/** Configures the SPI peripheral as I2S at a given sample rate
  *
  * @param: speed.  The sample rate to which we will play the sound sample.
  * @retval: none
  *
  */
void spi_config( uint32_t speed )
{
    spi_i2s_deinit( PROJECT_SPI );
    i2s_init( PROJECT_SPI, I2S_MODE_MASTERTX, I2S_STD_PHILLIPS, I2S_CKPL_LOW );
    spi_i2s_data_frame_format_config( PROJECT_SPI, SPI_FRAMESIZE_16BIT );
    i2s_psc_config(PROJECT_SPI, speed, I2S_FRAMEFORMAT_DT16B_CH16B, I2S_MCKOUT_DISABLE);
    i2s_enable( PROJECT_SPI );
    spi_enable( PROJECT_SPI );
    spi_dma_enable(PROJECT_SPI, SPI_DMA_TRANSMIT );
}


/** Set up the clocks
  *
  * @param: none
  * @retval: none
  */
static void SetupClocks( void )
{
  rcu_system_clock_source_config( RCU_SCSS_IRC8M );
  rcu_ahb_clock_config( 1 );
  rcu_osci_off( RCU_PLL_CK );
  //rcu_osci_on ( RCU_HXTAL );
  fmc_wscnt_set( WS_WSCNT_2 );

  /* Let's go flat-out at 120MHz. Zoooooom!!! */
  rcu_pll_config(RCU_PLLSRC_IRC8M_DIV2, RCU_PLL_MUL30 );
  rcu_osci_on( RCU_PLL_CK );
  if( SUCCESS != rcu_osci_stab_wait( RCU_PLL_CK ) )
  { 
    Error_Handler();
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

  // Enable Timer 2 clock.
  //
  rcu_periph_clock_enable( RCU_TIMER2 );

  // Enable ADC1 clock
  //
  rcu_periph_clock_enable( RCU_ADC1 );
  rcu_adc_clock_config( RCU_CKADC_CKAPB2_DIV16 ); // Set proper ADC clock
}


/** Puts the system into a low-power state asleep
  *
  * @note: If you add any further interrrupts, you must disable them and clear any pending IRQ flags
  * before entering sleep mode or it may never actually sleep.
  * @param: none
  * @retval|: none
  *
  */
void Enter_LP_SleepMode( void )
{
  //SysClockToSlow();

  SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk; // Disable SysTick interrupt
  nvic_irq_disable( DMA0_Channel4_IRQn );     // If for some reason DMA is running, stop it's IRQs
  nvic_irq_disable( ADC0_1_IRQn );

  /*  Flush pending interrupts */
  NVIC_ClearPendingIRQ( EXTI5_9_IRQn );
  NVIC_ClearPendingIRQ( DMA0_Channel4_IRQn );
  NVIC_ClearPendingIRQ( SysTick_IRQn );
  NVIC_ClearPendingIRQ( ADC0_1_IRQn );


  /* To sleep, perchance to dream */
  pmu_to_deepsleepmode( PMU_LDO_LOWPOWER, PMU_LOWDRIVER_ENABLE , WFI_CMD );

  /* Wake from your slumber, mighty microcontroller! */
  nvic_irq_enable( DMA0_Channel4_IRQn, 1, 0 );
  SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;  // Re-enable after wakeup
  SetupClocks();
  nvic_irq_enable(ADC0_1_IRQn, 0, 0); // Need to reenable ADC0,1,2 ISR
}


/* If you end up here, you are in a bad place, go git yer bug blaster! */
void HardFault_Handler( void )
{
  __disable_irq();
  while(1);
}


/* Just clears up the IRQ flag for the trigger, we are using the IRQ to wake the mcu on triggering. */
void EXTI5_9_IRQHandler( void )
{
  exti_interrupt_flag_clear( EXTI_8 );
}


/* ADC Conversion results get posted from within here. */
void ADC0_1_IRQHandler( void )
{
  adc_interrupt_flag_clear( ADC1, ADC_INT_FLAG_EOC );
  adc_raw = adc_regular_data_read( ADC1 );
}