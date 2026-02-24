#include "dalby_multi.h"
#include "main.h"
#include "audio_engine.h"
#include <stdbool.h>

#include "dalby_tritone16b16k.h"
#include "mind_the_door.h"
#include "doors_opening.h"
#include "doors_closing.h"

extern void     Enter_LP_SleepMode( void );
extern void     WaitForTrigger    ( uint8_t trig_to_wait_for );
extern void     DAC_MasterSwitch  ( uint8_t setting );
extern void     SetSleepSetting   ( uint8_t setting );
extern uint8_t  GetTriggerOption  ( void );
extern void     delay_ms          ( uint32_t millis );

/** Main loop for the dalby application
  *
  * @param: none
  * @retval: none.  Does not return BTW.
  */
void ChimeLoop( void )
{
  OptionSelTypeDef option;
    
  uint8_t trigger_option = GetTriggerOption();
 
  SetDAC_Control( 1 );
  SetLpf16BitLevel( LPF_Off );
  SetSoftClippingEnable( 1 );
  
  while( true )
  {
    option =
            (
              ( gpio_input_bit_get( OPT3_Bank, OPT3_Pin ) << 2 )  |
              ( gpio_input_bit_get( OPT2_Bank, OPT2_Pin ) << 1 )  |
              ( gpio_input_bit_get( OPT1_Bank, OPT1_Pin )      )
            );

    switch( option ) {
    case OPT_Chime:   // Value: 0
    default:          // ...or default
      SetSleepSetting( 1 );
      SetFadeInTime( 0.2f );
      SetFadeOutTime( 0.2f );
      SetDAC_Control( 1 );
      
      if( trigger_option == 1 ) { WaitForTrigger( TRIGGER_SET ); }
      PlaySample( dalby_tritone16b16k, DALBY_TRITONE16B16K_SZ, I2S_AUDIOSAMPLE_16K, 16, DALBY_TRITONE16B16K_PB_FMT );
      WaitForSampleEnd();
      if( trigger_option == 1 )
      {
        WaitForTrigger( TRIGGER_CLR );
      } else {
        ShutDownAudio();
        __disable_irq();
        while( true ) {
          Enter_LP_SleepMode();
        }
      }
      break;

    case OPT_MindTheDoor:   // Value: 1
        SetSleepSetting( 1 );
        DAC_MasterSwitch( 1 );
        SetFadeInTime( 0.2f );
        SetFadeOutTime( 0.2f );
        SetDAC_Control( 1 );

        if( trigger_option == 1 ) { WaitForTrigger( TRIGGER_SET ); }
        PlaySample( mind_the_door, MIND_THE_DOOR_SZ, I2S_AUDIOSAMPLE_22K, 16, MIND_THE_DOOR_PB_FMT );
        WaitForSampleEnd();
        if( trigger_option == 1 ) {
          WaitForTrigger( TRIGGER_CLR );
        } else {
          ShutDownAudio();
          __disable_irq();
          while( true ) {
            Enter_LP_SleepMode();
          }
        }
        break;

    case OPT_DoorsOpeningClosing: // Value: 2
      // We'll keep the system live the whole time for this option.
      SetDAC_Control( 0 );
      DAC_MasterSwitch( 1 );
      SetSleepSetting( 0 );
      SetFadeInTime( 0.01f );
      SetFadeOutTime( 0.01f );

      // Endless loop of doors opening/closing.
      while( true ) {
        PlaySample( do_16b1c24k, DO_16B1C24K_SZ, 24000, 16, DO_16B1C24K_PB_FMT );
        WaitForTrigger( TRIGGER_CLR );
        StopPlayback();
        PlaySample( dc_16b1c24k, DC_16B1C24K_SZ, 24000, 16, DC_16B1C24K_PB_FMT );
        WaitForTrigger( TRIGGER_SET );
        StopPlayback();
      }      
      break;
    }
  }
}