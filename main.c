//
// main.c: Defines entry point for an GD32F30x C/C++ application.
//
#include <stdbool.h>
#include "main.h"
#include "interrupt_utils.h"
#include "audio_engine.h"

/* Sounds for playback */
#ifndef DALBY_BUILD
#include "newchallenger.h"
#include "newchallenger11k.h"
#include "guitar.h"
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
#include "Lemon_Tree.h"
#include "medieval_flute.h"
#include "When_I_Needed_a_neighbour.h"

#else
#include "dalby_multi.h"
#endif

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
              void        SetSleepSetting           ( uint8_t setting );
              uint8_t     GetSleepSetting           ( void );


// Trigger control variables (hardware-specific)
volatile  uint16_t        trig_counter                  = 0;              // Counter for trigger input timing
volatile  uint8_t         trig_timeout_flag             = 0;              // Flag indicating trigger timeout has occurred
volatile  uint16_t        trig_timeout_counter          = 0;              // Counter for trigger timeout duration
volatile  uint8_t         trig_status                   = TRIGGER_CLR;    // Current trigger status  (SET or CLR)
volatile  uint8_t         trig_rise_event               = 0;              // Latched on trigger rising edge IRQ
volatile  uint8_t         trig_fall_event               = 0;              // Latched on trigger falling edge IRQ

// External variables.
#ifdef DALBY_BUILD
volatile  extern OptionSelTypeDef option;
#endif

// Sleep Settings
uint8_t sleep_setting = 1;                                                // Defaults to sleep permitted.


/* SysTick variables */
volatile  uint32_t        systick_counter               = 0,
                          uwTick                        = 0;

/* The ADC Value */
volatile  uint16_t        adc_out                       = 0;

// External variables from audio_engine
extern FilterConfig_TypeDef filter_cfg;

int main( void )
{
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  SystemInit();

  SetupClocks();

  // If we're debugging, let's prevent the system from sleeping when we hit the sleep command, otherwise we won't be able to debug anything after that point without power cycling.
  if( ( CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk ) != 0U ) {
    DBG_CTL0 |= ( DBG_CTL0_SLP_HOLD | DBG_CTL0_DSLP_HOLD | DBG_CTL0_STB_HOLD );
  }

  /* Keep DMA below SysTick so SysTick-based delays can still advance. */
  nvic_irq_enable( DMA0_Channel4_IRQn, 5,0 );
  nvic_irq_enable( EXTI5_9_IRQn, 4,0 );

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
  delay_ms( 200 );

  // Set DAC control to auto..
  SetDAC_Control( 1 );                // 0 = manual control, 1 = auto control by audio engine

  // FilterConfig_TypeDef filter_cfg;
  filter_cfg.enable_noise_gate            = 0;  // Noise gate disabled by default; enable as needed
  filter_cfg.enable_16bit_biquad_lpf      = 0;  // 16-bit biquad LPF disabled by default; enable as needed
  filter_cfg.enable_8bit_lpf              = 1;  // 8-bit LPF disabled by default; enable as needed
  filter_cfg.enable_soft_dc_filter_16bit  = 1;  // Soft DC blocking filter for 16-bit samples enabled by default
  filter_cfg.enable_soft_clipping         = 0;  // Soft clipping enabled by default
  filter_cfg.enable_air_effect            = 0;  // Air effect (high-shelf brightening) disabled by default; enable as needed
  filter_cfg.enable_filter_chain_16bit    = 1;  // Master enable for entire 16-bit filter chain
  filter_cfg.enable_filter_chain_8bit     = 0;  // Master enable for entire 8-bit filter chain
  

  // Apply initial filter configuration
  SetFilterConfig( &filter_cfg );

  SetLpf16BitLevel( LPF_Off );

  // Set fade times
  SetFadeInTime(0.2f );                   // 100 ms fade-in
  SetFadeOutTime( 0.4f );                 // 100 ms fade-out
  SetPauseFadeTime( 1.5f );               // 500 ms pause fade-out
  SetResumeFadeTime( 1.5f );              // 500 ms resume fade-in

#ifdef DALBY_BUILD
  ChimeLoop();
 #else
  /* Superloop */
  while( true )
  {
    WaitForTrigger( TRIGGER_SET );

    //PlaySample( Lemon_Tree16b16km, LEMON_TREE16B16KM_SZ, I2S_AUDIOSAMPLE_16K, 16, LEMON_TREE16B16KM_PB_FMT );
    //PlaySample( medieval_flute16b22k1c, MEDIEVAL_FLUTE16B22K1C_SZ, I2S_AUDIOSAMPLE_22K, 16, Mode_mono );
    //PlaySample( ocarina32k, OCARINA32K_SZ, I2S_AUDIOSAMPLE_32K, 16, OCARINA32K_PB_FMT );
    PlaySample(neighbour16b16k1c, NEIGHBOUR16B16K1C_SZ, I2S_AUDIOSAMPLE_16K, 16, NEIGHBOUR16B16K1C_PB_FMT );

    WaitForSampleEnd();
  }
#endif
}


/** Changes NSD_MODE_Pin pin to control the DAC between on and Shutdown
  *
  * param: DAC_OFF (0) or DAC_ON (non zero)
  * retval: none
  */
void DAC_MasterSwitch( uint8_t setting )
{
  gpio_bit_write( nSD_MODE_BANK, nSD_MODE_Pin, setting );
  delay_ms( 12 );
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

  // Use 12-bit ADC value (0-4095) for linear volume
  // Scale 12-bit ADC directly to match 16-bit volume range with 16x scaling factor
  // (4095 * 16 = 65520, close to full 65535 range)
  #ifndef VOLUME_ADC_INVERTED
  uint32_t lin = (uint32_t)adc_out * MASTER_VOLUME_SCALE;                   // Scale 12-bit to acceptable range
  #else
  uint32_t lin = ( ( 4095U - (uint32_t) adc_out ) * MASTER_VOLUME_SCALE );  // Invert ADC reading so 0 = max volume, 4095 = min volume
  #endif
  if( lin > VOLUME_ADC_MAX_SCALED ) lin = VOLUME_ADC_MAX_SCALED;            // Cap at maximum ADC * 16
  volume = (uint16_t)lin;

  /* Analog signals have noise; clamp low values to avoid noise-induced ultra-quiet audio */
  if( volume < MASTER_VOLUME_MINIMUM ) volume = MASTER_VOLUME_MINIMUM;

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
    Enter_LP_SleepMode();
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
#if defined(TEST_CYCLING) || defined(FORCE_TRIGGER_OPT)
  return 1;
#else
  return gpio_input_bit_get( OPT4_Bank, OPT4_Pin );
#endif
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
  #ifdef DALBY_BUILD
  option = GetOption();
  #endif
}


/** Blocking delay function.
  *
  * @brief: A blocking delay that does not depend on an interrupt.
  *
  * @param: millis. The number of milliseconds to wait
  * @retval: none
  *
  */
void delay_ms( uint32_t millis )
{
  const uint32_t ticks_per_ms = SystemCoreClock / 1000U;
  const uint32_t reload_ticks = SysTick->LOAD + 1U;

  while( millis-- )
  {
    uint32_t start = SysTick->VAL;
    uint32_t elapsed = 0U;

    while( elapsed < ticks_per_ms )
    {
      uint32_t now = SysTick->VAL;
      if( start >= now )
      {
        elapsed += ( start - now );
      }
      else
      {
        /* Counter wrapped from 0 back to LOAD between samples. */
        elapsed += ( start + ( reload_ticks - now ) );
      }
      start = now;
    }
  }
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
  gpio_pin_remap_config( GPIO_SWJ_SWDPENABLE_REMAP, ENABLE );
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
  gpio_exti_source_select( GPIO_PORT_SOURCE_GPIOA, GPIO_PIN_SOURCE_8 );
  gpio_pin_lock( TRIGGER_Bank, TRIGGER_Pin );
}


void SetupADC( void )
{
  adc_deinit(ADC1);
  // Configure ADC1: Single channel, no scan
  adc_special_function_config(ADC1, ADC_SCAN_MODE, DISABLE);
  adc_special_function_config(ADC1, ADC_CONTINUOUS_MODE, DISABLE); // Triggered, not continuous
  adc_data_alignment_config(ADC1, ADC_DATAALIGN_RIGHT);

  // Set Trigger Source to Timer1 TRGO
  adc_external_trigger_source_config( ADC1, ADC_REGULAR_CHANNEL, ADC0_1_EXTTRIG_REGULAR_T2_TRGO );
  adc_external_trigger_config( ADC1, ADC_REGULAR_CHANNEL, ENABLE );
  adc_regular_channel_config( ADC1, 0, ADC_CHANNEL_6, ADC_SAMPLETIME_7POINT5 );
  // Enable ADC
  adc_enable(ADC1);
  // Allow settling time.
  delay_ms( 3 );
  // Start calibration
  adc_calibration_enable(ADC1);

  // Enable Interrupt for End of Conversion
  adc_interrupt_enable(ADC1, ADC_INT_EOC);
  nvic_irq_enable(ADC0_1_IRQn, 3, 0); // Need to handle ADC0,1,2 ISR

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
  timer_initpara.prescaler         = 120-1;
  timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
  timer_initpara.counterdirection  = TIMER_COUNTER_UP;
  timer_initpara.period            = 2500-1;
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
  // Only act if permitted.
  if( !sleep_setting ) return;
#ifdef NO_SLEEP_MODE
  return;
#endif

  // Prepare for sleep
  delay_ms( 100 );                            // Provide time for the system so settle down.
  SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk; // Disable SysTick interrupt
  nvic_irq_disable( DMA0_Channel4_IRQn );     // If for some reason DMA is running, stop it's IRQs
  nvic_irq_disable( ADC0_1_IRQn );            // Stop the ADC Interrupts.

  /*  Flush pending interrupts */
  NVIC_ClearPendingIRQ( EXTI5_9_IRQn );
  NVIC_ClearPendingIRQ( DMA0_Channel4_IRQn );
  NVIC_ClearPendingIRQ( SysTick_IRQn );
  NVIC_ClearPendingIRQ( ADC0_1_IRQn );


  /* To sleep, perchance to dream */
  pmu_to_deepsleepmode( PMU_LDO_NORMAL, PMU_LOWDRIVER_ENABLE, WFI_CMD );

  /* Wake from your slumber, mighty microcontroller! */
  SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;  // Re-enable after wakeup
  SetupClocks();
  nvic_irq_enable( DMA0_Channel4_IRQn, 5, 0 );
  nvic_irq_enable(ADC0_1_IRQn, 3, 0); // Need to reenable ADC0,1,2 ISR
  

}

/** Determine whether the mcu can enter sleep mode or not.
  *
  * @param: setting. 1 = permitted, 0 = no sleep permitted
  * @retval: none
  */
void SetSleepSetting( uint8_t setting )
{
  sleep_setting = setting ? 1 : 0;
}


uint8_t GetSleepSetting( void )
{
  return sleep_setting;
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
  if( gpio_input_bit_get( TRIGGER_Bank, TRIGGER_Pin ) != 0 ) {
    trig_rise_event = 1;
  }
  else {
    trig_fall_event = 1;
  }
  exti_interrupt_flag_clear( EXTI_8 );
}


/* ADC Conversion results get posted from within here. */
void ADC0_1_IRQHandler( void )
{
  #define FILTER_SHIFT 4      // Smoothing factor (higher = smoother)

  static uint16_t adc_filtered = 0;
         uint16_t adc_raw;

  // Get the raw value
  adc_raw = (uint16_t)adc_regular_data_read( ADC1 );
  // Note: We perform the subtraction first to find the 'error'
  adc_filtered = adc_filtered + ( adc_raw - ( adc_filtered >> FILTER_SHIFT ) );

  // Your actual 12-bit volume result (0-4095)
  adc_out = adc_filtered >> FILTER_SHIFT;
}