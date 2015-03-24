/*-------------------------------- Arctic Core ------------------------------
 * Copyright (C) 2013, ArcCore AB, Sweden, www.arccore.com.
 * Contact: <contact@arccore.com>
 * 
 * You may ONLY use this file:
 * 1)if you have a valid commercial ArcCore license and then in accordance with  
 * the terms contained in the written license agreement between you and ArcCore, 
 * or alternatively
 * 2)if you follow the terms found in GNU General Public License version 2 as 
 * published by the Free Software Foundation and appearing in the file 
 * LICENSE.GPL included in the packaging of this file or here 
 * <http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt>
 *-------------------------------- Arctic Core -----------------------------*/

#ifndef ADC_CONFIGTYPES_H_
#define ADC_CONFIGTYPES_H_

typedef uint16_t Adc_ValueGroupType;
/* Group definitions. */

typedef enum
{
  ADC_CH0,
  ADC_CH1,
  ADC_CH2,
  ADC_CH3,
  ADC_CH4,
  ADC_CH5,
  ADC_CH6,
  ADC_CH7,
  ADC_CH8,
  ADC_CH9,
  ADC_CH10,
  ADC_CH11,
  ADC_CH12,
  ADC_CH13,
  ADC_CH14,
  ADC_CH15,
  ADC_CH16,
  ADC_CH17,
  ADC_CH18,
  ADC_CH19,
  ADC_CH20,
  ADC_CH21,
  ADC_CH22,
  ADC_CH23,
  ADC_CH24,
  ADC_NBR_OF_CHANNELS,
}Adc_ChannelType;

/* Std-type, supplier defined */
typedef enum
{
  ADC_SYSTEM_CLOCK
}Adc_ClockSourceType;


/* Std-type, supplier defined */
typedef enum
{
  ADC_SYSTEM_CLOCK_DISABLED,
  ADC_SYSTEM_CLOCK_DIVIDE_FACTOR_1,
  ADC_SYSTEM_CLOCK_DIVIDE_FACTOR_2,
  ADC_SYSTEM_CLOCK_DIVIDE_FACTOR_4,
  ADC_SYSTEM_CLOCK_DIVIDE_FACTOR_6,
  ADC_SYSTEM_CLOCK_DIVIDE_FACTOR_8,
  ADC_SYSTEM_CLOCK_DIVIDE_FACTOR_10,
  ADC_SYSTEM_CLOCK_DIVIDE_FACTOR_12,
  ADC_SYSTEM_CLOCK_DIVIDE_FACTOR_14,
  ADC_SYSTEM_CLOCK_DIVIDE_FACTOR_16,
  ADC_SYSTEM_CLOCK_DIVIDE_FACTOR_18,
  ADC_SYSTEM_CLOCK_DIVIDE_FACTOR_20,
  ADC_SYSTEM_CLOCK_DIVIDE_FACTOR_22,
  ADC_SYSTEM_CLOCK_DIVIDE_FACTOR_24,
  ADC_SYSTEM_CLOCK_DIVIDE_FACTOR_26,
  ADC_SYSTEM_CLOCK_DIVIDE_FACTOR_28,
  ADC_SYSTEM_CLOCK_DIVIDE_FACTOR_30,
  ADC_SYSTEM_CLOCK_DIVIDE_FACTOR_32,
}Adc_PrescaleType;

/* Std-type, supplier defined */
typedef uint16_t Adc_ConversionTimeType; //2 - 2048 range on TMS570LS1227

/* Non-standard type */
typedef struct
{
  Adc_ClockSourceType clockSource;
  uint8_t             hwUnitId;
  Adc_PrescaleType    adcPrescale;
  Adc_ConversionTimeType convTime;
}Adc_HWConfigurationType;



/* Channel definitions, std container */
typedef struct
{
  Adc_ConversionTimeType adcChannelConvTime;
  // NOT SUPPORTED Adc_VoltageSourceType  adcChannelRefVoltSrcLow;
  // NOT SUPPORTED Adc_VoltageSourceType  adcChannelRefVoltSrcHigh;
  // NOT SUPPORTED Adc_ResolutionType     adcChannelResolution;
  // NOT SUPPORTED Adc_CalibrationType    adcChannelCalibrationEnable;
} Adc_ChannelConfigurationType;

/* Used ?? */
typedef struct
{
  uint8 				notifictionEnable;
  Adc_ValueGroupType *	resultBufferPtr;
  Adc_StatusType 		groupStatus;
} Adc_GroupStatus;


/* Std-type, supplier defined */
typedef enum
{
  ADC_CONV_MODE_DISABLED,
  ADC_CONV_MODE_ONESHOT   = 1,
  ADC_CONV_MODE_CONTINUOUS = 9,
} Adc_GroupConvModeType;

/** Not supported. */
typedef uint16_t Adc_StreamNumSampleType;

/* Implementation specific */
typedef struct
{
  // NOT SUPPORTED  Adc_GroupAccessModeType      accessMode;
  Adc_GroupConvModeType        conversionMode;
  Adc_TriggerSourceType        triggerSrc;
  // NOT SUPPORTED  Adc_HwTriggerSignalType      hwTriggerSignal;
  // NOT SUPPORTED  Adc_HwTriggerTimerType       hwTriggerTimer;
  void                         (*groupCallback)(void);
  // NOT SUPPORTED  Adc_StreamBufferModeType     streamBufferMode;
  // NOT SUPPORTED  Adc_StreamNumSampleType      streamNumSamples;
  const Adc_ChannelType        *channelList;
  Adc_ValueGroupType           *resultBuffer;
  // NOT SUPPORTED  Adc_CommandType              *commandBuffer;
  Adc_ChannelType              numberOfChannels;
  Adc_GroupStatus              *status;
  // NOT SUPPORTED  Dma_ChannelType              dmaCommandChannel;
  // NOT SUPPORTED  Dma_ChannelType              dmaResultChannel;
  // NOT SUPPORTED  const struct tcd_t *		   groupDMACommands;
  // NOT SUPPORTED  const struct tcd_t *		   groupDMAResults;
  uint8_t 						hwUnit;
  uint16_t						groupId;
} Adc_GroupDefType;

/* Non-standard type */
typedef struct
{
  const Adc_HWConfigurationType*      hwConfigPtr;
  const Adc_ChannelConfigurationType* channelConfigPtr;
  const uint16_t                      nbrOfChannels;
  const Adc_GroupDefType*             groupConfigPtr;
  const uint16_t                      nbrOfGroups;
} Adc_ConfigType;

extern const Adc_ConfigType AdcConfig [];


#endif /* ADC_CONFIGTYPES_H_ */
