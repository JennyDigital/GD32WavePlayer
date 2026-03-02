#include "dalby_multi.h"
#include "main.h"
#include "audio_engine.h"
#include <stdbool.h>

#include "dalby_tritone16b16k.h"
#include "please_mind_the_door.h"
#include "doors_opening.h"
#include "doors_closing.h"
#include "ground_floor.h"
#include "first_floor.h"
#include "second_floor.h"
#include "third_floor.h"
#include "top_floor.h"
#include "lift_out_of_service.h"

volatile OptionSelTypeDef option;

extern void     Enter_LP_SleepMode( void );
extern void     WaitForTrigger    ( uint8_t trig_to_wait_for );
extern void     SetSleepSetting   ( uint8_t setting );
extern uint8_t  GetTriggerOption  ( void );
extern void     delay_ms          ( uint32_t millis );

OptionSelTypeDef GetOption( void );

/** Main loop for the dalby application
  *
  * @param: none
  * @retval: none.  Does not return BTW.
  */
void ChimeLoop( void )
{    
  uint8_t trigger_option = GetTriggerOption();
 
  SetDAC_Control( 1 );
  SetLpf16BitLevel( LPF_Off );
  SetSoftClippingEnable( 1 );
  option = GetOption();

  while( true )
  {

    if( option != OPT_DoorsOpeningClosing ) {
      if( trigger_option == 1 ) { WaitForTrigger( TRIGGER_SET ); }
    }
 
    switch( option ) {
    case OPT_Chime:         // Value: 0
    default:                // ...or default
      SetSleepSetting( 1 );
      SetFadeInTime( 0.2f );
      SetFadeOutTime( 0.2f );
      SetDAC_Control( 1 );
      
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
        AudioEngine_DACSwitch( 1 );
        SetFadeInTime( 0.1f );
        SetFadeOutTime( 0.2f );
        SetDAC_Control( 1 );

        PlaySample( pmtd16k16b1c, PMTD16K16B1C_SZ, I2S_AUDIOSAMPLE_16K, 16, PMTD16K16B1C_PB_FMT );
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
      AudioEngine_DACSwitch( 1 );
      SetSleepSetting( 0 );
      SetFadeInTime( 0.01f );
      SetFadeOutTime( 0.01f );

      // Endless loop of doors opening/closing.
      while( true ) {
        PlaySample( do16k16b1c, DO16K16B1C_SZ, I2S_AUDIOSAMPLE_16K, 16, DO16K16B1C_PB_FMT );
        WaitForTrigger( TRIGGER_CLR );
        StopPlayback();
        if( option != OPT_DoorsOpeningClosing ) {
          break;
        }
        PlaySample( dc16k16b1c, DC16K16B1C_SZ, I2S_AUDIOSAMPLE_16K, 16, DC16K16B1C_PB_FMT );
        WaitForTrigger( TRIGGER_SET );
        StopPlayback();
        if( option != OPT_DoorsOpeningClosing ) {
          break;
        }
      }      
      break;

      case OPT_GroundFloor:   // Number 8
        SetSleepSetting( 1 );
        AudioEngine_DACSwitch( 1 );
        SetFadeInTime( 0.2f );
        SetFadeOutTime( 0.2f );
        SetDAC_Control( 1 );

        PlaySample( ground_floor16k16b1c, GROUND_FLOOR16K16B1C_SZ, I2S_AUDIOSAMPLE_16K, 16, GROUND_FLOOR16K16B1C_PB_FMT );
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

      case OPT_FirstFloor:    // Number 9
        SetSleepSetting( 1 );
        AudioEngine_DACSwitch( 1 );
        SetFadeInTime( 0.2f );
        SetFadeOutTime( 0.2f );
        SetDAC_Control( 1 );

        PlaySample( first_floor16k16b1c, FIRST_FLOOR16K16B1C_SZ, I2S_AUDIOSAMPLE_16K, 16, FIRST_FLOOR16K16B1C_PB_FMT );
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

      case OPT_SecondFloor:   // Number 10
        SetSleepSetting( 1 );
        AudioEngine_DACSwitch( 1 );
        SetFadeInTime( 0.2f );
        SetFadeOutTime( 0.2f );
        SetDAC_Control( 1 );

        PlaySample( second_floor16k16b1c, SECOND_FLOOR16K16B1C_SZ, I2S_AUDIOSAMPLE_16K, 16, SECOND_FLOOR16K16B1C_PB_FMT );
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

      case OPT_ThirdFloor:    // Number 11
        SetSleepSetting( 1 );
        AudioEngine_DACSwitch( 1 );
        SetFadeInTime( 0.2f );
        SetFadeOutTime( 0.2f );
        SetDAC_Control( 1 );

        PlaySample( third_floor16k16b1c, THIRD_FLOOR16K16B1C_SZ, I2S_AUDIOSAMPLE_16K, 16, THIRD_FLOOR16K16B1C_PB_FMT );
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

      case OPT_TopFloor:    // Number 12
        SetSleepSetting( 1 );
        AudioEngine_DACSwitch( 1 );
        SetFadeInTime( 0.01f );
        SetFadeOutTime( 0.1f );
        SetDAC_Control( 1 );

        PlaySample( top_floor16k16b1c, TOP_FLOOR16K16B1C_SZ, I2S_AUDIOSAMPLE_16K, 16, TOP_FLOOR16K16B1C_PB_FMT );
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

      case OPT_LiftOutOfService:    // Number 15
        SetSleepSetting( 1 );
        AudioEngine_DACSwitch( 1 );
        SetFadeInTime( 0.2f );
        SetFadeOutTime( 0.2f );
        SetDAC_Control( 1 );

        PlaySample( lift_oos16k16b1c, LIFT_OOS16K16B1C_SZ, I2S_AUDIOSAMPLE_16K, 16, LIFT_OOS16K16B1C_PB_FMT );
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
    }  // End option switch
  }
}

OptionSelTypeDef GetOption( void )
{
  return  (
            ( gpio_input_bit_get( OPT4_Bank, OPT4_Pin ) << 3 )  |
            ( gpio_input_bit_get( OPT3_Bank, OPT3_Pin ) << 2 )  |
            ( gpio_input_bit_get( OPT2_Bank, OPT2_Pin ) << 1 )  |
            ( gpio_input_bit_get( OPT1_Bank, OPT1_Pin )      )
          );
}
