#ifndef GD32_SDK_COMPAT_H
#define GD32_SDK_COMPAT_H

/* ADC API rename compatibility (new SDK uses "routine" naming). */
#ifndef ADC_REGULAR_CHANNEL
#define ADC_REGULAR_CHANNEL ADC_ROUTINE_CHANNEL
#endif

#ifndef ADC0_1_EXTTRIG_REGULAR_T2_TRGO
#define ADC0_1_EXTTRIG_REGULAR_T2_TRGO ADC0_1_EXTTRIG_ROUTINE_T2_TRGO
#endif

#ifndef adc_regular_channel_config
#define adc_regular_channel_config adc_routine_channel_config
#endif

#ifndef adc_regular_data_read
#define adc_regular_data_read adc_routine_data_read
#endif

/* Typo/rename compatibility in I2S standard selector. */
#ifndef I2S_STD_PHILLIPS
#define I2S_STD_PHILLIPS I2S_STD_PHILIPS
#endif

#endif
