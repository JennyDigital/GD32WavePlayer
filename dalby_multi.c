#include "dalby_multi.h"
#include "main.h"
#include "audio_engine.h"
#include <stdbool.h>

#include "dalby_tritone_low.h"
#include "please_mind_the_door.h"
#include "doors_opening.h"
#include "doors_closing.h"
#include "door_opening.h"
#include "door_closing.h"
#include "ground_floor.h"
#include "first_floor.h"
#include "second_floor.h"
#include "third_floor.h"
#include "top_floor.h"
#include "lift_out_of_service.h"

volatile OptionSelTypeDef option;

// External functions
extern void     Enter_LP_SleepMode( void );
extern void     WaitForTrigger    ( uint8_t trig_to_wait_for );
extern void     SetSleepSetting   ( uint8_t setting );
extern uint8_t  GetTriggerOption  ( void );
extern void     delay_ms          ( uint32_t millis );

// Internal helper Functions
OptionSelTypeDef  GetOption       ( void );
void              WaitForTrigCycle( void );

/** Main loop for the dalby application
  *
  * @param: none
  * @retval: none.  Does not return BTW.
  */
void ChimeLoop( void )
{    
  uint8_t trigger_option = 0;
 
  SetDAC_Control( 1 );
  SetLpf16BitLevel( LPF_Off );
  SetSoftClippingEnable( 1 );
  option = GetOption();

  while( true )
  {

    if( option != OPT_DoorsOpeningClosing &&
        option != OPT_DoorOpeningClosing ) {
      if( trigger_option == 1 ) { WaitForTrigger( TRIGGER_SET ); }
    }
 
    switch( option ) {

    
    case OPT_ChimeNoTrigger:    // Value 0
    default:                    // ...or default
      SetSleepSetting( 1 );
      SetFadeInTime( 0.05f );
      SetFadeOutTime( 0.05f );
      SetDAC_Control( 1 );
      SetFilterChain16BitEnable( 0 );
      SetLpf16BitLevel( LPF_Off );    
   
      PlaySample( dalby_tt_low44k16b1c, DALBY_TT_LOW44K16B1C_SZ, I2S_AUDIOSAMPLE_44K, 16, DALBY_TT_LOW44K16B1C_PB_FMT );
      WaitForSampleEnd();

      WaitForTrigCycle();
      option = GetOption();
      break;

    case OPT_Chime:             // Value: 1
      trigger_option = 1;
      SetSleepSetting( 1 );
      SetFadeInTime( 0.1f );
      SetFadeOutTime( 0.1f );
      SetDAC_Control( 1 );
      SetFilterChain16BitEnable( 0 );
 
      WaitForTrigger( TRIGGER_SET );
  
      PlaySample( dalby_tt_low44k16b1c, DALBY_TT_LOW44K16B1C_SZ, I2S_AUDIOSAMPLE_44K, 16, DALBY_TT_LOW44K16B1C_PB_FMT );
      WaitForSampleEnd();
      ShutDownAudio(); 
      WaitForTrigCycle();
      option = GetOption();
      break;

    case OPT_MindTheDoor:   // Value: 2
        SetSleepSetting( 1 );
        AudioEngine_DACSwitch( 1 );
        SetFadeInTime( 0.1f );
        SetFadeOutTime( 0.2f );
        SetDAC_Control( 1 );
        SetFilterChain16BitEnable( 1 );
        SetLpf16BitLevel( LPF_Off );

        PlaySample( pmtd16k16b1c, PMTD16K16B1C_SZ, I2S_AUDIOSAMPLE_16K, 16, PMTD16K16B1C_PB_FMT );
        WaitForSampleEnd();
        WaitForTrigCycle();;
        Enter_LP_SleepMode();
        option = GetOption();
        break;

    case OPT_DoorsOpeningClosing: // Value: 3
      // We'll keep the system live the whole time for this option.
      SetDAC_Control( 0 );
      AudioEngine_DACSwitch( 1 );
      SetSleepSetting( 0 );
      SetFadeInTime( 0.01f );
      SetFadeOutTime( 0.01f );
      SetFilterChain16BitEnable( 1 );
      SetLpf16BitLevel( LPF_Off );

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
      option = GetOption();     
      break;

    case OPT_DoorOpeningClosing: // Value: 4
      // We'll keep the system live the whole time for this option.
      SetDAC_Control( 0 );
      AudioEngine_DACSwitch( 1 );
      SetSleepSetting( 0 );
      SetFadeInTime( 0.01f );
      SetFadeOutTime( 0.01f );
      SetFilterChain16BitEnable( 1 );
      SetLpf16BitLevel( LPF_Off );

      // Endless loop of doors opening/closing.
      while( true ) {
        PlaySample( door_opening16k16b1c, DOOR_OPENING16K16B1C_SZ, I2S_AUDIOSAMPLE_16K, 16, DOOR_OPENING16K16B1C_PB_FMT );
        WaitForTrigger( TRIGGER_CLR );
        StopPlayback();
        if( option != OPT_DoorOpeningClosing ) {
          break;
        }
        PlaySample( door_closing16k16b1c, DOOR_CLOSING16K16B1C_SZ, I2S_AUDIOSAMPLE_16K, 16, DOOR_CLOSING16K16B1C_PB_FMT );
        WaitForTrigger( TRIGGER_SET );
        StopPlayback();
        if( option != OPT_DoorOpeningClosing ) {
          break;
        }
      }
      option = GetOption();      
      break;

      case OPT_GroundFloor:   // Number 8
        SetSleepSetting( 1 );
        SetFadeInTime( 0.01f );
        SetFadeOutTime( 0.165f );
        SetDAC_Control( 1 );
        SetFilterChain16BitEnable( 1 );
        SetLpf16BitLevel( LPF_Off );

        PlaySample( ground_floor16k16b1c, GROUND_FLOOR16K16B1C_SZ, I2S_AUDIOSAMPLE_16K, 16, GROUND_FLOOR16K16B1C_PB_FMT );
        WaitForSampleEnd();
        WaitForTrigCycle();
        option = GetOption();
        break;

      case OPT_FirstFloor:    // Number 9
        SetSleepSetting( 1 );
        SetFadeInTime( 0.2f );
        SetFadeOutTime( 0.2f );
        SetDAC_Control( 1 );
        SetFilterChain16BitEnable( 1 );
        SetLpf16BitLevel( LPF_Off );

        PlaySample( first_floor16k16b1c, FIRST_FLOOR16K16B1C_SZ, I2S_AUDIOSAMPLE_16K, 16, FIRST_FLOOR16K16B1C_PB_FMT );
        WaitForSampleEnd();
        WaitForTrigCycle();
        option = GetOption();
        break;

      case OPT_SecondFloor:   // Number 10
        SetSleepSetting( 1 );
        SetFadeInTime( 0.2f );
        SetFadeOutTime( 0.2f );
        SetDAC_Control( 1 );
        SetFilterChain16BitEnable( 1 );
        SetLpf16BitLevel( LPF_Off );

        PlaySample( second_floor16k16b1c, SECOND_FLOOR16K16B1C_SZ, I2S_AUDIOSAMPLE_16K, 16, SECOND_FLOOR16K16B1C_PB_FMT );
        WaitForSampleEnd();
        WaitForTrigCycle();
        Enter_LP_SleepMode();
        option = GetOption();
        break;

      case OPT_ThirdFloor:    // Number 11
        SetSleepSetting( 1 );
        SetFadeInTime( 0.2f );
        SetFadeOutTime( 0.2f );
        SetDAC_Control( 1 );
        SetFilterChain16BitEnable( 1 );
        SetLpf16BitLevel( LPF_Off );

        PlaySample( third_floor16k16b1c, THIRD_FLOOR16K16B1C_SZ, I2S_AUDIOSAMPLE_16K, 16, THIRD_FLOOR16K16B1C_PB_FMT );
        WaitForSampleEnd();
        WaitForTrigCycle();
        option = GetOption();
        break;

      case OPT_TopFloor:    // Number 12
        SetSleepSetting( 1 );
        SetFadeInTime( 0.01f );
        SetFadeOutTime( 0.1f );
        SetDAC_Control( 1 );
        SetFilterChain16BitEnable( 1 );
        SetLpf16BitLevel( LPF_Off );

        PlaySample( top_floor16k16b1c, TOP_FLOOR16K16B1C_SZ, I2S_AUDIOSAMPLE_16K, 16, TOP_FLOOR16K16B1C_PB_FMT );
        WaitForSampleEnd();
        WaitForTrigCycle();
        option = GetOption();
        break;

      case OPT_LiftOutOfService:    // Number 15
        SetSleepSetting( 1 );
        SetFadeInTime( 0.1f );
        SetFadeOutTime( 0.1f );
        SetDAC_Control( 1 );
        SetFilterChain16BitEnable( 1 );
        SetLpf16BitLevel( LPF_Off );

        PlaySample( lift_oos16k16b1c, LIFT_OOS16K16B1C_SZ, I2S_AUDIOSAMPLE_16K, 16, LIFT_OOS16K16B1C_PB_FMT );
        WaitForSampleEnd();
        WaitForTrigCycle();
        option = GetOption();
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


void WaitForTrigCycle( void )
{
  WaitForTrigger( TRIGGER_CLR );
  WaitForTrigger( TRIGGER_SET );
}