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


/** @tagSettings DEFAULT_ARCHITECTURE=ZYNQ */
/** @reqSettings DEFAULT_SPECIFICATION_REVISION=4.1.2 */

/* @req SWS_Gpt_00293 */
#include "Gpt.h"
#include "zynq.h"
#include "Mcu.h"
#if defined(USE_DET)
#include "Det.h"
#endif
#if !defined(CFG_DRIVERS_USE_CONFIG_ISRS)
#include "isr.h"
#include "irq_zynq.h"
#endif
#include "Os.h"

// General requirements implemented/inspected
/* @req SWS_Gpt_00352  */
/* @req SWS_Gpt_00353  */
/* @req SWS_Gpt_00354  */
/* @req SWS_Gpt_00355  */
/* @req SWS_Gpt_00356  */
/* @req SWS_Gpt_00258  */
/* @req SWS_Gpt_00113  */
/* @req SWS_Gpt_00114  */
/* @req SWS_Gpt_00115  */
/* @req SWS_Gpt_00116  */
/* @req SWS_Gpt_00117  */
/* @req SWS_Gpt_00118  */
/* @req SWS_Gpt_00155  */
/* @req SWS_Gpt_00156  */
/* @req SWS_Gpt_00360  */
/* @req SWS_Gpt_00359  */

/* @req SWS_Gpt_00375 */
#if defined(USE_DET) && (GPT_DEV_ERROR_DETECT == STD_ON)
#include "Det.h"
#endif

/* ----------------------------[private macro]-------------------------------*/

#if ( GPT_DEV_ERROR_DETECT == STD_ON )
#define VALIDATE(_exp,_api,_err ) \
        if( !(_exp) ) { \
        (void)  Det_ReportError(GPT_MODULE_ID,0,_api,_err); \
          return; \
        }

#define VALIDATE_W_RV(_exp,_api,_err,_rv ) \
        if( !(_exp) ) { \
          (void)Det_ReportError(GPT_MODULE_ID,0,_api,_err); \
          return (_rv); \
        }

#define VALID_CHANNEL(_ch)      ( Gpt_Global.configured & (1<<(_ch)) )

#else
#define VALIDATE(_exp,_api,_err )
#define VALIDATE_W_RV(_exp,_api,_err,_rv )
#endif

#define GPT_MAXIMUM_CHANNELS        6
#define GET_TTC_MODULE(_c)          (_c/3) ?&TTC1_REG : &TTC0_REG
#define GET_COUNTER(_c)             (_c%3)
#define IS_CHANNEL_VALID(_c)        (GPT_CHANNELS_CONFIGURED & (1 << _c)) && (_c < GPT_MAXIMUM_CHANNELS)
#define IS_TARGET_VALID(_c)      /*lint -save -e685 */  (_c != 0) && (_c <= 65535) /*lint -restore */
#define TIMER_REGISTER_VALUE(_reg, _cnt)   *((uint32 *)(&(_reg)+ _cnt))


#define PRESCALER_ENABLE            1
#define PRESCALER_VAL_START_BIT     1
#define DISABLE_COUNTER             1
#define OUTPUT_WAVE_DISABLE         (1 << 5)
#define ENABLE_RESET_COUNTER        (1 << 4)
#define ENABLE_COUNTER              (0xFE)
#define MATCH_ENABLE                (1 << 3)
#define INTERVAL_ENABLE             (1<<1)
#define MATCH1_INTERRUPT_ENABLE     (1 << 1)
#define MATCH_INTERRUPT_DISABLE     0
#define DEFAULT_RETURN_VALUE        0
#define GPT_CHANNEL_0               0
#define GPT_CHANNEL_1               1
#define GPT_CHANNEL_2               2
#define GPT_CHANNEL_3               3
#define GPT_CHANNEL_4               4
#define GPT_CHANNEL_5               5
#define GPT_ISR_PRIORITY            4

typedef enum {
    GPT_STATE_STOPPED = 0, GPT_STATE_RUNNING, GPT_STATE_EXPIRED, GPT_STATE_INITIALIZED
} Gpt_ChannelStateType;

/**
 * Type that holds all global data for Gpt
 */
typedef struct {
    // Set if Gpt_Init() have been called
    boolean initRun;

    //Gpt driver mode
    Gpt_ModeType mode;
    // Our config
    const Gpt_ConfigType *config;

#if ( GPT_WAKEUP_FUNCTIONALITY_API == STD_ON )
    uint8 wakeupEnabled;
#endif

    // Maps the a physical channel id to configured channel index
    uint8                 channelMap[GPT_MAXIMUM_CHANNELS];

    Gpt_ChannelStateType Gpt_ChannelState[GPT_CHANNEL_CNT]; //States for each channel

    Gpt_ValueType        Gpt_ChannelTargetValue[GPT_CHANNEL_CNT]; //Match value for each channel
    boolean              Gpt_NotifEnable[GPT_CHANNEL_CNT];
    boolean              Gpt_WakUpEnable[GPT_CHANNEL_CNT];

} Gpt_GlobalType;

// Global config
Gpt_GlobalType Gpt_Global = {
        .initRun = STD_OFF,
        .mode = GPT_MODE_UNINTIALIZED,
        .config = NULL,
};


#if (GPT_VERSION_INFO_API == STD_ON)
static Std_VersionInfoType _Gpt_VersionInfo =
{       .vendorID = (uint16)GPT_VENDOR_ID,
        .moduleID = (uint16) GPT_MODULE_ID,
        .sw_major_version = (uint8)GPT_SW_MAJOR_VERSION,
        .sw_minor_version = (uint8)GPT_SW_MINOR_VERSION,
        .sw_patch_version = (uint8)GPT_SW_PATCH_VERSION
};
/* @req SWS_Gpt_00279 */
void Gpt_GetVersionInfo( Std_VersionInfoType* VersionInfoPtr ) {

    /* @req SWS_Gpt_00338 **/
    VALIDATE( (VersionInfoPtr != NULL), GPT_GETVERSIONINFO_SERVICE_ID, GPT_E_PARAM_POINTER );

    memcpy(VersionInfoPtr, &_Gpt_VersionInfo, sizeof(Std_VersionInfoType));
    return;

}
#endif


void Gpt_Isr(uint8 Chnl) {

    uint8 chnlidx = Gpt_Global.channelMap[Chnl];
    struct TTC_reg * reg;
    uint8 offset;
    uint32 intSts,readVal;

    reg =  GET_TTC_MODULE(Chnl);
    offset = GET_COUNTER(Chnl);

    intSts = (uint32)TIMER_REGISTER_VALUE(reg->INTERRUPT_REGISTER_1, offset); //Clear any pending interrupts on read
    (void)intSts;

    /* Oneshot mode */
    if (GPT_CH_MODE_ONESHOT == Gpt_Global.config[chnlidx].GptChannelMode) {

        //Disable timer
        readVal = TIMER_REGISTER_VALUE(reg->COUNTER_CONTROL_1 , offset)  | DISABLE_COUNTER;
        TIMER_REGISTER_VALUE(reg->COUNTER_CONTROL_1 , offset)  = readVal; //Disable counter
        Gpt_Global.Gpt_ChannelState[chnlidx] = GPT_STATE_EXPIRED;

#if (GPT_REPORT_WAKEUP_SOURCE == STD_ON)
        /* Report wake up to ECUM */
        if (Gpt_Global.Gpt_WakUpEnable[chnlidx]) {
            EcuM_CheckWakeup(Gpt_Global.config[chnlidx].GptWakeupSource);
        }
#endif

    }
    //Notify
    if (Gpt_Global.Gpt_NotifEnable[chnlidx]) {
        Gpt_Global.config[chnlidx].GptNotification();
    }

}

ISR(Gpt_0_Isr) {
    Gpt_Isr(GPT_CHANNEL_0);
}
ISR(Gpt_1_Isr) {
    Gpt_Isr(GPT_CHANNEL_1);
}
ISR(Gpt_2_Isr) {
    Gpt_Isr(GPT_CHANNEL_2);
}
ISR(Gpt_3_Isr) {
    Gpt_Isr(GPT_CHANNEL_3);
}
ISR(Gpt_4_Isr) {
    Gpt_Isr(GPT_CHANNEL_4);
}
ISR(Gpt_5_Isr) {
    Gpt_Isr(GPT_CHANNEL_5);
}


#if !defined(CFG_DRIVERS_USE_CONFIG_ISRS)
static void installChannelInterrupt(uint8 Chnl)
{
    switch (Chnl) {
        case GPT_CHANNEL_0:
            /* Install ISR. IRQ ID#42 */
            ISR_INSTALL_ISR2("Gpt", Gpt_0_Isr, (IrqType)(IRQ_TTC_0_0),GPT_ISR_PRIORITY, 0);
            break;

        case GPT_CHANNEL_1:
            /* Install ISR. IRQ ID#43 */
            ISR_INSTALL_ISR2("Gpt", Gpt_1_Isr, (IrqType)(IRQ_TTC_0_1),GPT_ISR_PRIORITY, 0);
            break;

        case GPT_CHANNEL_2:
            /* Install ISR. IRQ ID#44 */
            ISR_INSTALL_ISR2("Gpt", Gpt_2_Isr, (IrqType)(IRQ_TTC_0_2),GPT_ISR_PRIORITY, 0);
            break;

        case GPT_CHANNEL_3:
            /* Install ISR. IRQ ID#69 */
            ISR_INSTALL_ISR2("Gpt", Gpt_3_Isr, (IrqType)(IRQ_TTC_1_0),GPT_ISR_PRIORITY, 0);
            break;

        case GPT_CHANNEL_4:
            /* Install ISR. IRQ ID#70 */
            ISR_INSTALL_ISR2("Gpt", Gpt_4_Isr, (IrqType)(IRQ_TTC_1_1),GPT_ISR_PRIORITY, 0);
            break;

        case GPT_CHANNEL_5:
            /* Install ISR. IRQ ID#71 */
            ISR_INSTALL_ISR2("Gpt", Gpt_5_Isr, (IrqType)(IRQ_TTC_1_2),GPT_ISR_PRIORITY, 0);
            break;
    }
}
#endif

/* @req SWS_Gpt_00280 */
void Gpt_Init( const Gpt_ConfigType* ConfigPtr ) {

    struct TTC_reg * reg;
    uint8 offset, intSts,phyChnl;
    uint32 readVal;

    /* @req SWS_Gpt_00307 */
    VALIDATE ((STD_OFF == Gpt_Global.initRun), GPT_INIT_SERVICE_ID, GPT_E_ALREADY_INITIALIZED);
    /* @req SWS_Gpt_00294 */
    VALIDATE((ConfigPtr != NULL), GPT_INIT_SERVICE_ID, GPT_E_PARAM_POINTER);

    /* @req SWS_Gpt_00006 */
    Gpt_Global.config = ConfigPtr;

    for (uint8 chnlcnt = 0; chnlcnt < GPT_CHANNEL_CNT; chnlcnt++) {
        /* @req SWS_Gpt_00068 */
        phyChnl = Gpt_Global.config[chnlcnt].GptChannelId;
        reg =  GET_TTC_MODULE(phyChnl);
        offset = GET_COUNTER(phyChnl);

        readVal = TIMER_REGISTER_VALUE(reg->COUNTER_CONTROL_1 , offset) | DISABLE_COUNTER; //Disable counter
        TIMER_REGISTER_VALUE(reg->COUNTER_CONTROL_1 , offset)      = readVal;
        TIMER_REGISTER_VALUE(reg->CLOCK_CONTROL_1 , offset)        = ((Gpt_Global.config[chnlcnt].GptChannelPrescale << PRESCALER_VAL_START_BIT)|PRESCALER_ENABLE); //Set prescaler and select default clock
        intSts = TIMER_REGISTER_VALUE(reg->INTERRUPT_REGISTER_1, offset); //Clear any pending interrupts on read

        (void)intSts;
        TIMER_REGISTER_VALUE(reg->INTERRUPT_ENABLE_1 , offset)     = MATCH_INTERRUPT_DISABLE; //Disable interrupts

        Gpt_Global.channelMap[phyChnl] =  chnlcnt; //Mapping from Physical channel to channel index in config
        Gpt_Global.Gpt_ChannelState[chnlcnt] = GPT_STATE_INITIALIZED;
        Gpt_Global.Gpt_ChannelTargetValue[chnlcnt] = 0;
        /* @req SWS_Gpt_00107 */
        Gpt_Global.Gpt_NotifEnable[chnlcnt] = FALSE;
        /* @req SWS_Gpt_00258 */
        Gpt_Global.Gpt_WakUpEnable[chnlcnt] = FALSE;

#if !defined(CFG_DRIVERS_USE_CONFIG_ISRS)
        installChannelInterrupt(phyChnl);
#endif

    }

    /* @req SWS_Gpt_00339 */
    Gpt_Global.mode = GPT_MODE_NORMAL;
    Gpt_Global.initRun = STD_ON;
}

/* @req SWS_Gpt_00194 */
#if GPT_DEINIT_API == STD_ON
/* @req SWS_Gpt_00281 */
void Gpt_DeInit( void ) {

    struct TTC_reg * reg;
    uint8 offset, intSts;
    uint32 readVal;

    /* @req SWS_Gpt_00220 */
    VALIDATE ((STD_ON == Gpt_Global.initRun), GPT_DEINIT_SERVICE_ID, GPT_E_UNINIT);

    /* @req SWS_Gpt_00008 */
    for (uint8 chnlcnt = 0; chnlcnt < GPT_CHANNEL_CNT; chnlcnt++) {
        /* @req SWS_Gpt_00068 */
        reg    = GET_TTC_MODULE(Gpt_Global.config[chnlcnt].GptChannelId);
        offset = GET_COUNTER(Gpt_Global.config[chnlcnt].GptChannelId);

        /* @req SWS_Gpt_00234 */
        VALIDATE(( GPT_STATE_RUNNING != Gpt_Global.Gpt_ChannelState[chnlcnt]),GPT_DEINIT_SERVICE_ID,GPT_E_BUSY);

        /* @req SWS_Gpt_00162 */
        readVal = TIMER_REGISTER_VALUE(reg->COUNTER_CONTROL_1, offset) | DISABLE_COUNTER; //Disable counter
        TIMER_REGISTER_VALUE(reg->CLOCK_CONTROL_1, offset)   = readVal;
        TIMER_REGISTER_VALUE(reg->MATCH_1_COUNTER_1, offset) = 0;
        TIMER_REGISTER_VALUE(reg->INTERVAL_COUNTER_1, offset)= 0;
        TIMER_REGISTER_VALUE(reg->INTERRUPT_ENABLE_1, offset)= MATCH_INTERRUPT_DISABLE;
        intSts = TIMER_REGISTER_VALUE(reg->INTERRUPT_REGISTER_1, offset); //Clear any pending interrupts on read
        (void)intSts;
    }

    Gpt_Global.mode = GPT_MODE_UNINTIALIZED;
    /* @req SWS_Gpt_00363 */
    Gpt_Global.initRun = STD_OFF;
}
#endif

/* @req SWS_Gpt_00195 */
#if ( GPT_TIME_ELAPSED_API == STD_ON )
/* @req SWS_Gpt_00282 */
Gpt_ValueType Gpt_GetTimeElapsed( Gpt_ChannelType Channel ) {

    struct TTC_reg *reg;
    uint8 offset, chnlidx;
    Gpt_ValueType retVal = 0;

    /* @req SWS_Gpt_00222 */
    VALIDATE_W_RV((STD_ON == Gpt_Global.initRun), GPT_GETTIMEELAPSED_SERVICE_ID, GPT_E_UNINIT, DEFAULT_RETURN_VALUE);

    /* @req SWS_Gpt_00210 */
    VALIDATE_W_RV(IS_CHANNEL_VALID(Channel), GPT_GETTIMEELAPSED_SERVICE_ID, GPT_E_PARAM_CHANNEL, DEFAULT_RETURN_VALUE);

    reg     = GET_TTC_MODULE(Channel);
    offset  = GET_COUNTER(Channel);
    chnlidx = Gpt_Global.channelMap[Channel];


    if (GPT_STATE_INITIALIZED == Gpt_Global.Gpt_ChannelState[chnlidx])
    {
        /* @req SWS_Gpt_00295 */
        return DEFAULT_RETURN_VALUE;
    }

    /* @req SWS_Gpt_00299 */ // Expired - Counter Freeze at target
    if ((GPT_CH_MODE_ONESHOT == Gpt_Global.config[chnlidx].GptChannelMode)  &&
            (GPT_STATE_EXPIRED == Gpt_Global.Gpt_ChannelState[chnlidx])){
        return Gpt_Global.Gpt_ChannelTargetValue[chnlidx];
    }

    /* @req SWS_Gpt_00010 */ // Running
    /* @req SWS_Gpt_00297 */ // Stopped
    /* @req SWS_Gpt_00361 */ // Continuous mode
    /*lint -save -e662 -e661 */
    retVal = (Gpt_ValueType)TIMER_REGISTER_VALUE(reg->COUNTER_VALUE_1, offset);
    /*lint -restore */
    return retVal;

}

#endif

/* @req SWS_Gpt_00196 */
#if ( GPT_TIME_REMAINING_API == STD_ON )
/* @req SWS_Gpt_00283 */
Gpt_ValueType Gpt_GetTimeRemaining( Gpt_ChannelType Channel ) {

    struct TTC_reg * reg;
    uint8 offset, chnlidx;
    Gpt_ValueType retVal = 0;

    /* @req SWS_Gpt_00223 */
    VALIDATE_W_RV((STD_ON == Gpt_Global.initRun), GPT_GETTIMEREMAINING_SERVICE_ID, GPT_E_UNINIT, DEFAULT_RETURN_VALUE);

    /* @req SWS_Gpt_00211 */
    VALIDATE_W_RV(IS_CHANNEL_VALID(Channel), GPT_GETTIMEREMAINING_SERVICE_ID, GPT_E_PARAM_CHANNEL, DEFAULT_RETURN_VALUE);

    reg     = GET_TTC_MODULE(Channel);
    offset  = GET_COUNTER(Channel);
    chnlidx = Gpt_Global.channelMap[Channel];

    if (GPT_STATE_INITIALIZED == Gpt_Global.Gpt_ChannelState[chnlidx])
    {
        /* @req SWS_Gpt_00301 */
        return DEFAULT_RETURN_VALUE;
    }

    /* @req SWS_Gpt_00305 */ // Expired - Counter Freeze at target
    if ((GPT_CH_MODE_ONESHOT == Gpt_Global.config[chnlidx].GptChannelMode)  &&
            (GPT_STATE_EXPIRED == Gpt_Global.Gpt_ChannelState[chnlidx])){
        return DEFAULT_RETURN_VALUE;
    }

    /* @req SWS_Gpt_00083 */ // Running
    /* @req SWS_Gpt_00303 */ // Stopped
    /*lint -save -e662 -e661 */
    retVal = (Gpt_ValueType)(Gpt_Global.Gpt_ChannelTargetValue[chnlidx] - (uint32)TIMER_REGISTER_VALUE(reg->COUNTER_VALUE_1, offset));
    /*lint -restore */
    return retVal;

}
#endif

/* @req SWS_Gpt_00284 */
void Gpt_StartTimer( Gpt_ChannelType Channel, Gpt_ValueType Value ) {

    struct TTC_reg *reg;
    uint8 offset, chnlidx;
    uint32 readVal;

    /* @req SWS_Gpt_00224 */
    VALIDATE((STD_ON == Gpt_Global.initRun), GPT_STARTTIMER_SERVICE_ID, GPT_E_UNINIT);

    /* @req SWS_Gpt_00212 */
    VALIDATE(IS_CHANNEL_VALID(Channel), GPT_STARTTIMER_SERVICE_ID, GPT_E_PARAM_CHANNEL);

    /* @req SWS_Gpt_00218 */
    VALIDATE(IS_TARGET_VALID(Value), GPT_STARTTIMER_SERVICE_ID, GPT_E_PARAM_VALUE);

    reg     = GET_TTC_MODULE(Channel);
    offset  = GET_COUNTER(Channel);
    chnlidx = Gpt_Global.channelMap[Channel];

    /* @req SWS_Gpt_00084 */
    VALIDATE((GPT_STATE_RUNNING != Gpt_Global.Gpt_ChannelState[chnlidx]), GPT_STARTTIMER_SERVICE_ID, GPT_E_BUSY);

    /*  @req SWS_Gpt_00274 */
    //Lint warnings disable pointer would not result in out-of-bound pointer. Verified the maximum possible value for offset (2).
    /*lint -save -e662 -e661 */
    TIMER_REGISTER_VALUE(reg->MATCH_1_COUNTER_1, offset)  = Value; //Enable match
    TIMER_REGISTER_VALUE(reg->INTERVAL_COUNTER_1, offset) = Value; //Set interval
    TIMER_REGISTER_VALUE(reg->INTERRUPT_ENABLE_1, offset) = MATCH1_INTERRUPT_ENABLE; //Enable interrupts

    readVal = TIMER_REGISTER_VALUE(reg->COUNTER_CONTROL_1 , offset)|INTERVAL_ENABLE | MATCH_ENABLE | ENABLE_RESET_COUNTER;//Enable interval mode, match and restart timer
    TIMER_REGISTER_VALUE(reg->COUNTER_CONTROL_1 , offset) = readVal & ENABLE_COUNTER;//Enable counter

    Gpt_Global.Gpt_ChannelTargetValue[chnlidx] = Value;
    /*lint -restore */
    /* @req SWS_Gpt_00364 */
    Gpt_Global.Gpt_ChannelState[chnlidx] = GPT_STATE_RUNNING;
}

/* @req SWS_Gpt_00285 */
void Gpt_StopTimer( Gpt_ChannelType Channel ) {

    struct TTC_reg * reg;
    uint8 offset, chnlidx,intSts;
    uint32 readVal;


    /* @req SWS_Gpt_00225 */
    VALIDATE((STD_ON == Gpt_Global.initRun), GPT_STOPTIMER_SERVICE_ID, GPT_E_UNINIT);

    /* @req SWS_Gpt_00213 */
    VALIDATE(IS_CHANNEL_VALID(Channel), GPT_STOPTIMER_SERVICE_ID, GPT_E_PARAM_CHANNEL);


    reg     = GET_TTC_MODULE(Channel);
    offset  = GET_COUNTER(Channel);
    chnlidx = Gpt_Global.channelMap[Channel];

    if (GPT_STATE_RUNNING != Gpt_Global.Gpt_ChannelState[chnlidx]) {
        /* @req SWS_Gpt_00099 */
        /* @req SWS_Gpt_00344 */
        return;
    }

    //Lint warnings disable pointer would not result in out-of-bound pointer. Verified the maximum possible value for offset (2).
    /*lint -save -e662 -e661 */
    TIMER_REGISTER_VALUE(reg->INTERRUPT_ENABLE_1, offset) = MATCH_INTERRUPT_DISABLE; //Disable interrupts
    intSts = TIMER_REGISTER_VALUE(reg->INTERRUPT_REGISTER_1, offset); //Clear any pending interrupts on read
    (void)intSts;
    /* @req SWS_Gpt_00013 */
    readVal =  TIMER_REGISTER_VALUE(reg->COUNTER_CONTROL_1 , offset) | DISABLE_COUNTER;
    TIMER_REGISTER_VALUE(reg->COUNTER_CONTROL_1 , offset) = readVal; //Disable counter
    /*lint -restore */
    /* @req SWS_Gpt_00343 */
    Gpt_Global.Gpt_ChannelState[chnlidx] = GPT_STATE_STOPPED;

}

/* @req SWS_Gpt_00199 */
/* @req SWS_Gpt_00200 */
#if ( GPT_ENABLE_DISABLE_NOTIFICATION_API == STD_ON )
/* @req SWS_Gpt_00286 */
void Gpt_EnableNotification( Gpt_ChannelType Channel ) {

    uint8 chnlidx;

    /* @req SWS_Gpt_00226 */
    VALIDATE((STD_ON == Gpt_Global.initRun), GPT_ENABLENOTIFICATION_SERVICE_ID, GPT_E_UNINIT);

    /* @req SWS_Gpt_00214 */
    VALIDATE(IS_CHANNEL_VALID(Channel), GPT_ENABLENOTIFICATION_SERVICE_ID, GPT_E_PARAM_CHANNEL);

    chnlidx = Gpt_Global.channelMap[Channel];

    /* @req SWS_Gpt_00377 */
    VALIDATE((NULL != Gpt_Global.config[chnlidx].GptNotification), GPT_ENABLENOTIFICATION_SERVICE_ID, GPT_E_PARAM_CHANNEL);

    /* @req SWS_Gpt_00014 */
    /*  @req SWS_Gpt_00275 */
    Gpt_Global.Gpt_NotifEnable[chnlidx] = TRUE;

}

/* @req SWS_Gpt_00287 */
void Gpt_DisableNotification( Gpt_ChannelType Channel ) {

    uint8 chnlidx;

    /* @req SWS_Gpt_00227 */
    VALIDATE((STD_ON == Gpt_Global.initRun), GPT_DISABLENOTIFICATION_SERVICE_ID, GPT_E_UNINIT);

    /* @req SWS_Gpt_00217 */
    VALIDATE(IS_CHANNEL_VALID(Channel), GPT_DISABLENOTIFICATION_SERVICE_ID, GPT_E_PARAM_CHANNEL);

    chnlidx = Gpt_Global.channelMap[Channel];

    /* @req SWS_Gpt_00379 */
    VALIDATE((NULL != Gpt_Global.config[chnlidx].GptNotification), GPT_DISABLENOTIFICATION_SERVICE_ID, GPT_E_PARAM_CHANNEL);

    /* @req SWS_Gpt_00015 */
    Gpt_Global.Gpt_NotifEnable[chnlidx] = FALSE;

}
#endif


/* @req SWS_Gpt_00201 */
/* @req SWS_Gpt_00202 */
/* @req SWS_Gpt_00203 */
/* @req SWS_Gpt_00324 */
#if ( GPT_WAKEUP_FUNCTIONALITY_API == STD_ON )


/* @req SWS_Gpt_00157 */
/* @req SWS_Gpt_00255 */
/* @req SWS_Gpt_00158 */
#if (GPT_REPORT_WAKEUP_SOURCE == STD_ON)



/* @req SWS_Gpt_00288 */
void Gpt_SetMode( Gpt_ModeType Mode ) {

    /* @req SWS_Gpt_00228 */
    VALIDATE((STD_ON == Gpt_Global.initRun), GPT_SETMODE_SERVICE_ID, GPT_E_UNINIT);

    /* @req SWS_Gpt_00231 */
    VALIDATE(((GPT_MODE_SLEEP == Mode) || (GPT_MODE_NORMAL == Mode)), GPT_SETMODE_SERVICE_ID, GPT_E_PARAM_MODE);

    /* @req SWS_Gpt_00164 */
    if (GPT_MODE_SLEEP == Mode) {
        for (uint8 chnlCnt = 0; chnlCnt < GPT_CHANNEL_CNT; chnlCnt++) {
            if ((!Gpt_Global.Gpt_WakUpEnable[chnlCnt]) && (GPT_STATE_RUNNING == Gpt_Global.Gpt_ChannelState[chnlCnt])) {
                Gpt_StopTimer(Gpt_Global.config[chnlCnt].GptChannelId); //Stop channels not configured/Enabled for wakeup
            }
            /* @req SWS_Gpt_00341 */
            //No Wakeup timer started
        }
    } else {
        /* @req SWS_Gpt_00165 */
        //Other timers are not started in Normal mode
    }

    /* @req SWS_Gpt_00151 */
    Gpt_Global.mode = Mode;
}


/* @req SWS_Gpt_00289 */
void Gpt_DisableWakeup( Gpt_ChannelType Channel ) {

    uint8 chnlidx;

    /* @req SWS_Gpt_00229 */
    VALIDATE((STD_ON == Gpt_Global.initRun), GPT_DISABLEWAKEUP_SERVICE_ID, GPT_E_UNINIT);

    /* @req SWS_Gpt_00215 */
    VALIDATE(IS_CHANNEL_VALID(Channel), GPT_DISABLEWAKEUP_SERVICE_ID, GPT_E_PARAM_CHANNEL);

    chnlidx = Gpt_Global.channelMap[Channel];

    /* @req SWS_Gpt_00215 */
    VALIDATE(Gpt_Global.config[chnlidx].GptEnableWakeup, GPT_DISABLEWAKEUP_SERVICE_ID, GPT_E_PARAM_CHANNEL);

    /* @req SWS_Gpt_00159 */
    Gpt_Global.Gpt_WakUpEnable[chnlidx] = FALSE;

}

/* @req SWS_Gpt_00290 */
void Gpt_EnableWakeup( Gpt_ChannelType Channel ) {

    uint8 chnlidx;

    /* @req SWS_Gpt_00230 */
    VALIDATE((STD_ON == Gpt_Global.initRun), GPT_ENABLEWAKEUP_SERVICE_ID, GPT_E_UNINIT);

    /* @req SWS_Gpt_00216 */
    VALIDATE(IS_CHANNEL_VALID(Channel), GPT_ENABLEWAKEUP_SERVICE_ID, GPT_E_PARAM_CHANNEL);

    chnlidx = Gpt_Global.channelMap[Channel];

    /* @req SWS_Gpt_00216 */
    VALIDATE(Gpt_Global.config[chnlidx].GptEnableWakeup, GPT_ENABLEWAKEUP_SERVICE_ID, GPT_E_PARAM_CHANNEL);

    /* @req SWS_Gpt_00160 */
    Gpt_Global.Gpt_WakUpEnable[chnlidx] = TRUE;

}

/* @req SWS_Gpt_00328 */
void Gpt_CheckWakeup( EcuM_WakeupSourceType WakeupSource ) {

    /* @req SWS_Gpt_00325 */
    VALIDATE((STD_ON == Gpt_Global.initRun), GPT_CHECKWAKEUP_SERVICE_ID, GPT_E_UNINIT);

    for (uint8 chnlCnt = 0; chnlCnt < GPT_CHANNEL_CNT; chnlCnt++) {
        // Loop over all channels to check if Wakeup source is found
        if (Gpt_Global.Gpt_WakUpEnable[chnlCnt] && (Gpt_Global.config[chnlCnt].GptWakeupSource == WakeupSource)
                && ( GPT_STATE_EXPIRED == Gpt_Global.Gpt_ChannelState[chnlCnt])) {
            EcuM_SetWakeupEvent(WakeupSource);
            break;
        }
    }

    return;
}
#endif
#endif
