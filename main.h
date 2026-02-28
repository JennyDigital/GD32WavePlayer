#ifndef _MAIN_H
#define _MAIN_H

/** Pin definitions
  *
  */

  // Option Pins
  //
  #define OPT1_Pin            GPIO_PIN_4
  #define OPT1_Bank           GPIOB
  #define OPT2_Pin            GPIO_PIN_5
  #define OPT2_Bank           GPIOB
  #define OPT3_Pin            GPIO_PIN_6
  #define OPT3_Bank           GPIOB
  #define OPT4_Pin            GPIO_PIN_7
  #define OPT4_Bank           GPIOB

  // Trigger Pin
  //
  #define TRIGGER_Pin         GPIO_PIN_8
  #define TRIGGER_Bank        GPIOA

  // I2S Support Pins
  //
  #define nSD_MODE_Pin        GPIO_PIN_2
  #define nSD_MODE_BANK       GPIOB
  #define I2S_WS_Pin          GPIO_PIN_12
  #define I2S_WS_BANK         GPIOB
  #define I2S_CK_Pin          GPIO_PIN_13
  #define I2S_CK_BANK         GPIOB
  #define I2S_SD_Pin          GPIO_PIN_15
  #define I2S_SD_BANK         GPIOB

  // ADC Input Pin
  //
  #define VOLRef_Pin          GPIO_PIN_6
  #define VOLRef_BANK         GPIOA

  // Defines for I2S
  //
  #define PROJECT_SPI         SPI1        // The port used on the PCB design, no remap is available.
  #define PROJECT_SPI_CLOCK   RCU_SPI1    // Clock for above peripheral.


// Trigger counter values
#define TC_LOW_THRESHOLD      60U
#define TC_HIGH_THRESHOLD     120U
#define TC_MAX                240U
#define TRIGGER_SET           1
#define TRIGGER_CLR           0
#define TRIG_TIMEOUT_MS       1000U

// Analog volume ADC scaling (12-bit ADC scaled to 16-bit range)
// ADC max (4095) * 16 = 65520, representing the maximum usable 16-bit volume value
#define VOLUME_ADC_MAX_SCALED 65520U

// Scale factor for master volume input.
#define MASTER_VOLUME_SCALE 8U

// Volume floor value
#define MASTER_VOLUME_MINIMUM 8U

// Sleep mode control; debug doesn't work well if it's enabled. Comment out for sleep feature.
#define NO_SLEEP_MODE

//* Build options
//#define FORCE_TRIGGER_OPT
//#define TEST_CYCLING
//#define DALBY_BUILD

  // Includes
  //
  #include <gd32f30x.h>

#endif // _MAIN_H