#ifndef _MAIN_H
#define _MAIN_H

/** Pin definitions
  *
  */

  // Option Pins
  //
  #define OPT1_Pin            GPIO_PIN_7
  #define OPT1_Bank           GPIOB
  #define OPT2_Pin            GPIO_PIN_6
  #define OPT2_Bank           GPIOB
  #define OPT3_Pin            GPIO_PIN_5
  #define OPT3_Bank           GPIOB
  #define OPT4_Pin            GPIO_PIN_4
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

  // Defines for I2S
  //
  #define PROJECT_SPI         SPI1        // The port used on the PCB design, no remap is available.
  #define PROJECT_SPI_CLOCK   RCU_SPI1    // Clock for above peripheral.


// Trigger counter values
#define TC_LOW_THRESHOLD      120U
#define TC_HIGH_THRESHOLD     240U
#define TC_MAX                360U
#define TRIGGER_SET           1
#define TRIGGER_CLR           0
#define TRIG_TIMEOUT_MS       1000U

// Sleep mode control; debug doesn't work well if it's enabled. Comment out for sleep feature.
//#define NO_SLEEP_MODE

  // Includes
  //
  #include <gd32f30x.h>

#endif // _MAIN_H